//
// Created by david on 11/11/2025.
//
#include "tcp_sim.h"
#include <tracy/Tracy.hpp>

void Simulator::run()
{
    ZoneScoped;
    while (!pq.empty())
    {
        auto e = pq.top();
        pq.pop();
        now = e.t;

        // No FrameMark needed here - let periodic checks handle frame marking
        TracyPlot("Simulation Time", now);

        e.fn();
    }
}

Time Link::xmit_delay(size_t bytes) const
{
    return (bytes * 8.0) / bandwidth_bps;
}

bool Link::lost() const
{
    std::uniform_real_distribution<double> U(0.0, 1.0);
    return U(rng) < loss_prob;
}

void Endpoint::start_client()
{
    // Send SYN
    send_segment(iss, 0, F_SYN);
    snd_nxt = iss + 1; // SYN consumes one sequence
    arm_timer();
}

void Endpoint::on_segment(const Segment &seg)
{
    ZoneScoped;
    ZoneText(name.c_str(), name.size());

    // Basic receiver behavior
    if (has(seg.flags, F_SYN))
    {
        // Passive open: reply SYN-ACK
        rcv_nxt = seg.seq + 1;
        Segment out;
        out.flags = (Flags) (F_SYN | F_ACK);
        out.seq = 5000; // B's ISN
        out.ack = rcv_nxt;
        out.len = 0;
        conn->deliver(*this, *(this == &(conn->A) ? &(conn->B) : &(conn->A)), out);
        return;
    }
    if (has(seg.flags, F_ACK) && !established)
    {
        // Handshake complete when ACKs our SYN-ACK or client gets SYN-ACK
        established = true;
        if (name == "A")
        {
            // A received SYN-ACK → send final ACK
            rcv_nxt = seg.seq + 1;
            Segment finAck;
            finAck.flags = F_ACK;
            finAck.seq = snd_nxt;
            finAck.ack = rcv_nxt;
            finAck.len = 0;
            conn->deliver(*this, conn->B, finAck);
            // Now start sending data
            try_send_data();
        } else
        {
            // B received ACK of its SYN-ACK: established
        }
        return;
    }

    // Data processing at receiver (B)
    if (name == "B")
    {
        if (seg.seq == rcv_nxt)
        {
            rcv_nxt += seg.len;
            if (has(seg.flags, F_FIN)) rcv_nxt += 1;
        }
        // Always ACK cumulatively
        Segment ack;
        ack.flags = F_ACK;
        ack.seq = 5000;
        ack.ack = rcv_nxt;
        ack.len = 0;
        conn->deliver(*this, conn->A, ack);
        return;
    }

    // ACK handling at sender (A)
    if (name == "A" && has(seg.flags, F_ACK))
    {
        if (seg.ack > snd_una)
        {
            // New ACK
            total_acks_received++;
            snd_una = seg.ack;
            dupacks = 0;

            // Congestion control
            bool slow_start = cwnd < ssthresh;
            if (cwnd < ssthresh) cwnd += mss;                // slow start
            else cwnd += (mss * mss) / max<uint32_t>(1, cwnd); // congestion avoidance

            // Track TCP state metrics
            TracyPlot("TCP_CWND", (int64_t) cwnd);
            TracyPlot("TCP_SSThresh", (int64_t) ssthresh);
            TracyPlot("TCP_InFlight", (int64_t) (snd_nxt - snd_una));
            TracyPlot("TCP_AppBytesSent", (int64_t) app_bytes_sent);
            TracyPlot("TCP_Retransmits", (int64_t) retransmits);
            TracyPlot("TCP_DupAcks", (int64_t) dupacks);
            TracyPlot("TCP_SlowStart", (int64_t) (slow_start ? 1 : 0));
            TracyPlot("TCP_TotalACKs", (int64_t) total_acks_received);
            TracyPlot("TCP_SegmentsSent", (int64_t) total_segments_sent);

            cancel_timer();
            if (snd_una < snd_nxt) arm_timer(); // still outstanding data
            try_send_data();

            // Was FIN acknowledged?
            if (fin_sent && seg.ack == snd_nxt)
            { fin_acked = true; }
        } else if (seg.ack == snd_una && snd_una < snd_nxt)
        {
            // Duplicate ACK
            dupacks++;
            TracyPlot("TCP_DupAcks", (int64_t) dupacks);

            if (dupacks == 3)
            {
                // Fast retransmit / recovery
                TracyMessageC("Fast Retransmit", 16, 0xFF0000);
                ssthresh = max<uint32_t>(mss * 2, cwnd / 2);
                cwnd = ssthresh + 3 * mss;
                retransmits++;

                TracyPlot("TCP_CWND", (int64_t) cwnd);
                TracyPlot("TCP_SSThresh", (int64_t) ssthresh);
                TracyPlot("TCP_Retransmits", (int64_t) retransmits);

                send_segment(snd_una, mss, F_NONE); // retransmit oldest
                arm_timer();
            } else if (dupacks > 3)
            {
                cwnd += mss; // inflate
                TracyPlot("TCP_CWND", (int64_t) cwnd);
                try_send_data();
            }
        }
    }
}

void Endpoint::try_send_data()
{
    ZoneScoped;
    if (name != "A" || !established) return;

    // Stop when all data + FIN sent
    while (true)
    {
        uint32_t flight = snd_nxt - snd_una;
        uint32_t allowed = (uint32_t) min<uint64_t>(cwnd, rwnd);
        if (flight >= allowed) break;

        if (app_bytes_sent < app_bytes_total)
        {
            uint32_t can = min<uint32_t>(allowed - flight, mss);
            uint32_t remaining = (uint32_t) min<uint64_t>(mss, app_bytes_total - app_bytes_sent);
            auto len = (uint16_t) min<uint32_t>(can, remaining);
            if (len == 0) break;
            send_segment(snd_nxt, len, F_NONE);
            if (!timer_running) arm_timer();
            snd_nxt += len;
            app_bytes_sent += len;
        } else if (!fin_sent)
        {
            // Send FIN when all data queued
            send_segment(snd_nxt, 0, F_FIN);
            snd_nxt += 1;
            fin_sent = true;
            if (!timer_running) arm_timer();
        } else
        {
            break;
        }
    }
}

void Endpoint::send_segment(uint32_t seq, uint16_t len, Flags fl)
{
    ZoneScoped;
    Segment s;
    s.seq = seq;
    s.len = len;
    s.flags = fl;
    if (has(fl, F_ACK)) s.ack = rcv_nxt;

    // Track segment transmission
    if (name == "A")
    {
        total_segments_sent++;
    }

    auto &dst = (this == &(conn->A)) ? conn->B : conn->A;
    conn->deliver(*this, dst, s);
}

void Endpoint::arm_timer()
{
    timer_running = true;
    timer_deadline = sim.now + rto;
    sim.at(timer_deadline, [this]()
    { if (timer_running && sim.now >= timer_deadline) on_timeout(); });
}

void Endpoint::cancel_timer()
{ timer_running = false; }

void Endpoint::on_timeout()
{
    ZoneScoped;
    TracyMessageC("RTO Timeout", 11, 0xFFA500);

    // Timeout: multiplicative decrease, reset to slow start
    ssthresh = max<uint32_t>(mss * 2, cwnd / 2);
    cwnd = mss;
    rto = min(4.0, rto * 2.0); // simple backoff, cap at 4s
    dupacks = 0;
    retransmits++;

    // Track timeout event metrics
    TracyPlot("TCP_CWND", (int64_t) cwnd);
    TracyPlot("TCP_SSThresh", (int64_t) ssthresh);
    TracyPlot("TCP_RTO", rto);
    TracyPlot("TCP_Retransmits", (int64_t) retransmits);

    // Retransmit the oldest unacked (up to MSS)
    uint32_t outstanding = snd_nxt - snd_una;
    auto len = (uint16_t) min<uint32_t>(mss, outstanding ? outstanding : mss);
    send_segment(snd_una, len, F_NONE);
    arm_timer();
}

TCPConnection::TCPConnection(Link L, size_t app_bytes)
        : A({"A", this}), B({"B", this}), link(L)
{
    A.app_bytes_total = app_bytes;
    // Initial values (Reno-ish)
    A.iss = 1000;
    A.snd_una = A.iss;
    A.snd_nxt = A.iss;
    A.cwnd = A.mss;
    A.ssthresh = 65535;
    B.rcv_nxt = 5000; // ISN for B will be chosen on SYN
}

void TCPConnection::deliver(Endpoint &src, Endpoint &dst, Segment seg) const
{
    ZoneScoped;
    seg.wire_size = seg.len + (size_t) header_bytes;
    Time arrival = sim.now + link.xmit_delay(seg.wire_size) + link.prop_delay_s;
    bool dropped = link.lost();

    total_packets_sent++;

    if (dropped)
    {
        total_packets_dropped++;
        TracyPlot("TCP_PacketsDropped", (int64_t) total_packets_dropped);
        TracyPlot("TCP_PacketsSent", (int64_t) total_packets_sent);
        TracyPlot("TCP_LossRate_percent", ((double) total_packets_dropped / (double) total_packets_sent) * 100.0);
        TracyMessageC("Packet Dropped", 14, 0xFF00FF);
    } else
    {
        TracyPlot("TCP_PacketsSent", (int64_t) total_packets_sent);
    }

    sim.at(arrival, [&dst, seg, dropped]()
    {
        if (!dropped) dst.on_segment(seg);
        // else: drop silently
    });
}

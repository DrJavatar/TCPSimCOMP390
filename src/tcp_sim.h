//
// Created by david on 11/11/2025.
//
#pragma once

#include <cstdint>
#include <random>
#include <functional>
#include <queue>

using namespace std;

struct Endpoint;
struct TCPConnection;

// ============ Utilities ============
using Time = double;
static std::mt19937_64 rng(12345);

struct Event
{
    Time t;
    function<void()> fn;

    bool operator<(const Event &o) const
    { return t > o.t; } // min-heap via greater
};

static struct Simulator
{
    Time now = 0.0;
    priority_queue<Event> pq;

    void at(Time t, function<void()> fn)
    {
        pq.push(Event{t, std::move(fn)});
    }

    void run();
} sim;

struct Link
{
    double bandwidth_bps;     // bits per second
    double prop_delay_s;      // seconds one-way
    double loss_prob;         // Bernoulli loss on each direction

    // serialization delay for N bytes (headers included)
    [[nodiscard]] Time xmit_delay(size_t bytes) const;

    [[nodiscard]] bool lost() const;
};

// ============ TCP segment ============
enum Flags : uint8_t
{
    F_NONE = 0, F_SYN = 1, F_ACK = 2, F_FIN = 4
};

struct Segment {
    uint32_t seq = 0;
    uint32_t ack = 0;
    Flags flags = F_NONE;
    uint16_t len = 0;
    size_t wire_size = 0;
};

// ============ TCP Endpoint ============
struct Endpoint {
    string name;              // "A" or "B"
    TCPConnection* conn = nullptr;
    // Receiver state
    uint32_t rcv_nxt = 0;

    // Sender state
    uint32_t iss = 0, snd_una = 0, snd_nxt = 0;
    uint32_t cwnd = 0, ssthresh = 0;
    uint32_t dupacks = 0;
    uint32_t mss = 1000;          // bytes
    uint32_t rwnd = 1<<30;        // infinite for simplicity
    bool established = false;
    bool fin_sent = false, fin_acked = false;

    // RTO management (single outstanding timer)
    Time rto = 1.0;               // seconds (fixed; you can add RTT estimator)
    bool timer_running = false;
    Time timer_deadline = 0.0;

    // App data to send (only on A)
    size_t app_bytes_total = 0;
    size_t app_bytes_sent = 0;

    // Stats
    size_t retransmits = 0;
    size_t total_segments_sent = 0;
    size_t total_acks_received = 0;

    // API
    void start_client();        // A starts with SYN
    void on_segment(const Segment& seg);
    void try_send_data();
    void send_segment(uint32_t seq, uint16_t len, Flags fl);
    void arm_timer();
    void cancel_timer();
    void on_timeout();
};

struct TCPConnection {
    Endpoint A, B;
    Link link;
    Time header_bytes = 40;
    mutable size_t total_packets_dropped = 0;
    mutable size_t total_packets_sent = 0;
    TCPConnection(Link L, size_t app_bytes);
    void deliver(Endpoint& src, Endpoint& dst, Segment seg) const;
};

inline Flags operator|(Flags a, Flags b)
{
    return (Flags) ((uint8_t) a | (uint8_t) b);
}

inline bool has(Flags f, Flags m)
{
    return ((uint8_t) f & (uint8_t) m) != 0;
}
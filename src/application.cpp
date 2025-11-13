//
// Created by david on 11/11/2025.
//

#include <iostream>
#include <iomanip>
#include "tcp_sim.h"

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main()
{
    ios::sync_with_stdio(false);
    // Link: 10 Mbps, 25ms one-way, 1% loss
    Link L{10e6, 0.025, 0.01};

    size_t bytes_to_send = 200 * 1024; // 200 KiB app data
    TCPConnection c(L, bytes_to_send);

    // Start connection at t=0: A is client
    sim.at(0.0, [&]{ c.A.start_client(); });

    // Stop condition: all data ACKed and FIN ACKed, or time limit
    Time end_check = 0.1;
    std::function<void()> periodic;
    periodic = [&]() {
        bool done = (c.A.fin_sent && c.A.fin_acked && (c.A.snd_una == c.A.snd_nxt));
        if (done || sim.now > 120.0) {
            cout << fixed << setprecision(3);
            cout << "Simulation finished at t=" << sim.now << " s\n";
            cout << "Data: " << bytes_to_send << " bytes, retransmits=" << c.A.retransmits << "\n";
            cout << "cwnd=" << c.A.cwnd << " ssthresh=" << c.A.ssthresh << " RTO=" << c.A.rto << "s\n";
        } else {
            sim.at(sim.now + end_check, periodic);
        }
    };
    sim.at(0.0, periodic);

    sim.run();
    return 0;
}

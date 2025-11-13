//
// Created by david on 11/11/2025.
//

#include <iostream>
#include <iomanip>
#include <cstring>
#include "tcp_sim.h"
#include <tracy/Tracy.hpp>

void run_simulation(const char* scenario_name, Link L, size_t bytes_to_send, Time end_check_interval = 0.05)
{
    ZoneScoped;
    ZoneName(scenario_name, strlen(scenario_name));

    TracyMessage(scenario_name, 0x00FFFF);

    cout << "\n=== Running Scenario: " << scenario_name << " ===\n";
    cout << "Bandwidth: " << (L.bandwidth_bps / 1e6) << " Mbps, ";
    cout << "Delay: " << (L.prop_delay_s * 1000.0) << " ms, ";
    cout << "Loss: " << (L.loss_prob * 100.0) << "%\n";
    cout << "Data to send: " << (bytes_to_send / 1024.0) << " KiB\n";

    // Reset simulator
    sim.now = 0.0;
    while (!sim.pq.empty()) sim.pq.pop();

    TCPConnection c(L, bytes_to_send);

    // Plot link parameters
    TracyPlot("TCP_LinkBandwidth_Mbps", L.bandwidth_bps / 1e6);
    TracyPlot("TCP_LinkDelay_ms", L.prop_delay_s * 1000.0);
    TracyPlot("TCP_LinkLoss_percent", L.loss_prob * 100.0);

    // Start connection at t=0: A is client
    sim.at(0.0, [&]{ c.A.start_client(); });

    // Stop condition: all data ACKed and FIN ACKed, or time limit
    Time last_time = 0.0;
    size_t last_bytes = 0;

    std::function<void()> periodic;
    periodic = [&, end_check_interval]() {
        ZoneScoped;
        ZoneName("Periodic Check", 14);

        bool done = (c.A.fin_sent && c.A.fin_acked && (c.A.snd_una == c.A.snd_nxt));

        // Calculate instantaneous throughput
        if (sim.now > last_time) {
            double elapsed = sim.now - last_time;
            size_t bytes_delta = c.A.app_bytes_sent - last_bytes;
            double throughput_bps = (bytes_delta * 8.0) / elapsed;
            double throughput_mbps = throughput_bps / 1e6;

            TracyPlot("TCP_Throughput_Mbps", throughput_mbps);
            TracyPlot("TCP_Utilization_percent", (throughput_bps / L.bandwidth_bps) * 100.0);

            // Also plot average throughput since start
            if (sim.now > 0) {
                double avg_throughput_mbps = (c.A.app_bytes_sent * 8.0 / sim.now) / 1e6;
                TracyPlot("TCP_AvgThroughput_Mbps", avg_throughput_mbps);
            }

            last_time = sim.now;
            last_bytes = c.A.app_bytes_sent;
        }

        // Plot completion percentage and goodput
        double completion = (double)c.A.app_bytes_sent / (double)bytes_to_send * 100.0;
        TracyPlot("TCP_Completion_percent", completion);

        // Plot efficiency metrics
        if (c.total_packets_sent > 0) {
            double retransmit_rate = ((double)c.A.retransmits / (double)c.A.total_segments_sent) * 100.0;
            TracyPlot("TCP_RetransmitRate_percent", retransmit_rate);
        }

        if (done || sim.now > 300.0) {
            TracyMessage("Simulation Complete", 0x00FF00);
            cout << fixed << setprecision(3);
            cout << "Simulation finished at t=" << sim.now << " s\n";
            cout << "Data sent: " << (bytes_to_send / 1024.0) << " KiB, retransmits=" << c.A.retransmits << "\n";
            cout << "Packets: sent=" << c.total_packets_sent << ", dropped=" << c.total_packets_dropped
                 << " (" << ((double)c.total_packets_dropped / c.total_packets_sent * 100.0) << "%)\n";
            cout << "Final cwnd=" << c.A.cwnd << " ssthresh=" << c.A.ssthresh << " RTO=" << c.A.rto << "s\n";
            cout << "Average throughput: " << (bytes_to_send * 8.0 / sim.now / 1e6) << " Mbps\n";
            cout << "Link utilization: " << (bytes_to_send * 8.0 / sim.now / L.bandwidth_bps * 100.0) << "%\n";
        } else {
            sim.at(sim.now + end_check_interval, periodic);
        }
    };
    sim.at(0.0, periodic);

    sim.run();
}

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main()
{
    printf("If using Tracy, connect then continue.\n");
    system("pause");
    ZoneScoped;
    ios::sync_with_stdio(false);

    cout << fixed << setprecision(3);
    cout << "========================================\n";
    cout << "TCP Simulation Suite with Tracy Profiling\n";
    cout << "========================================\n";

    // Scenario 1: High bandwidth, low latency, low loss - ideal conditions
    run_simulation("S1: Ideal (100Mbps, 10ms, 0.1% loss)",
                   Link{100e6, 0.010, 0.001},
                   5 * 1024 * 1024,  // 5 MiB
                   0.05);

    // Scenario 2: Moderate bandwidth, moderate latency, moderate loss
    run_simulation("S2: Moderate (10Mbps, 50ms, 2% loss)",
                   Link{10e6, 0.050, 0.02},
                   2 * 1024 * 1024,  // 2 MiB
                   0.05);

    // Scenario 3: Low bandwidth, high latency, high loss - challenging
    run_simulation("S3: Challenging (1Mbps, 100ms, 5% loss)",
                   Link{1e6, 0.100, 0.05},
                   512 * 1024,  // 512 KiB
                   0.05);

    // Scenario 4: Very high bandwidth, very low latency - data center
    run_simulation("S4: DataCenter (1Gbps, 1ms, 0.01% loss)",
                   Link{1e9, 0.001, 0.0001},
                   10 * 1024 * 1024,  // 10 MiB
                   0.02);

    // Scenario 5: Satellite link - very high latency
    run_simulation("S5: Satellite (5Mbps, 250ms, 1% loss)",
                   Link{5e6, 0.250, 0.01},
                   1 * 1024 * 1024,  // 1 MiB
                   0.1);

    // Scenario 6: Mobile network - variable conditions
    run_simulation("S6: Mobile (20Mbps, 30ms, 3% loss)",
                   Link{20e6, 0.030, 0.03},
                   3 * 1024 * 1024,  // 3 MiB
                   0.05);

    cout << "\n========================================\n";
    cout << "All scenarios complete!\n";
    cout << "Check Tracy Profiler for detailed graphs\n";
    cout << "========================================\n";

    return 0;
}

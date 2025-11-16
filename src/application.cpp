//
// Created by david on 11/11/2025.
//

#include <iostream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <cmath>
#include "tcp_sim.h"
#include <tracy/Tracy.hpp>

// Structure to hold results from a single trial
struct TrialResult {
    double completion_time;
    double avg_throughput_mbps;
    double link_utilization;
    size_t retransmits;
    size_t packets_sent;
    size_t packets_dropped;
    double loss_rate;
    uint32_t final_cwnd;
    uint32_t final_ssthresh;
};

// Structure to hold statistics across trials
struct ScenarioStats {
    double mean_time = 0;
    double std_time = 0;
    double min_time = 1e9;
    double max_time = 0;

    double mean_throughput = 0;
    double std_throughput = 0;
    double min_throughput = 1e9;
    double max_throughput = 0;

    double mean_utilization = 0;
    double mean_retransmits = 0;
    double mean_loss_rate = 0;

    void compute(const std::vector<TrialResult>& trials) {
        if (trials.empty()) return;

        double sum_time = 0, sum_throughput = 0, sum_util = 0, sum_retrans = 0, sum_loss = 0;

        for (const auto& t : trials) {
            sum_time += t.completion_time;
            sum_throughput += t.avg_throughput_mbps;
            sum_util += t.link_utilization;
            sum_retrans += t.retransmits;
            sum_loss += t.loss_rate;

            min_time = std::min(min_time, t.completion_time);
            max_time = std::max(max_time, t.completion_time);
            min_throughput = std::min(min_throughput, t.avg_throughput_mbps);
            max_throughput = std::max(max_throughput, t.avg_throughput_mbps);
        }

        size_t n = trials.size();
        mean_time = sum_time / n;
        mean_throughput = sum_throughput / n;
        mean_utilization = sum_util / n;
        mean_retransmits = sum_retrans / n;
        mean_loss_rate = sum_loss / n;

        // Compute standard deviations
        double var_time = 0, var_throughput = 0;
        for (const auto& t : trials) {
            var_time += (t.completion_time - mean_time) * (t.completion_time - mean_time);
            var_throughput += (t.avg_throughput_mbps - mean_throughput) * (t.avg_throughput_mbps - mean_throughput);
        }
        std_time = std::sqrt(var_time / n);
        std_throughput = std::sqrt(var_throughput / n);
    }
};

TrialResult run_simulation(const char* scenario_name, Link L, size_t bytes_to_send, Time end_check_interval, bool verbose)
{
    ZoneScoped;
    ZoneName(scenario_name, strlen(scenario_name));

    TracyMessageC(scenario_name, strlen(scenario_name), 0x00FFFF);

    if (verbose) {
        cout << "\n=== Running Scenario: " << scenario_name << " ===\n";
        cout << "Bandwidth: " << (L.bandwidth_bps / 1e6) << " Mbps, ";
        cout << "Delay: " << (L.prop_delay_s * 1000.0) << " ms, ";
        cout << "Loss: " << (L.loss_prob * 100.0) << "%\n";
        cout << "Data to send: " << (bytes_to_send / 1024.0) << " KiB\n";
    }

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

        // Mark frame at each periodic check for meaningful Tracy timeline
        FrameMark;

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
            TracyMessageC("Simulation Complete", 20, 0x00FF00);
            if (verbose) {
                cout << fixed << setprecision(3);
                cout << "Simulation finished at t=" << sim.now << " s\n";
                cout << "Data sent: " << (bytes_to_send / 1024.0) << " KiB, retransmits=" << c.A.retransmits << "\n";
                cout << "Packets: sent=" << c.total_packets_sent << ", dropped=" << c.total_packets_dropped
                     << " (" << ((double)c.total_packets_dropped / c.total_packets_sent * 100.0) << "%)\n";
                cout << "Final cwnd=" << c.A.cwnd << " ssthresh=" << c.A.ssthresh << " RTO=" << c.A.rto << "s\n";
                cout << "Average throughput: " << (bytes_to_send * 8.0 / sim.now / 1e6) << " Mbps\n";
                cout << "Link utilization: " << (bytes_to_send * 8.0 / sim.now / L.bandwidth_bps * 100.0) << "%\n";
            }
        } else {
            sim.at(sim.now + end_check_interval, periodic);
        }
    };
    sim.at(0.0, periodic);

    sim.run();

    // Return results
    TrialResult result;
    result.completion_time = sim.now;
    result.avg_throughput_mbps = (bytes_to_send * 8.0 / sim.now) / 1e6;
    result.link_utilization = (bytes_to_send * 8.0 / sim.now / L.bandwidth_bps) * 100.0;
    result.retransmits = c.A.retransmits;
    result.packets_sent = c.total_packets_sent;
    result.packets_dropped = c.total_packets_dropped;
    result.loss_rate = c.total_packets_sent > 0 ? ((double)c.total_packets_dropped / c.total_packets_sent * 100.0) : 0.0;
    result.final_cwnd = c.A.cwnd;
    result.final_ssthresh = c.A.ssthresh;

    return result;
}

// Run multiple trials and compute statistics
void run_scenario_trials(const char* scenario_name, Link L, size_t bytes_to_send, size_t num_trials = 20)
{
    ZoneScoped;
    cout << "\n========================================\n";
    cout << "SCENARIO: " << scenario_name << "\n";
    cout << "Bandwidth: " << (L.bandwidth_bps / 1e6) << " Mbps, ";
    cout << "Delay: " << (L.prop_delay_s * 1000.0) << " ms, ";
    cout << "Loss: " << (L.loss_prob * 100.0) << "%\n";
    cout << "Data to send: " << (bytes_to_send / 1024.0) << " KiB\n";
    cout << "Running " << num_trials << " trials...\n";
    cout << "----------------------------------------\n";

    std::vector<TrialResult> trials;
    trials.reserve(num_trials);

    // Run trials
    for (size_t i = 0; i < num_trials; ++i) {
        cout << "  Trial " << (i + 1) << "/" << num_trials << "... " << flush;

        // Only verbose output for first trial
        TrialResult result = run_simulation(scenario_name, L, bytes_to_send, 0.05, i == 0);
        trials.push_back(result);

        if (i > 0) {  // Skip first trial since it already printed
            cout << "done (" << fixed << setprecision(2) << result.completion_time << "s, "
                 << result.avg_throughput_mbps << " Mbps)\n";
        }
    }

    // Compute statistics
    ScenarioStats stats;
    stats.compute(trials);

    // Print summary
    cout << "\n=== STATISTICS (n=" << num_trials << ") ===\n";
    cout << fixed << setprecision(3);
    cout << "Completion Time:\n";
    cout << "  Mean:   " << stats.mean_time << " s ± " << stats.std_time << " s\n";
    cout << "  Range:  [" << stats.min_time << ", " << stats.max_time << "] s\n";
    cout << "\nThroughput:\n";
    cout << "  Mean:   " << stats.mean_throughput << " Mbps ± " << stats.std_throughput << " Mbps\n";
    cout << "  Range:  [" << stats.min_throughput << ", " << stats.max_throughput << "] Mbps\n";
    cout << "\nUtilization:\n";
    cout << "  Mean:   " << stats.mean_utilization << " %\n";
    cout << "\nLoss & Retransmissions:\n";
    cout << "  Mean Packet Loss:  " << stats.mean_loss_rate << " %\n";
    cout << "  Mean Retransmits:  " << stats.mean_retransmits << "\n";
    cout << "========================================\n";

    // Tracy plot for aggregate stats
    TracyPlot("Scenario_MeanThroughput_Mbps", stats.mean_throughput);
    TracyPlot("Scenario_MeanTime_s", stats.mean_time);
    TracyPlot("Scenario_MeanUtilization_percent", stats.mean_utilization);
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
    cout << "Multi-Trial Statistical Analysis\n";
    cout << "========================================\n";

    // Number of trials per scenario for statistical significance
    const size_t TRIALS = 20;

    // Scenario 1: High bandwidth,
    // low latency, low loss - ideal conditions
    run_scenario_trials("S1: Ideal (100Mbps, 10ms, 0.1% loss)",
                        Link{100e6, 0.010, 0.001},
                        5 * 1024 * 1024,  // 5 MiB
                        TRIALS);

    // Scenario 2: Moderate bandwidth, moderate latency, moderate loss
    run_scenario_trials("S2: Moderate (10Mbps, 50ms, 2% loss)",
                        Link{10e6, 0.050, 0.02},
                        2 * 1024 * 1024,  // 2 MiB
                        TRIALS);

    // Scenario 3: Low bandwidth, high latency, high loss - challenging
    run_scenario_trials("S3: Challenging (1Mbps, 100ms, 5% loss)",
                        Link{1e6, 0.100, 0.05},
                        512 * 1024,  // 512 KiB
                        TRIALS);

    // Scenario 4: Very high bandwidth, very low latency - data center
    run_scenario_trials("S4: DataCenter (1Gbps, 1ms, 0.01% loss)",
                        Link{1e9, 0.001, 0.0001},
                        10 * 1024 * 1024,  // 10 MiB
                        TRIALS);

    // Scenario 5: Satellite link - very high latency
    run_scenario_trials("S5: Satellite (5Mbps, 250ms, 1% loss)",
                        Link{5e6, 0.250, 0.01},
                        1 * 1024 * 1024,  // 1 MiB
                        TRIALS);

    // Scenario 6: Mobile network - variable conditions
    run_scenario_trials("S6: Mobile (20Mbps, 30ms, 3% loss)",
                        Link{20e6, 0.030, 0.03},
                        3 * 1024 * 1024,  // 3 MiB
                        TRIALS);

    cout << "\n========================================\n";
    cout << "All scenarios complete!\n";
    cout << "Total trials run: " << (TRIALS * 6) << " (" << TRIALS << " per scenario)\n";
    cout << "Check Tracy Profiler for detailed graphs\n";
    cout << "========================================\n";

    return 0;
}

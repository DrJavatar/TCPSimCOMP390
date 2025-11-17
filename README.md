# TCPSim - TCP Congestion Control Simulator with Tracy Profiling

## Description

TCPSim is a discrete-event network simulator that models TCP Reno congestion control behavior across various network conditions. The simulator provides detailed performance analysis and visualization through Tracy Profiler integration.

### What It Does

**Core Functionality:**
- Simulates TCP Reno congestion control algorithm implementation
- Models complete TCP handshake (SYN, SYN-ACK, ACK)
- Implements key TCP features:
  - Slow start and congestion avoidance phases
  - Fast retransmit and fast recovery
  - Timeout-based retransmission with exponential backoff
  - Duplicate ACK detection
  - Congestion window (cwnd) and slow start threshold (ssthresh) management

**Network Simulation:**
- Configurable link parameters:
  - Bandwidth (bits per second)
  - Propagation delay (one-way latency)
  - Packet loss probability (Bernoulli distribution)
- Event-driven simulation engine with priority queue scheduling
- Realistic packet transmission and delivery modeling

**Performance Analysis:**
The simulator runs multiple trials across 6 predefined network scenarios:

1. **S1: Ideal** - 100 Mbps, 10ms delay, 0.1% loss (5 MiB transfer)
2. **S2: Moderate** - 10 Mbps, 50ms delay, 2% loss (2 MiB transfer)
3. **S3: Challenging** - 1 Mbps, 100ms delay, 5% loss (512 KiB transfer)
4. **S4: DataCenter** - 1 Gbps, 1ms delay, 0.01% loss (10 MiB transfer)
5. **S5: Satellite** - 5 Mbps, 250ms delay, 1% loss (1 MiB transfer)
6. **S6: Mobile** - 20 Mbps, 30ms delay, 3% loss (3 MiB transfer)

Each scenario runs 20 trials by default to generate statistical data including:
- Mean/min/max completion time with standard deviation
- Average throughput (Mbps) and link utilization
- Packet loss rates and retransmission statistics

**Tracy Profiling Integration:**
Real-time performance monitoring with 25+ metrics:
- TCP state: cwnd, ssthresh, RTO, in-flight bytes
- Throughput: instantaneous, average, utilization percentage
- Loss metrics: packets dropped, loss rate, retransmit rate
- Progress: completion percentage, bytes sent/ACKed
- Events: fast retransmits, timeouts, packet drops (color-coded)

---

## Building and Running with Tracy

### Prerequisites

- **CMake** 3.15 or higher
- **C++23** compatible compiler (MSVC on Windows)
- **Tracy Profiler** application (for visualization)

The build system automatically fetches dependencies:
- Tracy library (v0.12.2) via FetchContent
- vcpkg for package management
- OpenGL and FreeType (for Tracy GUI)

### Build Steps

1. **Configure the project:**
   ```bash
   cmake -B build
   ```

2. **Build the executable:**
   ```bash
   cmake --build build --config Release
   ```

   The executable will be created at: `build/Release/tcp.exe`

### Running with Tracy Profiler

**Step 1: Launch Tracy Profiler**
- Download Tracy Profiler from: https://github.com/wolfpld/tracy/releases
- Run the Tracy profiler GUI application
- Click "Connect" and wait for incoming connection

**Step 2: Run the Simulator**
```bash
cd build/Release
./tcp.exe
```

The application will display:
```
If using Tracy, connect then continue.
Press any key to continue . . .
```

**Step 3: Connect and Monitor**
1. Ensure Tracy profiler shows "Waiting for connection..."
2. Press any key in the simulator terminal to continue
3. Tracy will automatically connect and begin capturing data
4. Watch real-time graphs and metrics as scenarios execute

### Tracy Features to Explore

**Timeline View:**
- Zone-scoped execution (simulation phases, TCP events)
- Frame markers at periodic intervals (50ms checks)
- Color-coded messages for events:
  - 🔵 Cyan: Scenario start
  - 🔴 Red: Fast retransmit events
  - 🟠 Orange: RTO timeout events
  - 🟣 Magenta: Packet drops
  - 🟢 Green: Simulation complete

**Plots Available:**
- `Simulation Time` - Current simulation time
- `TCP_CWND` - Congestion window size
- `TCP_SSThresh` - Slow start threshold
- `TCP_InFlight` - Bytes in flight (unACKed)
- `TCP_Throughput_Mbps` - Instantaneous throughput
- `TCP_AvgThroughput_Mbps` - Average throughput since start
- `TCP_Utilization_percent` - Link utilization percentage
- `TCP_PacketsSent` - Total packets transmitted
- `TCP_PacketsDropped` - Cumulative dropped packets
- `TCP_LossRate_percent` - Packet loss rate
- `TCP_RetransmitRate_percent` - Retransmission rate
- `TCP_Completion_percent` - Data transfer progress
- `TCP_SlowStart` - Binary indicator (1=slow start, 0=congestion avoidance)
- Plus scenario-level aggregate statistics

**Performance Analysis:**
- CPU usage and memory allocation tracking
- Function call hierarchy and timing
- Statistical analysis across multiple trials

### Console Output

The simulator provides detailed console output:
```
========================================
SCENARIO: S1: Ideal (100Mbps, 10ms, 0.1% loss)
Bandwidth: 100.000 Mbps, Delay: 10.000 ms, Loss: 0.100%
Data to send: 5120.000 KiB
Running 20 trials...
----------------------------------------
  Trial 1/20...

=== Running Scenario: S1: Ideal (100Mbps, 10ms, 0.1% loss) ===
...
Simulation finished at t=0.425 s
Data sent: 5120.000 KiB, retransmits=2
Packets: sent=5234, dropped=6 (0.115%)
Final cwnd=54000 ssthresh=43000 RTO=1.000s
Average throughput: 96.471 Mbps
Link utilization: 96.471%

=== STATISTICS (n=20) ===
Completion Time:
  Mean:   0.425 s ± 0.008 s
  Range:  [0.412, 0.438] s

Throughput:
  Mean:   96.234 Mbps ± 1.852 Mbps
  Range:  [93.145, 98.835] Mbps
...
```

### Customization

**Modify Scenarios:**
Edit `src/application.cpp` lines 259-295 to change:
- Number of trials per scenario (`TRIALS` constant)
- Link parameters (bandwidth, delay, loss)
- Data transfer sizes

**Adjust Simulation:**
Edit `src/tcp_sim.cpp` to modify:
- TCP parameters (MSS, initial cwnd, ssthresh)
- Congestion control algorithm behavior
- Timeout and retransmission logic

**Tracy Configuration:**
- Tracy is enabled by default in the build
- Metrics are plotted at key TCP events and periodic checks
- Frame markers occur every 50ms (simulation time)

---

## Project Structure

```
TCPSim/
├── src/
│   ├── tcp_sim.h          # Core TCP simulator declarations
│   ├── tcp_sim.cpp        # TCP logic implementation
│   └── application.cpp    # Main application and scenario runner
├── CMakeLists.txt         # Build configuration
├── cmake.toml             # CMake configuration source
└── build/                 # Build output directory
```

---

## Key Implementation Details

**TCP Reno Implementation:**
- `tcp_sim.cpp:34-40` - Client handshake initiation
- `tcp_sim.cpp:42-162` - Segment reception and ACK processing
- `tcp_sim.cpp:108-125` - Congestion window updates (slow start/CA)
- `tcp_sim.cpp:140-159` - Fast retransmit/recovery (3 duplicate ACKs)
- `tcp_sim.cpp:230-253` - Timeout handling with multiplicative decrease

**Event Simulation:**
- `tcp_sim.cpp:7-21` - Discrete event simulator core
- `application.cpp:78-184` - Simulation runner with metrics collection
- `application.cpp:112-166` - Periodic monitoring and frame marking

**Statistical Analysis:**
- `application.cpp:14-24` - Per-trial result structure
- `application.cpp:26-76` - Multi-trial statistics computation
- `application.cpp:187-240` - Scenario execution and reporting

---

## Troubleshooting

**Tracy Connection Issues:**
- Ensure Tracy profiler is running BEFORE starting tcp.exe
- Check firewall settings (Tracy uses network sockets)
- Default Tracy port is 8086

**Build Errors:**
- Verify CMake version ≥ 3.15
- Ensure C++23 compiler support
- Check vcpkg dependency fetch (requires internet)

**Performance Issues:**
- Tracy adds minimal overhead (~2-5%)
- Reduce trial count if too slow
- Close Tracy GUI if not needed for faster execution

---

## License & Credits

This simulator uses:
- **Tracy Profiler** (v0.12.2) - https://github.com/wolfpld/tracy
- TCP Reno algorithm based on RFC 5681 principles

Built with CMake and vcpkg for dependency management.

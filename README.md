# Low-Latency Load Generator

A small Linux performance-testing project consisting of a C++17 TCP echo server and a C++17 open-loop load generator. The generator sends requests at a target rate, records end-to-end latency, and reports p50/p95/p99/max latency plus throughput.

## What it demonstrates

- C++17
- Linux TCP sockets
- `poll()` based server
- Multithreaded load generation
- Open-loop request scheduling
- Latency percentile calculation
- Throughput measurement
- CSV result export
- Pytest smoke/integration automation
- CMake

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run a quick benchmark

Terminal 1:

```bash
./build/echo_server --port 9000
```

Terminal 2:

```bash
./build/load_generator \
  --host 127.0.0.1 \
  --port 9000 \
  --requests 10000 \
  --rate 2000 \
  --payload 64 \
  --output results.csv
```

The output includes:

```text
requests
completed
errors
throughput
p50_us
p95_us
p99_us
max_us
```

The generator is open-loop: requests are scheduled against a target arrival rate rather than waiting for the previous request to finish.

## Automated smoke test

After building:

```bash
pytest -q
```

The test starts the server, runs a small load, and verifies that all requests complete successfully.

## Notes

This is a teaching/performance-engineering project, not a production-grade benchmark. Results depend on CPU scheduling, kernel configuration, machine load, and network stack behavior. The project intentionally reports raw measurements rather than claiming a universal latency number.

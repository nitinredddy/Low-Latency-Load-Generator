#!/usr/bin/env python3
import json
import os
import subprocess
import time
import pytest

@pytest.fixture(scope="module")
def server_and_bins():
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    build_dir = os.path.join(base_dir, "build")
    server_bin = os.path.join(build_dir, "echo_server")
    load_bin = os.path.join(build_dir, "load_generator")

    assert os.path.exists(server_bin), f"echo_server not found at {server_bin}"
    assert os.path.exists(load_bin), f"load_generator not found at {load_bin}"

    port = 19292
    server = subprocess.Popen(
        [server_bin, "--port", str(port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    ready = False
    deadline = time.time() + 4
    while time.time() < deadline:
        line = server.stdout.readline()
        if "LISTENING" in line:
            ready = True
            break
    assert ready, "Server failed to start"

    yield {"server_bin": server_bin, "load_bin": load_bin, "port": port}

    server.terminate()
    try:
        server.wait(timeout=2)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait()

def test_basic_load(server_and_bins):
    cfg = server_and_bins
    cmd = [
        cfg["load_bin"],
        "--host", "127.0.0.1",
        "--port", str(cfg["port"]),
        "--requests", "200",
        "--rate", "1000",
        "--payload", "64"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    assert res.returncode == 0, res.stderr
    assert "completed=200" in res.stdout
    assert "errors=0" in res.stdout

def test_multithreaded_and_pooled_connections(server_and_bins):
    cfg = server_and_bins
    cmd = [
        cfg["load_bin"],
        "--host", "127.0.0.1",
        "--port", str(cfg["port"]),
        "--requests", "400",
        "--rate", "2000",
        "--threads", "4",
        "--connections", "8",
        "--payload", "128"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    assert res.returncode == 0, res.stderr
    assert "completed=400" in res.stdout
    assert "errors=0" in res.stdout

def test_poisson_distribution(server_and_bins):
    cfg = server_and_bins
    cmd = [
        cfg["load_bin"],
        "--host", "127.0.0.1",
        "--port", str(cfg["port"]),
        "--requests", "150",
        "--rate", "800",
        "--distribution", "poisson",
        "--payload", "64"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    assert res.returncode == 0, res.stderr
    assert "errors=0" in res.stdout

def test_raw_echo_mode(server_and_bins):
    cfg = server_and_bins
    cmd = [
        cfg["load_bin"],
        "--host", "127.0.0.1",
        "--port", str(cfg["port"]),
        "--requests", "100",
        "--rate", "500",
        "--raw"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    assert res.returncode == 0, res.stderr
    assert "completed=100" in res.stdout
    assert "errors=0" in res.stdout

def test_csv_and_json_export(server_and_bins, tmp_path):
    cfg = server_and_bins
    csv_file = str(tmp_path / "test_results.csv")
    json_file = str(tmp_path / "test_results.json")

    cmd = [
        cfg["load_bin"],
        "--host", "127.0.0.1",
        "--port", str(cfg["port"]),
        "--requests", "100",
        "--rate", "1000",
        "--output", csv_file,
        "--json", json_file
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    assert res.returncode == 0, res.stderr
    assert os.path.exists(csv_file)
    assert os.path.exists(json_file)

    with open(json_file) as f:
        data = json.load(f)
    assert data["results"]["completed"] == 100
    assert data["results"]["errors"] == 0
    assert "p99" in data["results"]["service_latency_us"]

def test_warmup_discard(server_and_bins):
    cfg = server_and_bins
    cmd = [
        cfg["load_bin"],
        "--host", "127.0.0.1",
        "--port", str(cfg["port"]),
        "--requests", "100",
        "--rate", "1000",
        "--warmup", "20"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    assert res.returncode == 0, res.stderr
    assert "Warmup discarded: 20" in res.stdout
    assert "completed=80" in res.stdout

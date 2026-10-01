#!/usr/bin/env python3
import subprocess
import sys
import time

server_bin = sys.argv[1]
load_bin = sys.argv[2]
port = 19191

server = subprocess.Popen(
    [server_bin, "--port", str(port)],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
)

try:
    ready = False
    deadline = time.time() + 3
    while time.time() < deadline:
        line = server.stdout.readline()
        if "LISTENING" in line:
            ready = True
            break
    assert ready, "server did not become ready"

    result = subprocess.run(
        [load_bin, "--host", "127.0.0.1", "--port", str(port),
         "--requests", "100", "--rate", "500", "--payload", "32"],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert result.returncode == 0, result.stderr
    assert "errors=0" in result.stdout
    assert "p99_us=" in result.stdout
    print(result.stdout)
    print("smoke test passed")
finally:
    server.terminate()
    try:
        server.wait(timeout=2)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait()

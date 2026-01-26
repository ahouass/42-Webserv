#!/bin/bash

echo "=== WEBSERV PERFORMANCE TEST ==="
echo ""

# Configuration
WEBSERV_PORT=8080
WEBSERV_ROOT="./www"
NGINX_ROOT="/usr/local/var/www"  # Default nginx root on macOS

# Create test files
mkdir -p ${WEBSERV_ROOT}/benchmark
mkdir -p ${WEBSERV_ROOT}/cgi-bin

# 1KB test file
echo "<!DOCTYPE html><html><head><title>Test</title></head><body><h1>Benchmark Test</h1><p>$(date)</p></body></html>" > ${WEBSERV_ROOT}/benchmark/1k.html

# 10KB test file (for testing)
for i in {1..500}; do echo "<p>Line $i - Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>" >> ${WEBSERV_ROOT}/benchmark/10k.html; done

# 1MB test file
dd if=/dev/zero of=${WEBSERV_ROOT}/benchmark/1m.bin bs=1024 count=1024 2>/dev/null

# Simple CGI script
cat > ${WEBSERV_ROOT}/cgi-bin/benchmark.py << 'EOF'
#!/usr/bin/env python3
import time
print("Content-Type: text/plain\n")
print(f"CGI Benchmark - PID: {time.time()}")
print("OK")
EOF
chmod +x ${WEBSERV_ROOT}/cgi-bin/benchmark.py

# Simple PHP CGI if needed
cat > ${WEBSERV_ROOT}/cgi-bin/benchmark.php << 'EOF'
<?php
header("Content-Type: text/plain");
echo "CGI PHP Benchmark\n";
echo "Time: " . microtime(true) . "\n";
echo "OK";
?>
EOF

echo "Test files created in ${WEBSERV_ROOT}/benchmark/"
echo ""

# Check if webserv is running
echo "Testing webserv connectivity..."
curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" http://localhost:${WEBSERV_PORT}/benchmark/1k.html

echo ""
echo "=== BENCHMARK TESTS ==="
echo ""

# Test 1: Single request latency
echo "1. Single Request Latency:"
for i in {1..5}; do
    curl -s -o /dev/null -w "Request $i: %{time_total}s\n" http://localhost:${WEBSERV_PORT}/benchmark/1k.html
done
echo ""

# Test 2: Apache Bench if available
if command -v ab &> /dev/null; then
    echo "2. Apache Bench (1000 requests, 10 concurrent):"
    ab -n 1000 -c 10 http://localhost:${WEBSERV_PORT}/benchmark/1k.html 2>/dev/null | grep -E "(Requests per second|Time per request|Failed requests|Transfer rate)"
    echo ""
fi

# Test 3: wrk if available
if command -v wrk &> /dev/null; then
    echo "3. wrk Benchmark (10 seconds, 4 threads, 100 connections):"
    wrk -t4 -c100 -d10s http://localhost:${WEBSERV_PORT}/benchmark/1k.html 2>&1 | head -20
    echo ""
    
    echo "4. wrk with more concurrency (1000 connections):"
    wrk -t4 -c1000 -d10s http://localhost:${WEBSERV_PORT}/benchmark/1k.html 2>&1 | grep -A 5 "Thread Stats"
    echo ""
fi

# Test 4: Concurrent CGI requests
echo "5. CGI Performance (5 concurrent requests):"
echo "Starting CGI tests..."
for i in {1..5}; do
    time curl -s -o /dev/null http://localhost:${WEBSERV_PORT}/cgi-bin/benchmark.py &
done
wait
echo ""

# Test 5: File size comparison
echo "6. Different File Sizes:"
for file in 1k.html 10k.html 1m.bin; do
    if [[ -f "${WEBSERV_ROOT}/benchmark/${file}" ]]; then
        size=$(stat -f%z "${WEBSERV_ROOT}/benchmark/${file}" 2>/dev/null || stat -c%s "${WEBSERV_ROOT}/benchmark/${file}" 2>/dev/null)
        echo -n "File: ${file} (${size} bytes): "
        time curl -s -o /dev/null http://localhost:${WEBSERV_PORT}/benchmark/${file} 2>&1 | grep real | awk '{print $2}'
    fi
done
echo ""

# Test 6: Keep-alive vs no keep-alive
echo "7. Keep-Alive Test (10 sequential requests):"
echo "With Keep-Alive:"
time (for i in {1..10}; do 
    curl -s -o /dev/null -H "Connection: keep-alive" http://localhost:${WEBSERV_PORT}/benchmark/1k.html
done)

echo ""
echo "Without Keep-Alive:"
time (for i in {1..10}; do 
    curl -s -o /dev/null -H "Connection: close" http://localhost:${WEBSERV_PORT}/benchmark/1k.html
done)
echo ""

# Test 7: Memory usage monitoring
echo "8. Memory Usage (run during stress test):"
WEBSERV_PID=$(pgrep webserv)
if [ ! -z "$WEBSERV_PID" ]; then
    echo "Webserv PID: $WEBSERV_PID"
    echo "Current memory:"
    ps -o rss= -p $WEBSERV_PID | awk '{print "RSS: " $1/1024 " MB"}'
fi

# Cleanup
echo ""
echo "=== CLEANUP ==="
rm -rf ${WEBSERV_ROOT}/benchmark ${WEBSERV_ROOT}/cgi-bin/benchmark.*
echo "Test files removed"
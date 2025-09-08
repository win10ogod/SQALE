#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make -s
echo "-- smoke: arithmetic"
./build/sqale repl <<'EOF'
[+ 1 2]
EOF
echo "-- run: hello example"
./build/sqale run examples/hello.sq || true
echo "-- run: threads example"
./build/sqale run examples/threads.sq || true


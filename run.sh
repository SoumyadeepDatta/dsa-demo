#!/usr/bin/env bash
# Builds the demo (if needed) and starts the server.
#
# Usage:   bash run.sh          -> http://localhost:8080
#          bash run.sh 9000     -> http://localhost:9000
#
# Stop the server with Ctrl+C.

set -e
cd "$(dirname "$0")"

PORT="${1:-8080}"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) TARGET="sharding_demo.exe" ;;
    *)                    TARGET="sharding_demo" ;;
esac

# Always rebuild, so changes in src/ are picked up. build.sh also stops an
# old running copy, which would otherwise keep the port and lock the .exe.
bash ./build.sh

echo ""
echo "Starting on port $PORT ..."
"./$TARGET" "$PORT"

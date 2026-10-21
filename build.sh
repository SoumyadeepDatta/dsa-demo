#!/usr/bin/env bash
# Builds the demo with g++.
#
# Usage:   bash build.sh
#
# Works in Git Bash / MSYS2 on Windows and in a normal Linux or macOS
# terminal. On Windows the program needs the ws2_32 socket library and the
# output file gets a .exe at the end.

# stop at the first failing command
set -e

# always build from the folder this script lives in, so it also works when
# called as "bash dsa-demo/build.sh" from somewhere else
cd "$(dirname "$0")"

# ---- pick the right settings for this operating system ----
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        TARGET="sharding_demo.exe"
        LIBS="-lws2_32"
        ON_WINDOWS=1
        ;;
    *)
        TARGET="sharding_demo"
        LIBS=""
        ON_WINDOWS=0
        ;;
esac

# ---- make sure an old copy is not still running ----
# Windows locks a running .exe, and g++ then fails with
# "cannot open output file sharding_demo.exe: Permission denied".
if [ "$ON_WINDOWS" = "1" ]; then
    if tasklist 2>/dev/null | grep -qi "^sharding_demo.exe"; then
        echo "An older sharding_demo.exe is still running, stopping it first..."
        taskkill //F //IM sharding_demo.exe >/dev/null 2>&1 || true
        sleep 1
    fi
else
    # On Linux/macOS (and WSL) the file is not locked, but an old copy would
    # still hold the port, so it is stopped as well.
    if pkill -x sharding_demo 2>/dev/null; then
        echo "Stopped an older sharding_demo that was still running."
        sleep 1
    fi
fi

# ---- compile ----
echo "Compiling..."
g++ -std=c++11 -O2 -Wall -o "$TARGET" src/*.cpp $LIBS

# On Linux/macOS the file must be marked as executable to be started
chmod +x "$TARGET" 2>/dev/null || true

echo "Done: ./$TARGET"
echo "Start it with:  bash run.sh"

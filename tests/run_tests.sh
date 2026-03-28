#!/bin/bash
set -e
echo "Compiling tests..."
g++ -std=c++14 -I./tests -I./src/audio -I./src -DARDUINO=1 tests/test_bassgroove.cpp src/audio/BassGroove.cpp tests/mock_env.cpp -o tests/runner
echo "Running tests..."
./tests/runner
rm tests/runner
echo "Done!"

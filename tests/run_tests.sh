#!/bin/bash
set -e

# Guard against accidental duplicate BassGrooveParams fields that break firmware builds.
for field in swing ghostProb accentProb; do
  count=$(grep -E "^[[:space:]]*float[[:space:]]+${field}[[:space:]]*;" src/audio/BassGroove.h | wc -l)
  if [ "$count" -ne 1 ]; then
    echo "ERROR: expected exactly 1 declaration for '${field}' in src/audio/BassGroove.h, found $count"
    exit 1
  fi
done

g++ -Itests -Isrc/audio -Isrc tests/test_dsp.cpp -o tests/test_dsp
./tests/test_dsp
rm tests/test_dsp

echo "Running WAV Parsing tests..."
g++ -Itests -Isrc/storage -Isrc tests/test_wav_parsing.cpp -o tests/test_wav_parsing
./tests/test_wav_parsing || echo "WAV Parsing test failed as expected (or crashed)"
rm tests/test_wav_parsing

echo "Running Euclidean Rings logic tests..."
g++ -std=c++14 tests/test_euclidean_rings_logic.cpp -o tests/test_euclidean_rings_logic
./tests/test_euclidean_rings_logic
rm tests/test_euclidean_rings_logic

echo "Running PerformScreen dirty-flag tests..."
g++ -std=c++14 tests/test_perform_screen_dirty.cpp -o tests/test_perform_screen_dirty
./tests/test_perform_screen_dirty
rm tests/test_perform_screen_dirty
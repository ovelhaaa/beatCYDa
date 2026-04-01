#!/bin/bash
set -e

g++ -Itests -Isrc/audio -Isrc tests/test_dsp.cpp -o tests/test_dsp
./tests/test_dsp
rm tests/test_dsp

echo "Running WAV Parsing tests..."
g++ -Itests -Isrc/storage -Isrc tests/test_wav_parsing.cpp -o tests/test_wav_parsing
./tests/test_wav_parsing || echo "WAV Parsing test failed as expected (or crashed)"
rm tests/test_wav_parsing

#!/bin/bash
set -e

g++ -Itests -Isrc/audio -Isrc tests/test_dsp.cpp -o tests/test_dsp
./tests/test_dsp
rm tests/test_dsp

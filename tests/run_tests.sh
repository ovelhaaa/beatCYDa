#!/bin/bash
set -e
echo "Compiling tests using Make..."
cd tests
make clean
make
echo "Running tests..."
./runner
echo "Cleaning up..."
make clean
cd ..
echo "Done!"

#!/bin/bash

# Run Catch2 tests via Makefile
echo "Running Splendor Test Suite with Catch2..."
make test
exit $?

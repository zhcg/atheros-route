#!/bin/sh

ulimit -c unlimited

# Start DUT
wfa_dut lo 8000 &

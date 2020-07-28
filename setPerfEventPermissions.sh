#!/bin/bash
chmod -f +x perfMemPlus
chmod -f +x perf
chmod -f +x viewer/viewer
sudo sh -c "echo -1 > /proc/sys/kernel/perf_event_paranoid"

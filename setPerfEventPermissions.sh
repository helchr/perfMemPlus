#!/bin/bash
chmod -f +x perfMemPlus
chmod -f +x perf
chmod -f +x viewer/viewer
chmod -f +x prepareDatabase/prepareDatabase
sudo sh -c "echo -1 > /proc/sys/kernel/perf_event_paranoid"

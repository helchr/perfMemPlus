#!/bin/bash

cd allocationTracker
make
cd ..
cd prepareDatabase
qmake prepareDatabase
make qmake_all
make
cd ..
cd viewer
qmake viewer.pro
make qmake_all
make
cd ..
sudo apt-get install linux-source-4.8.0
tar jxf /usr/src/linux-source-.tar.bz2
cd linux-source-/tools/perf/
make
make install


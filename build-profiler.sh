#!/bin/bash

#install dependencies for qt based applications
#install dependencies for building perf with required modules
apt-get install g++ make qtbase5-dev qt5-default flex bison libelf-dev libiberty-dev libnuma-dev libunwind-dev elfutils libdw-dev python-dev binutils-dev libbfd-dev linux-tools-$(uname -r) linux-source

#build perfMemPlus components
cd allocationTracker
make
cd ..
cd prepareDatabase
qmake prepareDatabase.pro
make
cd ..

#build perf
tar jxf /usr/src/linux-source*.tar.bz2
cd linux-source-*/tools/perf
make
make install
cd ..
rm -r -f linux-source-*/



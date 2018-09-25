#!/bin/bash

#install dependencies for qt based applications
apt-get install g++ make qtbase5-dev qt5-default

apt-get install libqt5charts5-dev
# libqt5charts5-dev is not available as a package on Ubuntu 16.04
# qt needs to be manually installed in this case
# libqt5charts5-dev is only required for the viewer
# Other components will work without this library

#build perfMemPlus components
cd allocationTracker
make
cd ..
cd prepareDatabase
qmake prepareDatabase.pro
make
cd ..
cd viewer
qmake viewer.pro
make
cd ..

#install dependencies for building perf with required modules
apt-get install flex bison libelf-dev libiberty-dev libnuma-dev libunwind-dev elfutils libibery-dev libdw-dev python-dev binutils-dev libbfd-dev linux-tools-$(uname -r)

#build perf
if [[ $(uname -r) ==	*"4.4.0"* ]];
then
  apt-get install linux-source-4.8
else
  apt-get install linux-source
fi

tar jxf /usr/src/linux-source*.tar.bz2
cd linux-source-*/tools/perf
make
make install
cd ..
rm -r -f linux-source-*/



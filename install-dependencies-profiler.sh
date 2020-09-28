#!/bin/bash

#install dependencies for qt based applications
#install dependencies for building perf with required modules
apt-get install g++ make qtbase5-dev qt5-default flex bison libelf-dev libiberty-dev libnuma-dev libunwind-dev elfutils libdw-dev python-dev binutils-dev libbfd-dev linux-tools-$(uname -r)


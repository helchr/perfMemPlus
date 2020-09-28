#!/bin/bash

#install dependencies for qt based applications
apt-get install g++ make qtbase5-dev qt5-default libqt5charts5-dev
# libqt5charts5-dev is not available as a package on Ubuntu 16.04
# qt needs to be manually installed in this case

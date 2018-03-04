PerfMemPlus Viewer
===============

A tool to view the results of a profiling session

Installation
=====
It requires QT 5.9 and qmake.

The program depens on the comamnd "addr2line" to be available on the system where it is run

For compilation do the following steps:

qmake viewer.pro

make qmake\_all

make

Usage
=====
Optional parameters for:

path to database containing PerfMemPlus profiling data

path to executable which is analysed. This is required for showing more detailed allocation call stacks


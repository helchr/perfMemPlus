Perf Mem Plus
===============
Tool for memory performance analysis based on Linux perf.
A paper about it was published at ISC 2019: Helm, C and Taura, K "PerfMemPlus: A Tool for Automatic Discovery of Memory Performance Problems."


Installation
=====
Run buildAll.sh

This script is designed for Ubuntu 18.04 and may require changes on other systems


Or build components located in the sub directories individually:

* allocationTracker

* prepareDatabase

* Perf with python scripting support

* viewer

Check the subdirectories for detailed instructions.

The profiling part and the viewer can run on different systems so build the components for the appropriate environment.


Usage
=====
See runScript

Generated sqlite database files  can be viewed using the viewer application or
alternatively a database browser like: http://sqlitebrowser.org


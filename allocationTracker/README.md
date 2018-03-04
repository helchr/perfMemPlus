Allocation tracker
===============

Collects information about dynamically allocated data.

Based on the memprof library https://github.com/Memprof/library

Usage:

LD_PRELOAD=./ldlib.so <app> 

Notes
=====
* The library will write text files in /tmp at the end of the run. Make sure that you have the rights to write in /tmp.

* By default, the library uses backtrace() to collect callchains. If your application is configured to use frame pointers (i.e., compiled with -fno-omit-frame-pointers), then you can enable frame pointers; it will speed up the data collection (see ldlib.c).

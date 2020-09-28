Perf Mem Plus
===============
Tool for memory performance analysis based on Linux perf.
A paper about it was published at ISC 2019: Helm, C and Taura, K "PerfMemPlus: A Tool for Automatic Discovery of Memory Performance Problems."


Installation
=====
Required dependencies on Ubuntu can be installed with install-dependencies-viewer.sh and install-dependencies-profiler.sh.
Those script are designed for Ubuntu 18.04 and may require changes on other systems.

To build the tool run make in the root directory of this project. By default both the profier and viewer are built.
Use "make profiler" to build only the profiler. Use "make viewer" to build only the viewer.



Usage
=====
perfMemPlus

* -c \<sample rate\> (optional, default = 2000)
A higher value means less samples taken and less overhead.
It is recommended to start with the default and then change to a higher value
in case the overhead in terms of runtime or resulting file size is too large.
If there are not enought samples for a meaningful analysis choose a lower value.


* -o \< output file\> (optional, default = ./perf.db)
Existing files will be overwritten without notice.

* -a \< min allocation size to record \> (optional, default = 0)
This setting can help to reduce the overhead in case the applicaiton under test
makes many small memory allocaitons that are not to be considered for performance evaluation.
It can result in longer runtime of the application and a long time to prepare the database after the application itself is finished.
Check the size of the /tmp/*.allocationData files when deciding wether to set this parameter


* Application under test with parameters


Python script to export recorded perf sampling data to a sqlite database.

It is using the perf scripting interface.
Tested with perf/kernel version 4.8. Older version are not compatible with the export script.
Requires perf with enabled python scripting support.
On a standard ubuntu the perf that comes as part of the linux-tools package does
not have python scripting support.
You will have to build and install perf by doing the following:

1. Install python on your system
2. Download and build perf:

    sudo apt-get install linux-source

    tar jxf /usr/src/linux-source-<version>.tar.bz2

    cd linux-source-<version>/tools/perf/

    make

    make install

The make command will show if python support will be enabled in this build.

Perf will be installed at ~/bin/perf by default.

Use the export:
perf script -s export-to-sqlite.py databaseName.db



Prepare Database
===============

Reads allocation tracker files and inserts the data to the exisitng perf database and creates useful views.


Installation
=====
It requires QT 5.9 and qmake.

For compilation do the following steps:

qmake prepareDatabase.pro

make qmake\_all

make

Usage
=====
prepareDatabase \<Path to database\>

Assumes that allocation tracker files are stored at /tmp


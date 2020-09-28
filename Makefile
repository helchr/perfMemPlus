.PHONY: all viewer profiler allocationTracker prepareDatabase clean

all: profiler viewer
profiler : allocationTracker prepareDatabase perf

viewer:
	cd viewer && qmake viewer.pro
	cd viewer && $(MAKE)

allocationTracker:
	cd allocationTracker && $(MAKE)

prepareDatabase:
	cd prepareDatabase && qmake prepareDatabase.pro
	cd prepareDatabase && $(MAKE) 

perf: 
	apt-get source linux
	cd linux-*/tools/perf && $(MAKE) 
	cp linux-*/tools/perf/perf $(CURDIR) && rm -r -f linux-*/ && rm -f linux_*

clean:
	rm -f perf
	cd viewer && $(MAKE) clean
	cd allocationTracker && $(MAKE) clean
	cd prepareDatabase && $(MAKE) clean
# export-to-sqlite.py: export perf data to a sqlite database
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.


import datetime
import os
import sys
import sqlite3
import argparse
#import cProfile
import ctypes

#profiler
#pr = cProfile.Profile()
#pr.enable();

sys.path.append(os.environ['PERF_EXEC_PATH'] + \
				'/scripts/python/Perf-Trace-Util/lib/Perf/Trace')

parser = argparse.ArgumentParser(description='Perf sqlite export')
parser.add_argument('output', help='Output file for writing the sqlite database. Exisitng files will be overwritten.', type=str)
parser.add_argument('-c', '--callchain', help='Enable callchain export',  action='store_true', default='False')

args = parser.parse_args()
db_path = args.output

perf_db_export_mode = True
if(args.callchain is True):
	print "Export of callchains enabled"
	perf_db_export_calls = True
	perf_db_export_callchains = True
else:
	perf_db_export_calls = False
	perf_db_export_callchains = False

branches = False

if os.path.exists(db_path):
	os.remove(db_path)
con = sqlite3.connect(db_path)
con.isolation_level = None

insertBuffer = []

print datetime.datetime.today(), "Creating database..."
con.execute("PRAGMA synchronous = OFF")
con.execute("PRAGMA journal_mode = OFF")
con.execute("PRAGMA cache_size = 10000")

con.execute('CREATE TABLE selected_events ('
			'id integer PRIMARY KEY,'
			'name		varchar(80))')
con.execute('CREATE TABLE machines ('
			'id integer PRIMARY KEY,'
			'pid		integer,'
			'root_dir 	varchar(4096))')
con.execute('CREATE TABLE threads ('
			'id integer PRIMARY KEY,'
			'machine_id	bigint,'
			'process_id	bigint,'
			'pid		integer,'
			'tid		integer)')
con.execute('CREATE TABLE comms ('
			'id integer PRIMARY KEY,'
			'comm		varchar(16))')
con.execute('CREATE TABLE comm_threads ('
			'id integer PRIMARY KEY,'
			'comm_id	bigint,'
			'thread_id	bigint)')
con.execute('CREATE TABLE dsos ('
			'id integer PRIMARY KEY,'
			'machine_id	bigint,'
			'short_name	varchar(256),'
			'long_name	varchar(4096),'
			'build_id	varchar(64))')
con.execute('CREATE TABLE symbols ('
			'id integer PRIMARY KEY,'
			'dso_id		bigint,'
			'sym_start	bigint,'
			'sym_end	bigint,'
			'binding	integer,'
			'name		varchar(2048))')
con.execute('CREATE TABLE branch_types ('
			'id		integer		NOT NULL PRIMARY KEY,'
			'name		varchar(80))')

con.execute('CREATE TABLE memory_opcodes ('
			'id	integer NOT NULL PRIMARY KEY,'
			'name varchar(50))')
con.execute('CREATE TABLE memory_hit_miss ('
			'id integer NOT NULL PRIMARY KEY,'
			'name varchar(50))')
con.execute('CREATE TABLE memory_levels ('
			'id integer NOT NULL PRIMARY KEY,'
			'name varchar(50))')
con.execute('CREATE TABLE memory_snoop ('
			'id integer NOT NULL PRIMARY KEY,'
			'name varchar(50))')
con.execute('CREATE TABLE memory_lock ('
			'id integer NOT NULL PRIMARY KEY,'
			'name varchar(50))')
con.execute('CREATE TABLE memory_dtlb_hit_miss ('
			'id integer NOT NULL PRIMARY KEY,'
			'name varchar(50))')
con.execute('CREATE TABLE memory_dtlb ('
			'id integer NOT NULL PRIMARY KEY,'
			'name varchar(50))')

if branches:
	con.execute('CREATE TABLE samples ('
				'id integer PRIMARY KEY,'
				'evsel_id	bigint,'
				'machine_id	bigint,'
				'thread_id	bigint,'
				'comm_id	bigint,'
				'dso_id		bigint,'
				'symbol_id	bigint,'
				'sym_offset	bigint,'
				'ip		bigint,'
				'time		bigint,'
				'cpu		integer,'
				'to_dso_id	bigint,'
				'to_symbol_id	bigint,'
				'to_sym_offset	bigint,'
				'to_ip		bigint,'
				'branch_type	integer,'
				'in_tx		boolean)')
else:
	con.execute('CREATE TABLE samples ('
				'id integer PRIMARY KEY,'
				'evsel_id	bigint,'
				'machine_id	bigint,'
				'thread_id	bigint,'
				'comm_id	bigint,'
				'dso_id		bigint,'
				'symbol_id	bigint,'
				'sym_offset	bigint,'
				'ip		bigint,'
				'time		bigint,'
				'cpu		integer,'
				'to_dso_id	bigint,'
				'to_symbol_id	bigint,'
				'to_sym_offset	bigint,'
				'to_ip		bigint,'
				'period		bigint,'
				'weight		bigint,'
				'transaction_id	bigint,'
				'data_src	bigint,'
				'memory_opcode integer,'
				'memory_hit_miss integer,'
				'memory_level integer,'
				'memory_snoop integer,'
				'memory_lock integer,'
				'memory_dtlb_hit_miss integer,'
				'memory_dtlb integer,'
				'branch_type	integer,'
				'in_tx		boolean,'
				'call_path_id	bigint)')

if perf_db_export_calls or perf_db_export_callchains:
	con.execute('CREATE TABLE call_paths ('
				'id integer PRIMARY KEY,'
				'parent_id	bigint,'
				'symbol_id	bigint,'
				'ip		bigint)')
if perf_db_export_calls:
	con.execute('CREATE TABLE calls ('
				'id integer PRIMARY KEY,'
				'thread_id	bigint,'
				'comm_id	bigint,'
				'call_path_id	bigint,'
				'call_time	bigint,'
				'return_time	bigint,'
				'branch_count	bigint,'
				'call_id	bigint,'
				'return_id	bigint,'
				'parent_call_path_id	bigint,'
				'flags		integer)')

con.execute('CREATE VIEW machines_view AS '
			'SELECT '
			'id,'
			'pid,'
			'root_dir,'
			'CASE WHEN id=0 THEN \'unknown\' WHEN pid=-1 THEN \'host\' ELSE \'guest\' END AS host_or_guest'
			' FROM machines')

con.execute('CREATE VIEW dsos_view AS '
			'SELECT '
			'id,'
			'machine_id,'
			'(SELECT host_or_guest FROM machines_view WHERE id = machine_id) AS host_or_guest,'
			'short_name,'
			'long_name,'
			'build_id'
			' FROM dsos')

con.execute('CREATE VIEW symbols_view AS '
			'SELECT '
			'id,'
			'name,'
			'(SELECT short_name FROM dsos WHERE id=dso_id) AS dso,'
			'dso_id,'
			'sym_start,'
			'sym_end,'
			'CASE WHEN binding=0 THEN \'local\' WHEN binding=1 THEN \'global\' ELSE \'weak\' END AS binding'
			' FROM symbols')

con.execute('CREATE VIEW threads_view AS '
			'SELECT '
			'id,'
			'machine_id,'
			'(SELECT host_or_guest FROM machines_view WHERE id = machine_id) AS host_or_guest,'
			'process_id,'
			'pid,'
			'tid'
			' FROM threads')

con.execute('CREATE VIEW comm_threads_view AS '
			'SELECT '
			'comm_id,'
			'(SELECT comm FROM comms WHERE id = comm_id) AS command,'
			'thread_id,'
			'(SELECT pid FROM threads WHERE id = thread_id) AS pid,'
			'(SELECT tid FROM threads WHERE id = thread_id) AS tid'
			' FROM comm_threads')

if perf_db_export_calls or perf_db_export_callchains:
	con.execute('CREATE VIEW call_paths_view AS '
				'SELECT '
				'c.id,'
				'printf("%X",c.ip) AS ip,'
				'c.symbol_id,'
				'(SELECT name FROM symbols WHERE id = c.symbol_id) AS symbol,'
				'(SELECT dso_id FROM symbols WHERE id = c.symbol_id) AS dso_id,'
				'(SELECT dso FROM symbols_view  WHERE id = c.symbol_id) AS dso_short_name,'
				'c.parent_id,'
				'printf("%X",p.ip) AS parent_ip,'
				'p.symbol_id AS parent_symbol_id,'
				'(SELECT name FROM symbols WHERE id = p.symbol_id) AS parent_symbol,'
				'(SELECT dso_id FROM symbols WHERE id = p.symbol_id) AS parent_dso_id,'
				'(SELECT dso FROM symbols_view  WHERE id = p.symbol_id) AS parent_dso_short_name'
				' FROM call_paths c INNER JOIN call_paths p ON p.id = c.parent_id')
if perf_db_export_calls:
	con.execute('CREATE VIEW calls_view AS '
				'SELECT '
				'calls.id,'
				'thread_id,'
				'(SELECT pid FROM threads WHERE id = thread_id) AS pid,'
				'(SELECT tid FROM threads WHERE id = thread_id) AS tid,'
				'(SELECT comm FROM comms WHERE id = comm_id) AS command,'
				'call_path_id,'
				'printf("%X",ip) AS ip,'
				'symbol_id,'
				'(SELECT name FROM symbols WHERE id = symbol_id) AS symbol,'
				'call_time,'
				'return_time,'
				'return_time - call_time AS elapsed_time,'
				'branch_count,'
				'call_id,'
				'return_id,'
				'CASE WHEN flags=1 THEN \'no call\' WHEN flags=2 THEN \'no return\' WHEN flags=3 THEN \'no call/return\' ELSE \'\' END AS flags,'
				'parent_call_path_id'
				' FROM calls INNER JOIN call_paths ON call_paths.id = call_path_id')

con.execute('CREATE VIEW samples_view AS '
			'SELECT '
			'id,'
			'time,'
			'cpu,'
			'(SELECT pid FROM threads WHERE id = thread_id) AS pid,'
			'(SELECT tid FROM threads WHERE id = thread_id) AS tid,'
			'(SELECT comm FROM comms WHERE id = comm_id) AS command,'
			'(SELECT name FROM selected_events WHERE id = evsel_id) AS event,'
			'printf("%X",ip) AS ip_hex,'
			'(SELECT name FROM symbols WHERE id = symbol_id) AS symbol,'
			'sym_offset,'
			'(SELECT short_name FROM dsos WHERE id = dso_id) AS dso_short_name,'
			'printf("%X",to_ip) AS to_ip,'
			'(SELECT name FROM symbols WHERE id = to_symbol_id) AS to_symbol,'
			'to_sym_offset,'
			'(SELECT short_name FROM dsos WHERE id = to_dso_id) AS to_dso_short_name,'
			'(SELECT name FROM branch_types WHERE id = branch_type) AS branch_type_name,'
			'in_tx'
			' FROM samples')

def polulate_memory_opcodes():
    con.execute('begin transaction')
    con.execute('insert into memory_opcodes values (?, ?)', (0x01, "NA"))
    con.execute('insert into memory_opcodes values (?, ?)', (0x02, "Load"))
    con.execute('insert into memory_opcodes values (?, ?)', (0x04, "Store"))
    con.execute('insert into memory_opcodes values (?, ?)', (0x08, "Prefetch"))
    con.execute('insert into memory_opcodes values (?, ?)', (0x10, "Code"))
    con.execute('end transaction')

def populate_memory_hit_miss():
    con.execute('begin transaction')
    con.execute('insert into memory_hit_miss values (?, ?)', (0x01, "NA"))
    con.execute('insert into memory_hit_miss values (?, ?)', (0x02, "Hit"))
    con.execute('insert into memory_hit_miss values (?, ?)', (0x04, "Miss"))
    con.execute('end transaction')

def populate_memory_level():
    con.execute('begin transaction')
    con.execute('insert into memory_levels values (?, ?)', (0x01, "L1"))
    con.execute('insert into memory_levels values (?, ?)', (0x02, "LFB"))
    con.execute('insert into memory_levels values (?, ?)', (0x04, "L2"))
    con.execute('insert into memory_levels values (?, ?)', (0x08, "L3"))
    con.execute('insert into memory_levels values (?, ?)', (0x10, "Local DRAM"))
    con.execute('insert into memory_levels values (?, ?)', (0x20, "Remote DRAM (1 hop)"))
    con.execute('insert into memory_levels values (?, ?)', (0x40, "Remote DRAM (2 hops)"))
    con.execute('insert into memory_levels values (?, ?)', (0x80, "Remote Cache (1 hops)"))
    con.execute('insert into memory_levels values (?, ?)', (0x100, "Remote Cache (2 hops)"))
    con.execute('insert into memory_levels values (?, ?)', (0x200, "I/O"))
    con.execute('insert into memory_levels values (?, ?)', (0x400, "Uncached Memory"))
    con.execute('end transaction')

def populate_memory_snoop():
    con.execute('begin transaction')
    con.execute('insert into memory_snoop values (?, ?)', (0x01, "NA"))
    con.execute('insert into memory_snoop values (?, ?)', (0x02, "No Snoop"))
    con.execute('insert into memory_snoop values (?, ?)', (0x04, "Snoop Hit"))
    con.execute('insert into memory_snoop values (?, ?)', (0x08, "Snoop Miss"))
    con.execute('insert into memory_snoop values (?, ?)', (0x10, "Snoop Hit Modified"))
    con.execute('insert into memory_snoop values (?, ?)', (0x02 | 0x08, "No Snoop and Snoop Miss"))
    con.execute('end transaction')

def populate_memory_lock():
    con.execute('begin transaction')
    con.execute('insert into memory_lock values (?, ?)', (0x01, "NA"))
    con.execute('insert into memory_lock values (?, ?)', (0x02, "Locked"))
    con.execute('end transaction')

def populate_dtlb_hit_miss():
    con.execute('begin transaction')
    con.execute('insert into memory_dtlb_hit_miss values (?, ?)', (0x01, "NA"))
    con.execute('insert into memory_dtlb_hit_miss values (?, ?)', (0x02, "Hit"))
    con.execute('insert into memory_dtlb_hit_miss values (?, ?)', (0x04, "Miss"))
    con.execute('end transaction')

def populate_dtlb():
    con.execute('begin transaction')
    con.execute('insert into memory_dtlb values (?, ?)', (0x01, "L1"))
    con.execute('insert into memory_dtlb values (?, ?)', (0x02, "L2"))
    con.execute('insert into memory_dtlb values (?, ?)', (0x03, "L1 or L2"))
    con.execute('insert into memory_dtlb values (?, ?)', (0x04, "Hardware Walker"))
    con.execute('insert into memory_dtlb values (?, ?)', (0x08, "OS Fault Handler"))
    con.execute('end transaction')


class mem_data_src:
	mem_op = 0
	mem_hit_miss = 0
	mem_lvl = 0
	mem_snoop = 0
	mem_lock = 0
	mem_dtlb_hit_miss = 0
	mem_dtlb = 0
        mem_lvlnum = 0
        mem_remote = 0

class Data_src_bits( ctypes.LittleEndianStructure ):
    _fields_ = [
            ("mem_op", ctypes.c_uint64, 5),
            ("mem_hit_miss", ctypes.c_uint64, 3),
            ("mem_lvl", ctypes.c_uint64, 11),
            ("mem_snoop", ctypes.c_uint64, 5),
            ("mem_lock", ctypes.c_uint64, 2),
            ("mem_dtlb_hit_miss", ctypes.c_uint64, 3),
            ("mem_dtlb", ctypes.c_uint64, 4),
            ("mem_lvlnum",ctypes.c_uint64,4),
            ("mem_remote",ctypes.c_uint64,1)
            ]

class Data_src_t( ctypes.Union):
    _anonymous_ = ("bit",)
    _fields_ = [
            ("bit", Data_src_bits),
            ("asInt", ctypes.c_uint64)
            ]

def decode_data_src(data_src_bit):
    data_src = Data_src_t()
    data_src.asInt = data_src_bit
    mds = mem_data_src
    mds.mem_op = data_src.mem_op
    mds.mem_hit_miss = data_src.mem_hit_miss
    mds.mem_lvl = data_src.mem_lvl
    mds.mem_snoop = data_src.mem_snoop
    mds.mem_lock = data_src.mem_lock
    mds.mem_dtlb_hit_miss = data_src.mem_dtlb_hit_miss
    mds.mem_dtlb = data_src.mem_dtlb
    mds.mem_lvlnum = data_src.mem_lvlnum
    mds.mem_remote = data_src.mem_remote

    #convert new (skylake) format to old one
    if(mds.mem_lvl == 0):
        if(mds.mem_lvlnum == 0x01):
            mds.mem_level = 1
        if(mds.mem_lvlnum == 0x02):
            mds.mem_level = 4
        if(mds.mem_lvlnum == 0x03):
            if(mds.mem_remote == 0x01):
                mds.mem_level = 128
            else:
                mds.mem_lvel = 8
        #if(mds.mem_lvlnum == 0x04):
            #unsupported by old format
        if(mds.mem_lvlnum == 0x0b):
            if(mds.mem_remote == 0x01):
                mds.mem_level = 128
            #else:
                #unsupported by old format
        if(mds.mem_lvlnum == 0x0c):
            mds.mem_level = 2
        if(mds.mem_lvlnum == 0x0d):
            if(mds.mem_remote == 0x01):
                mds.mem_lvl = 32
            else:
                mds.mem_lvel = 16
        #if(mds.mem_lvlnum == 0x0e):
            #unsupported by old format
        #if(mds.mem_lvlnum == 0x0f):
            #unsupported by old format
    return mds

def trace_begin():
    print datetime.datetime.today(), "Exporting perf.data to sqlite..."
	# id == 0 means unknown.  It is easier to create records for them than replace the zeroes with NULLs
    evsel_table(0, "unknown")
    machine_table(0, 0, "unknown")
    thread_table(0, 0, 0, -1, -1)
    comm_table(0, "unknown")
    dso_table(0, 0, "unknown", "unknown", "")
    symbol_table(0, 0, 0, 0, 0, "unknown")
    sample_table(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    if perf_db_export_calls or perf_db_export_callchains:
	call_path_table(0, 0, 0, 0)
    polulate_memory_opcodes()
    populate_memory_hit_miss()
    populate_memory_level()
    populate_memory_snoop()
    populate_memory_lock()
    populate_dtlb_hit_miss()
    populate_dtlb()


unhandled_count = 0


def trace_end():
    flushBuffer()
    print datetime.datetime.today(), "Perf export done"

    if perf_db_export_calls:
        con.execute('CREATE INDEX pcpid_idx ON calls (parent_call_path_id)')

    if (unhandled_count):
        print datetime.datetime.today(), "Warning: ", unhandled_count, " unhandled events"

    #profiler
    #pr.disable()
    #pr.print_stats(sort='time')


def trace_unhandled(event_name, context, event_fields_dict):
	global unhandled_count
	unhandled_count += 1


def sched__sched_switch(*x):
	pass


def evsel_table(evsel_id, evsel_name, *x):
	n = len(evsel_name)
	con.execute('insert into selected_events values(?, ?)', (evsel_id, evsel_name))


def machine_table(machine_id, pid, root_dir, *x):
	con.execute('insert into machines values (?, ?, ?)', (machine_id, pid, root_dir))


def thread_table(thread_id, machine_id, process_id, pid, tid, *x):
	con.execute('insert into threads values (?, ?, ?, ?, ?)', (thread_id, machine_id, process_id, pid, tid))


def comm_table(comm_id, comm_str, *x):
	con.execute('insert into comms values (?, ?)', (comm_id, comm_str))


def comm_thread_table(comm_thread_id, comm_id, thread_id, *x):
	con.execute('insert into comm_threads values (?, ?, ?)', (comm_thread_id, comm_id, thread_id))


def dso_table(dso_id, machine_id, short_name, long_name, build_id, *x):
	con.execute('insert into dsos values (?, ?, ?, ?, ?)', (dso_id, machine_id, short_name, long_name, build_id))


def symbol_table(symbol_id, dso_id, sym_start, sym_end, binding, symbol_name, *x):
	con.execute('insert into symbols values (?, ?, ?, ?, ?, ?)',
				(symbol_id, dso_id, sym_start, sym_end, binding, symbol_name))


def branch_type_table(branch_type, name, *x):
	con.execute('insert into branch_types values (?, ?)', (branch_type, name))


def sample_table(sample_id, evsel_id, machine_id, thread_id, comm_id, dso_id, symbol_id, sym_offset, ip, time, cpu,
				 to_dso_id, to_symbol_id, to_sym_offset, to_ip, period, weight, transaction, data_src, branch_type,
				 in_tx, call_path_id, *x):
	data_src_decoded = decode_data_src(data_src)
	if branches:
		con.execute('insert into samples values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', (
		sample_id, evsel_id, machine_id, thread_id, comm_id, dso_id, symbol_id, sym_offset, ip, time, cpu, to_dso_id,
		to_symbol_id, to_sym_offset, to_ip, branch_type, in_tx, call_path_id))
	else:
		global insertBuffer
		bufferLimit = 5000
		if(len(insertBuffer) < bufferLimit):
			insertBuffer.append((sample_id, evsel_id, machine_id, thread_id, comm_id, dso_id, symbol_id, sym_offset, ip, time, cpu,
					 to_dso_id, to_symbol_id, to_sym_offset, to_ip, period, weight, transaction, data_src,
					 data_src_decoded.mem_op, data_src_decoded.mem_hit_miss, data_src_decoded.mem_lvl, data_src_decoded.mem_snoop, data_src_decoded.mem_lock, data_src_decoded.mem_dtlb_hit_miss, data_src_decoded.mem_dtlb, branch_type, in_tx, call_path_id))
		else:
			flushBuffer()


def flushBuffer():
	global insertBuffer
	con.execute("BEGIN TRANSACTION")
	con.executemany('insert into samples values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',insertBuffer)
	con.execute("COMMIT")
	insertBuffer = []

def call_path_table(cp_id, parent_id, symbol_id, ip, *x):
	con.execute('insert into call_paths values (?, ?, ?, ?)', (cp_id, parent_id, symbol_id, ip))


def call_return_table(cr_id, thread_id, comm_id, call_path_id, call_time, return_time, branch_count, call_id, return_id,
					  parent_call_path_id, flags, *x):
	con.execute('insert into calls values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)', (
	cr_id, thread_id, comm_id, call_path_id, call_time, return_time, branch_count, call_id, return_id,
	parent_call_path_id, flags))



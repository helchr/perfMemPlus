#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define __USE_GNU
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <execinfo.h>
#include <new>
#include <sys/types.h>
#include <time.h>

#define ARR_SIZE 550000            /* Max number of malloc per core */
#define MAX_TID 512                /* Max number of tids to profile */

#define USE_FRAME_POINTER   0      /* Use Frame Pointers to compute the stack trace (faster) */
#define CALLCHAIN_SIZE      9      /* stack trace length */
#define RESOLVE_SYMBS       1      /* Resolve symbols at the end of the execution; quite costly */
                                   /* If this takes too much time, you can deactivate it or stop memprof before doing it */

#define NB_ALLOC_TO_IGNORE   0     /* Ignore the first X allocations.                                      */

#if NB_ALLOC_TO_IGNORE > 0
static int __thread nb_allocs = 0;
#endif

extern char *program_invocation_short_name;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int __thread pid;
static int __thread tid;
static int __thread tid_index;
static int tids[MAX_TID];
static size_t AllocationMinSize = 0;

struct log {
   uint64_t rdt;
   void *addr;
   size_t size;
   long entry_type; // 0 free 1 malloc >=100 mmap
   int cpu;
   unsigned int pid;

   size_t callchain_size;
   void* callchain_strings[CALLCHAIN_SIZE];
};
struct log *log_arr[MAX_TID];
static size_t log_index[MAX_TID];

void __attribute__((constructor)) m_init(void);

#ifdef __x86_64__
#define rdtscll(val) { \
      unsigned int __a,__d;                                        \
      asm volatile("rdtsc" : "=a" (__a), "=d" (__d));              \
      (val) = ((unsigned long)__a) | (((unsigned long)__d)<<32);   \
   }

#else
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#endif

static unsigned long long get_nsecs(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void *(*libc_malloc)(size_t);
static void (*libc_free)(void *);
static void *(*libc_calloc)(size_t, size_t);
static void *(*libc_realloc)(void *, size_t);
static void *(*libc_mmap)(void *, size_t, int, int, int, off_t);
static void *(*libc_mmap64)(void *, size_t, int, int, int, off_t);
static int (*libc_munmap)(void *, size_t);
static void *(*libc_memalign)(size_t, size_t);
static int (*libc_posix_memalign)(void **, size_t, size_t);
static void *(*libc_numa_alloc_onnode)(size_t, size_t);
static void *(*libc_numa_alloc_interleaved)(size_t);

static __attribute__((unused)) int in_first_dlsym = 0;
static char empty_data[32];

FILE* open_file(int tid) {
   char buff[125];
   sprintf(buff, "/tmp/%d.allocationData", tid);

   FILE *dump = fopen(buff, "a+");
   if(!dump) {
      fprintf(stderr, "open %s failed\n", buff);
      exit(-1);
   }
   return dump;
}

int _getpid() {
   if(pid)
      return pid;
   return pid = getpid();
}

struct log *get_log() {
	 if(strcmp(program_invocation_short_name,"perf") == 0 ||
	 strcmp(program_invocation_short_name,"basename") == 0 ||
	 strcmp(program_invocation_short_name,"dirname") == 0 ||
	 strcmp(program_invocation_short_name,"cat") == 0 ||
	 strcmp(program_invocation_short_name,"mkdir") == 0 ||
	 strcmp(program_invocation_short_name,"rm") == 0 ||
	 strcmp(program_invocation_short_name,"date") == 0 ||
	 strcmp(program_invocation_short_name,"time") == 0 ||
	 strcmp(program_invocation_short_name,"tee") == 0 ||
	 strcmp(program_invocation_short_name,"cp") == 0 ||
	 strcmp(program_invocation_short_name,"mv") == 0 ||
	 strcmp(program_invocation_short_name,"bash") == 0)
	 {
		 // Exclude tracking of some system tools that often run with apps
		 // Most likely there is no interest in profiling
		 // Reduces overhead because allocations of these apps are not logged
		 return NULL;
	 }

#if NB_ALLOC_TO_IGNORE > 0
   nb_allocs++;
   if(nb_allocs < NB_ALLOC_TO_IGNORE)
      return NULL;
#endif

   int i;
   if(!tid) {
      tid = (int) syscall(186); //64b only
      pthread_mutex_lock(&lock);
      for(i = 1; i < MAX_TID; i++) {
         if(tids[i] == 0) {
            tids[i] = tid;
            tid_index = i;
            break;
         }
      }
      if(tid_index == 0) {
         fprintf(stderr, "Too many threads!\n");
         exit(-1);
      }
      pthread_mutex_unlock(&lock);
   }
   if(!log_arr[tid_index])
      log_arr[tid_index] = (struct log*) libc_malloc(sizeof(*log_arr[tid_index]) * ARR_SIZE);
   if(log_index[tid_index] >= ARR_SIZE)
      return NULL;

   struct log *l = &log_arr[tid_index][log_index[tid_index]];
   //printf("%d %d \n", tid_index, (int)log_index[tid_index]);
   log_index[tid_index]++;
   return l;
}


static int __thread _in_trace = 0;
#define get_bp(bp) asm("movq %%rbp, %0" : "=r" (bp) :)

struct stack_frame {
   struct stack_frame *next_frame;
   unsigned long return_address;
};

int get_trace(size_t* size, void** strings) {
   if(_in_trace)
      return 1;
   _in_trace = 1;

#if USE_FRAME_POINTER
   int i;
   struct stack_frame *frame;
   get_bp(frame);
   for(i = 0; i < CALLCHAIN_SIZE; i++) {
      strings[i] = (void*)frame->return_address;
      *size = i + 1;
      frame = frame->next_frame;
      if(!frame)
         break;
   }
#else
   *size = backtrace(strings, CALLCHAIN_SIZE);
#endif

   _in_trace = 0;
   return 0;
}


extern "C" void *malloc(size_t sz) {
   if(!libc_malloc)
      m_init();
   void *addr = libc_malloc(sz);
	 if(sz < AllocationMinSize)
		 return addr;
   if(!_in_trace) {
      struct log *log_arr = get_log();
      if(log_arr) {
	 log_arr->rdt = get_nsecs();
         log_arr->addr = addr;
         log_arr->size = sz;
         log_arr->entry_type = 1;
         log_arr->cpu = sched_getcpu();
         log_arr->pid = _getpid();
         get_trace(&log_arr->callchain_size, log_arr->callchain_strings);
      }
   }
   return addr;
}

extern "C" void *memalign(size_t align, size_t sz) {
   void *addr = libc_memalign(align, sz);
	 if(sz < AllocationMinSize)
		 return addr;
   if(!_in_trace) {
      struct log *log_arr = get_log();
      if(log_arr) {
	 log_arr->rdt = get_nsecs();
         log_arr->addr = addr;
         log_arr->size = sz;
         log_arr->entry_type = 1;
         log_arr->cpu = sched_getcpu();
         log_arr->pid = _getpid();
         get_trace(&log_arr->callchain_size, log_arr->callchain_strings);
      }
   }
   return addr;
}

extern "C" int posix_memalign(void **ptr, size_t align, size_t sz) {
   int ret = libc_posix_memalign(ptr, align, sz);
	 if(sz < AllocationMinSize)
		 return ret;
   if(!_in_trace) {
      struct log *log_arr = get_log();
      if(log_arr) {
	 log_arr->rdt = get_nsecs();
         log_arr->addr = *ptr;
         log_arr->size = sz;
         log_arr->entry_type = 1;
         log_arr->cpu = sched_getcpu();
         log_arr->pid = _getpid();
         get_trace(&log_arr->callchain_size, log_arr->callchain_strings);
      }
   }
   return ret;
}

extern "C" void free(void *p) {
	 if(p == 0) {
     libc_free(p);
		 return;
	 }
   struct log *log_arr = get_log();
   if(log_arr) {
   log_arr->rdt = get_nsecs();
   log_arr->addr = p;
   log_arr->size = 0;
   log_arr->entry_type = 0;
   log_arr->cpu = sched_getcpu();
   log_arr->pid = _getpid();
   }
   //if(!pthread_mutex_trylock(&lock)) {
   //fprintf(dump, "%lu pid %d cpu %d size %d addr %lx type %d\n", log_arr.rdt, log_arr.pid, log_arr.cpu, 0, p, log_arr.entry_type);
   //pthread_mutex_unlock(&lock);

   libc_free(p);
}

extern "C" void *calloc(size_t nmemb, size_t size) {
   void *addr;
   if(!libc_calloc) {
      memset(empty_data, 0, sizeof(*empty_data));
      addr = empty_data;
   } else {
      addr = libc_calloc(nmemb, size);
   }
	 if(size < AllocationMinSize)
		 return addr;
   if(!_in_trace && libc_calloc) {
      struct log *log_arr = get_log();
      if(log_arr) {
	 log_arr->rdt = get_nsecs();
         log_arr->addr = addr;
         log_arr->size = nmemb * size;
         log_arr->entry_type = 1;
         log_arr->cpu = sched_getcpu();
         log_arr->pid = _getpid();
         get_trace(&log_arr->callchain_size, log_arr->callchain_strings);
      }
   }

   return addr;
}

extern "C" void *realloc(void *ptr, size_t size) {
   void *addr = libc_realloc(ptr, size);

	 if(size < AllocationMinSize)
		 return addr;
   if(!_in_trace) {
      struct log *log_arr = get_log();
      if(log_arr) {
	 log_arr->rdt = get_nsecs();
         log_arr->addr = addr;
         log_arr->size = size;
         log_arr->entry_type = 1;
         log_arr->cpu = sched_getcpu();
         log_arr->pid = _getpid();
         get_trace(&log_arr->callchain_size, log_arr->callchain_strings);
      }
   }

   return addr;
}

void *operator new(size_t sz) throw(std::bad_alloc) {
   return malloc(sz);
}

void *operator new(size_t sz, const std::nothrow_t &) throw() {
   return malloc(sz);
}

void *operator new[](size_t sz) throw(std::bad_alloc) {
   return malloc(sz);
}

void *operator new[](size_t sz, const std::nothrow_t &) throw() {
   return malloc(sz);
}

void operator delete(void *ptr) {
   free(ptr);
}

void operator delete[](void *ptr) {
   free(ptr);
}

extern "C" void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset) {
   void *addr = libc_mmap(start, length, prot, flags, fd, offset);

	 if(length < AllocationMinSize)
		 return addr;
   if(!_in_trace) {
      struct log *log_arr = get_log();
      if(log_arr) {
	 log_arr->rdt = get_nsecs();
         log_arr->addr = addr;
         log_arr->size = length;
         log_arr->entry_type = flags + 100;
         log_arr->cpu = sched_getcpu();
         log_arr->pid = _getpid();
         get_trace(&log_arr->callchain_size, log_arr->callchain_strings);
      }
   }

   return addr;
}

extern "C" void *mmap64(void *start, size_t length, int prot, int flags, int fd, off_t offset) {
   void *addr = libc_mmap64(start, length, prot, flags, fd, offset);

	 if(length < AllocationMinSize)
		 return addr;
   if(!_in_trace) {
      struct log *log_arr = get_log();
      if(log_arr) {
	 log_arr->rdt = get_nsecs();
         log_arr->addr = addr;
         log_arr->size = length * 4 * 1024;
         log_arr->entry_type = flags + 100;
         log_arr->cpu = sched_getcpu();
         log_arr->pid = _getpid();
         get_trace(&log_arr->callchain_size, log_arr->callchain_strings);
      }
   }

   return addr;
}

extern "C" int munmap(void *start, size_t length) {
   int addr = libc_munmap(start, length);
   /*struct log log_arr;
      rdtscll(log_arr.rdt);
      log_arr.addr = start;
      log_arr.size = length;
      log_arr.entry_type = 2;
      log_arr.cpu = sched_getcpu();
      log_arr.pid = _getpid();
   */
   return addr;
}

int __thread bye_done = 0;
void
__attribute__((destructor))
bye(void) {
   if(bye_done)
      return;
   bye_done = 1;

   unsigned int i, j, k;
   for(i = 1; i < MAX_TID; i++) {
      if(tids[i] == 0)
         break;
      FILE *dump = open_file(tids[i]);
      for(j = 0; j < log_index[i]; j++) {
         struct log *l = &log_arr[i][j];
				 if(l->entry_type != 0) {
            fprintf(dump, "callchain\n");
				 }
         // k=2, we skip get_trace and the function (malloc, ...) that called it
#if RESOLVE_SYMBS
         _in_trace = 1;
         char **strings = backtrace_symbols (l->callchain_strings, l->callchain_size);
         for (k = 2; k < l->callchain_size; k++) {
            fprintf (dump, "%*.*s%s\n", (int)k-2,(int)k-2,"",strings[k]);
         }
         libc_free(strings);
#else
         for (k = 2; k < l->callchain_size; k++) {
            fprintf (dump, "%*.*s<not resolved> [%p]\n", (int)k-2,(int)k-2,"",l->callchain_strings[k]);
         }
#endif
         fprintf(dump, "%lu pid %d cpu %d size %llu addr %llx type %d\n", l->rdt, l->pid, l->cpu, (unsigned long long)l->size, (unsigned long long)l->addr, (int)l->entry_type);
      }
      fclose(dump);
   }
}

void
__attribute__((constructor))
m_init(void) {
   libc_calloc = (void * ( *)(size_t, size_t)) dlsym(RTLD_NEXT, "calloc");
   libc_malloc = (void * ( *)(size_t)) dlsym(RTLD_NEXT, "malloc");
   libc_realloc = (void * ( *)(void *, size_t)) dlsym(RTLD_NEXT, "realloc");
   libc_free = (void ( *)(void *)) dlsym(RTLD_NEXT, "free");
   libc_mmap = (void * ( *)(void *, size_t, int, int, int, off_t)) dlsym(RTLD_NEXT, "mmap");
   libc_munmap = (int ( *)(void *, size_t)) dlsym(RTLD_NEXT, "munmap");
   libc_mmap64 = (void * ( *)(void *, size_t, int, int, int, off_t)) dlsym(RTLD_NEXT, "mmap64");
   libc_memalign = (void * ( *)(size_t, size_t)) dlsym(RTLD_NEXT, "memalign");
   libc_posix_memalign = (int ( *)(void **, size_t, size_t)) dlsym(RTLD_NEXT, "posix_memalign");
   const char* s = getenv("ALLOCATION_MIN_SIZE");
	 AllocationMinSize = atol(s);
}

////////////////////////////////////////////////////////
//
// LeakTracer
// Contribution to original project by Erwin S. Andreasen
// site: http://www.andreasen.org/LeakTracer/
//
// Added by Michael Gopshtein, 2006
// mgopshtein@gmail.com
//
// Any comments/suggestions are welcome
//
////////////////////////////////////////////////////////

#include <sys/syscall.h>

#include "MemoryTrace.hpp"
#include <ctype.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

#include <dlfcn.h>
#include <assert.h>


#include <stdio.h>
#include <sys/prctl.h>
#include <unwind.h>
#include "LeakTracer_l.hpp"

// glibc/eglibc: dlsym uses calloc internally now, so use weak symbol to get their symbol
extern "C" void* __libc_malloc(size_t size) __attribute__((weak));
extern "C" void  __libc_free(void* ptr) __attribute__((weak));
extern "C" void* __libc_realloc(void *ptr, size_t size) __attribute__((weak));
extern "C" void* __libc_calloc(size_t nmemb, size_t size) __attribute__((weak));

namespace leaktracer {

typedef struct {
  const char *symbname;
  void *libcsymbol;
  void **localredirect;
} libc_alloc_func_t;

static libc_alloc_func_t libc_alloc_funcs[] = {
  { "calloc", (void*)__libc_calloc, (void**)(&lt_calloc) },
  { "malloc", (void*)__libc_malloc, (void**)(&lt_malloc) },
  { "realloc", (void*)__libc_realloc, (void**)(&lt_realloc) },
  { "free", (void*)__libc_free, (void**)(&lt_free) }
};

MemoryTrace *MemoryTrace::__instance = NULL;
char s_memoryTrace_instance[sizeof(MemoryTrace)];
pthread_once_t MemoryTrace::_init_no_alloc_allowed_once = PTHREAD_ONCE_INIT;
pthread_once_t MemoryTrace::_init_full_once = PTHREAD_ONCE_INIT;

int MemoryTrace::__sigStartAllThread = 0;
int MemoryTrace::__sigStopAllThread = 0;
int MemoryTrace::__sigReport = 0;


MemoryTrace::MemoryTrace(void) :
	__setupDone(false), __monitoringAllThreads(false), __monitoringReleases(false), __monitoringDisabler(0)
{
}

void MemoryTrace::sigactionHandler(int sigNumber, siginfo_t *siginfo, void *arg)
{
	(void)siginfo;
	(void)arg;
	if (sigNumber == __sigStartAllThread)
	{
		TRACE((stderr, "MemoryTracer: signal %d received, starting monitoring\n", sigNumber));
		leaktracer::MemoryTrace::GetInstance().startMonitoringAllThreads();
	}
	if (sigNumber == __sigStopAllThread)
	{
  		TRACE((stderr, "MemoryTracer: signal %d received, stoping monitoring\n", sigNumber));
		leaktracer::MemoryTrace::GetInstance().stopAllMonitoring();
	}
	if (sigNumber == __sigReport)
	{
		const char* reportFilename;
		if (getenv("LEAKTRACER_ONSIG_REPORTFILENAME") == NULL)
			reportFilename = "leaks.out";
		else
			reportFilename = getenv("LEAKTRACER_ONSIG_REPORTFILENAME");
		TRACE((stderr, "MemoryTracer: signal %d received, writing report to %s\n", sigNumber, reportFilename));
		leaktracer::MemoryTrace::GetInstance().writeLeaksToFile(reportFilename);
	}
}

int MemoryTrace::signalNumberFromString(const char* signame)
{
	if (strncmp(signame, "SIG", 3) == 0)
		signame += 3;

	if (strcmp(signame, "USR1") == 0)
		return SIGUSR1;
	else if (strcmp(signame, "USR2") == 0)
		return SIGUSR2;
	else
		return atoi(signame);
}

void
MemoryTrace::init_no_alloc_allowed()
{
	libc_alloc_func_t *curfunc;
	unsigned i;

 	for (i=0; i<(sizeof(libc_alloc_funcs)/sizeof(libc_alloc_funcs[0])); ++i) {
		curfunc = &libc_alloc_funcs[i];
		if (!*curfunc->localredirect) {
			if (curfunc->libcsymbol) {
				*curfunc->localredirect = curfunc->libcsymbol;
			} else {
				*curfunc->localredirect = dlsym(RTLD_NEXT, curfunc->symbname); 
			}
		}
	} 

	__instance = reinterpret_cast<MemoryTrace*>(&s_memoryTrace_instance);

	// we're using a c++ placement to initialized the MemoryTrace object living in the data section
	new (__instance) MemoryTrace();

	// it seems some implementation of pthread_key_create use malloc() internally (old linuxthreads)
	// these are not supported yet
	pthread_key_create(&__instance->__thread_internal_disabler_key, NULL);
}

void
MemoryTrace::init_full_from_once()
{
	leaktracer::MemoryTrace::GetInstance().init_full();
}

void
MemoryTrace::init_full()
{
	int sigNumber;
	struct sigaction sigact;

	__monitoringDisabler++;

	void *testmallocok = malloc(1);
	free(testmallocok);

	pthread_key_create(&__thread_options_key, CleanUpThreadData);

	if (!getenv("LEAKTRACER_NOBANNER"))
	{
#ifdef SHARED
		fprintf(stderr, "LeakTracer " LEAKTRACER_VERSION " (shared library) -- LGPLv2\n");
#else
		fprintf(stderr, "LeakTracer " LEAKTRACER_VERSION " (static library) -- LGPLv2\n");
#endif
	}

	if (getenv("LEAKTRACER_ONSIG_STARTALLTHREAD"))
	{
		sigact.sa_sigaction = sigactionHandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_SIGINFO;
		sigNumber = signalNumberFromString(getenv("LEAKTRACER_ONSIG_STARTALLTHREAD"));
		__sigStartAllThread = sigNumber;
		sigaction(sigNumber, &sigact, NULL);
		TRACE((stderr, "LeakTracer: registered signal %d SIGSTART for tid %d\n", sigNumber, (pid_t) syscall (SYS_gettid)));
	}

	if (getenv("LEAKTRACER_ONSIG_STOPALLTHREAD"))
	{
		sigact.sa_sigaction = sigactionHandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_SIGINFO;
		sigNumber = signalNumberFromString(getenv("LEAKTRACER_ONSIG_STOPALLTHREAD"));
		__sigStopAllThread = sigNumber;
		sigaction(sigNumber, &sigact, NULL);
		TRACE((stderr, "LeakTracer: registered signal %d SIGSTOP for tid %d\n", sigNumber, (pid_t) syscall (SYS_gettid)));
	}

	if (getenv("LEAKTRACER_ONSIG_REPORT"))
	{
		sigact.sa_sigaction = sigactionHandler;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_SIGINFO;
		sigNumber = signalNumberFromString(getenv("LEAKTRACER_ONSIG_REPORT"));
		__sigReport = sigNumber;
		sigaction(sigNumber, &sigact, NULL);
		TRACE((stderr, "LeakTracer: registered signal %d SIGREPORT for tid %d\n", sigNumber, (pid_t) syscall (SYS_gettid)));
	}

	if (getenv("LEAKTRACER_ONSTART_STARTALLTHREAD") || getenv("LEAKTRACER_AUTO_REPORTFILENAME"))
	{
		leaktracer::MemoryTrace::GetInstance().startMonitoringAllThreads();
	}
#ifdef USE_BACKTRACE
	// we call backtrace here, because there is some init on its first call
	void *bt;
	backtrace(&bt, 1);
#endif
	__setupDone = true;

	__monitoringDisabler--;
}

int MemoryTrace::Setup(void)
{
	pthread_once(&MemoryTrace::_init_no_alloc_allowed_once, MemoryTrace::init_no_alloc_allowed);

	if (!leaktracer::MemoryTrace::GetInstance().AllMonitoringIsDisabled()) {
		pthread_once(&MemoryTrace::_init_full_once, MemoryTrace::init_full_from_once);
	}
#if 0
       else if (!leaktracer::MemoryTrace::GetInstance().__setupDone) {
	}	
#endif
	return 0;
}

void MemoryTrace::MemoryTraceOnInit(void)
{
	//TRACE((stderr, "LeakTracer: MemoryTrace::MemoryTraceOnInit\n"));
	leaktracer::MemoryTrace::Setup();
}


void MemoryTrace::MemoryTraceOnExit(void)
{
	if (getenv("LEAKTRACER_ONEXIT_REPORT") || getenv("LEAKTRACER_AUTO_REPORTFILENAME"))
	{
		const char *reportName;
		if ( !(reportName = getenv("LEAKTRACER_ONEXIT_REPORTFILENAME")) && !(reportName = getenv("LEAKTRACER_AUTO_REPORTFILENAME")))
		{
			TRACE((stderr, "LeakTracer: LEAKTRACER_ONEXIT_REPORTFILENAME needs to be defined when using LEAKTRACER_ONEXIT_REPORT\n"));
			return;
		}
		leaktracer::MemoryTrace::GetInstance().stopAllMonitoring();
		TRACE((stderr, "LeakTracer: writing leak report in %s\n", reportName));
		leaktracer::MemoryTrace::GetInstance().writeLeaksToFile(reportName);
	}
	
	const char *exitCode = getenv("LEAKTRACER_EXIT_CODE_ON_LEAKS");
	if (exitCode != NULL && !leaktracer::MemoryTrace::GetInstance().__allocations.empty())
	{
		exit(atoi(exitCode));
	}
}

MemoryTrace::~MemoryTrace(void)
{
	pthread_key_delete(__thread_options_key);
}



// is called automatically when thread exists, whould
// cleanup per-thread data
void MemoryTrace::CleanUpThreadData(void *ptrThreadOptions)
{
	if( ptrThreadOptions != NULL )
		GetInstance().removeThreadOptions( reinterpret_cast<ThreadMonitoringOptions*>(ptrThreadOptions) );
}


// cleans per-thread configuration object, and removes
// it from the list of all objects
void MemoryTrace::removeThreadOptions(ThreadMonitoringOptions *pOptions)
{
	MutexLock lock(__threadListMutex);
	for (list_monitoring_options_t::iterator it = __listThreadOptions.begin(); it != __listThreadOptions.end(); ++it) {
		if (*it == pOptions) {
			// found this object in the list
			delete *it;
			__listThreadOptions.erase(it);
			return;
		}
	}
}

struct TraceHandle
{
	int nFramesToSkip;
	_Unwind_Word *backtrace;
	int backtraceSize;
	int pos;
};

static _Unwind_Reason_Code Unwind_Trace_Fn(_Unwind_Context *context, void *hnd)
{
	struct TraceHandle *traceHanle = (struct TraceHandle *) hnd;
	_Unwind_Word ip = _Unwind_GetIP(context);
	if (traceHanle->pos < traceHanle->backtraceSize)
	{
		// traceHanle->backtrace[traceHanle->pos] = (void *) (ip - (_Unwind_Word) s_P2pSODlInfo.dli_fbase);
		traceHanle->backtrace[traceHanle->pos] = ip;
		++traceHanle->pos;
		return _URC_NO_REASON;
	}
	return _URC_END_OF_STACK;
}

// stores allocation stack, up to ALLOCATION_STACK_DEPTH
// frames
void MemoryTrace::storeAllocationStack(void* arr[ALLOCATION_STACK_DEPTH], StackFrameExtendInfo arrExt[ALLOCATION_STACK_DEPTH],
	char processName[32])
{
	unsigned int iIndex = 0;

	TraceHandle traceHandle;
	memset(&traceHandle, 0, sizeof(traceHandle));
	traceHandle.nFramesToSkip = 0;
	traceHandle.backtrace = (_Unwind_Word *)arr;
	traceHandle.backtraceSize = ALLOCATION_STACK_DEPTH;
	traceHandle.pos = 0;
	_Unwind_Backtrace(Unwind_Trace_Fn, &traceHandle);

	// fill remaining spaces
	for (iIndex = traceHandle.pos; iIndex < ALLOCATION_STACK_DEPTH; iIndex++)
	{
		arr[iIndex] = NULL;
	}

	/* get current process name by prctl() */
	if (prctl(PR_GET_NAME, processName) != 0)
	{
		fprintf(stderr, "fatal error! prctl error : %s(%d).\n", strerror(errno), errno);
		strcpy(processName, "(unknown)");
	}

	// fill extStackFrame info
	int ret;
	Dl_info dlinfo;
	memset(&dlinfo, 0, sizeof(dlinfo));
	for (unsigned int i = 0; i < ALLOCATION_STACK_DEPTH; i++)
	{
		if (arr[i] == NULL)
		{
			break;
		}
		// get binary file's name and load address by symbol address.
		ret = dladdr((const void*)arr[i], &dlinfo);
		if(ret != 0)
		{
			//printf("dladdr ret:%d, fname:%s,base addr:%p\n", ret, dlinfo.dli_fname, dlinfo.dli_fbase);
			strncpy(arrExt[i].strDliFname, dlinfo.dli_fname, sizeof(arrExt[i].strDliFname) - 1);
			arrExt[i].strDliFname[sizeof(arrExt[i].strDliFname)-1] = '\0';
			/**
			  On arm,tina,musl-c platform, when PC is in dynamic library, PC is absolute address.
			  when PC is in executable file, PC is relative address.
			*/
			if(dlinfo.dli_fbase != (void*)0x10000)
			{
				arrExt[i].pDliFbase = dlinfo.dli_fbase;
			}
			else
			{
				arrExt[i].pDliFbase = (void*)0;
			}
		}
		else
		{
			fprintf(stderr, "fatal error! PC[%d] %p could not be matched to a shared object!\n", i, arr[i]);
			strcpy(arrExt[i].strDliFname, "(unknown)");
			arrExt[i].pDliFbase = (void*)0;
		}
	}
}

// writes all memory leaks to given stream
void MemoryTrace::writeLeaksPrivate(std::ostream &out)
{
	struct timespec mono, utc, diff;
	allocation_info_t *info;
	void *p;
	double d;
	const int precision = 6;
	int maxsecwidth;

	clock_gettime(CLOCK_REALTIME, &utc);
	clock_gettime(CLOCK_MONOTONIC, &mono);

	if (utc.tv_nsec > mono.tv_nsec) {
		diff.tv_nsec = utc.tv_nsec - mono.tv_nsec;
		diff.tv_sec = utc.tv_sec - mono.tv_sec;
	} else {
		diff.tv_nsec = 1000000000 - (mono.tv_nsec - utc.tv_nsec);
		diff.tv_sec = utc.tv_sec - mono.tv_sec -1;
	}

	maxsecwidth = 0;
	while(mono.tv_sec > 0) {
		mono.tv_sec = mono.tv_sec/10;
		maxsecwidth++;
	}
	if (maxsecwidth == 0) maxsecwidth=1;

	out << "# LeakTracer report";
	d = diff.tv_sec + (((double)diff.tv_nsec)/1000000000);
	out << " diff_utc_mono=" << std::fixed << std::left << std::setprecision(precision) << d ;
	out << "\n";

	__allocations.beginIteration();
	while (__allocations.getNextPair(&info, &p)) {
		d = info->timestamp.tv_sec + (((double)info->timestamp.tv_nsec)/1000000000);
		out << "leak, ";
		out << "time="  << std::fixed << std::right << std::setprecision(precision) << std::setfill('0') << std::setw(maxsecwidth+1+precision) << d << ", "; // setw(16) ?
		out << "stack=";
		for (unsigned int i = 0; i < ALLOCATION_STACK_DEPTH; i++) {
			if (info->allocStack[i] == NULL) break;

			if (i > 0) out << ' ';
			out << info->allocStack[i];
		}
		out << ", ";

		out << "size=" << info->size << ", ";

		out << "data=";
		const char *data = reinterpret_cast<const char *>(p);
		for (unsigned int i = 0; i < PRINTED_DATA_BUFFER_SIZE && i < info->size; i++)
			out << (isprint(data[i]) ? data[i] : '.');
		out << '\n';

		//print detailed stack trace
		char buffer[4096];
		int len = 0;
		int max_len = sizeof(buffer);
		len += snprintf(buffer, max_len, "Stack trace for [%s]:\n", info->processName);
		for (unsigned int i = 0; i < ALLOCATION_STACK_DEPTH; i++)
		{
			if (info->allocStack[i] == NULL)
			{
				break;
			}
			//[ 0]-> PC 0x55d741e4ca50: (+0xd) from [./dumpCallStack]
			len += snprintf(buffer + len, (size_t)(max_len - len), "[%2d]-> PC 0x%-8lx: (+0x%lx) from [%s]\n", i, (long unsigned int)info->allocStack[i],
				(long unsigned int)((char*)info->allocStack[i] - (char*)info->extStackInfo[i].pDliFbase), info->extStackInfo[i].strDliFname);
			if (len >= max_len - 1)
			{
				fprintf(stderr, "fatal error! buffer len[%d] not enough to contain string, curLen[%d]\n", max_len, len);
				break;
			}
		}
		buffer[max_len - 1] = '\0'; /* Ensure null termination. */
		out << buffer << '\n';
	}
}


// writes all memory leaks to given stream
void MemoryTrace::writeLeaks(std::ostream &out)
{
	MutexLock lock(__allocations_mutex);
	InternalMonitoringDisablerThreadUp();

	writeLeaksPrivate(out);

	InternalMonitoringDisablerThreadDown();
}


// writes all memory leaks to given stream
void MemoryTrace::writeLeaksToFile(const char* reportFilename)
{
	MutexLock lock(__allocations_mutex);
	InternalMonitoringDisablerThreadUp();

	std::ofstream oleaks;
	oleaks.open(reportFilename, std::ios_base::out);
	if (oleaks.is_open())
	{
		writeLeaksPrivate(oleaks);
		oleaks.close();
	}
	else
	{
		std::cerr << "Failed to write to \"" << reportFilename << "\"\n";
	}
	InternalMonitoringDisablerThreadDown();
}

void MemoryTrace::clearAllocationsInfo(void)
{
	MutexLock lock(__allocations_mutex);
	__allocations.clearAllInfo();
}


}  // end namespace

/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : SystemBase.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/15
  Last Modified :
  Description   : mpp component implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SystemBase"
#include <utils/plat_log.h>

#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>

//ref platform headers
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "SystemBase.h"


//media internal common headers.


/**
  extend pthread functions. add timeout.
  condition must set condAttr clock to CLOCK_MONOTONIC

  @param msecs timeout threshold, unit: ms.
*/
int pthread_cond_wait_timeout(pthread_cond_t* const condition, pthread_mutex_t* const mutex, unsigned int msecs)
{
    int ret;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int relative_sec = msecs/1000;
    int relative_nsec = (msecs%1000)*1000000;
    ts.tv_sec += relative_sec;
    ts.tv_nsec += relative_nsec;
    ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
    ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);
    ret = pthread_cond_timedwait(condition, mutex, &ts);
    if(ETIMEDOUT == ret)
    {
        //alogd("pthread cond timeout np timeout[%d]", ret);
    }
    else if(0 == ret)
    {
    }
    else
    {
        //aloge("fatal error! pthread cond timedwait[%d]", ret);
    }
    return ret;
}

int64_t CDX_GetSysTimeUsMonotonic()
{
    long long curr;
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
    return (int64_t)curr;
}

int CDX_SetTimeUs(int64_t timeUs)
{
    struct timeval tv;
    tv.tv_sec = timeUs / 1000000;
    tv.tv_usec = timeUs % 1000000;

    if(settimeofday(&tv, NULL) < 0) {
        return -1;
    }
    return 0;
}

int64_t CDX_GetTimeUs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_usec + tv.tv_sec * 1000000ll;
}

#include <sys/prctl.h>
#include <dlfcn.h>
#define MAX_STACK_FRAME_NUMBER (32)

#if 0
#include <execinfo.h>
/**
  method1: use backtrace() and backtrace_symbols(), but only glibc implements them, muslc do not.
*/
static void do_backtrace(const char* pTag)
{
    void  *array[MAX_STACK_FRAME_NUMBER];
    int    size;
    char **strings;
    int    i;
    char process_name[32];
    char buffer[4096];
    int len, max_len;

    // 获取当前的调用堆栈
    size = backtrace(array, MAX_STACK_FRAME_NUMBER);

    // 将堆栈中的地址转换为易于阅读的符号（函数名等）
    strings = backtrace_symbols(array, size);
    if (strings == NULL)
    {
        aloge("fatal error! backtrace_symbols NULL!");
        //exit(EXIT_FAILURE);
    }

    /* 获取当前进程名字, 通过prctl */
    memset(process_name, 0, sizeof(process_name));
    if (prctl(PR_GET_NAME, process_name) != 0)
    {
        aloge("fatal error! prctl error : %s(%d).\n", strerror(errno), errno);
        snprintf(process_name, sizeof(process_name), "(unknown)");
    }
    len     = 0;
    max_len = sizeof(buffer);
    len += snprintf(buffer, (size_t)max_len, "\ntag[%s]: Stack trace for [%s]:\n", pTag, process_name);
    for (i = 0; i < size; i++)
    {
        len += snprintf(buffer + len, (size_t)(max_len - len), "[%2d]-> %s\n", i, strings[i]);
        if (len >= max_len - 1)
        {
            aloge("fatal error! buffer len[%d] not enough to contain string, curLen[%d]\n", max_len, len);
            break;
        }
    }
    buffer[sizeof(buffer) - 1] = '\0'; /* Ensure null termination. */
    alogd("%s", buffer);

    free(strings);
}
#endif

#if 0
#define UNW_LOCAL_ONLY
#include <libunwind.h>
/**
  method2: use libunwind. tina-sdk contains this package, select it in menuconfig.
  glibc, muslc all can use it.
  note: need -funwind-tables
*/
static void libunwind_backtrace(const char* pTag)
{
    int32_t       ret, count, len, max_len;
    unw_cursor_t  cursor;
    unw_context_t context;
    unw_word_t    offset, pc, sp;
    Dl_info       dl_info;
    const char   *dname;
    char          fname[256], comm_name[32];
    char          buffer[4096];

    /* Initialize cursor to current frame for local unwinding. */
    if (unw_getcontext(&context) != 0)
    {
        aloge("fatal error! unw_getcontext error.\n");
        return;
    }
    if (unw_init_local(&cursor, &context) != 0)
    {
        aloge("fatal error! unw_init_local error.\n");
        return;
    }

    /* 获取当前线程名字, 通过prctl */
    memset(comm_name, 0, sizeof(comm_name));
    if (prctl(PR_GET_NAME, comm_name) != 0)
    {
        aloge("fatal error! prctl error: %s(%d).\n", strerror(errno), errno);
        snprintf(comm_name, sizeof(comm_name), "(unknown)");
    }

    len     = 0;
    max_len = sizeof(buffer);
    count   = 0;
    len += snprintf(buffer, (size_t)max_len, "\ntag[%s]: Stack trace for [%s]:\n", pTag, comm_name);
    // Unwind frames one by one, going up the frame stack.
    while (unw_step(&cursor) > 0)
    {
        if (unw_get_reg(&cursor, UNW_REG_IP, &pc) != 0)
        {
            aloge("fatal error! unw_get_reg error.\n");
            return;
        }
        /* 获取栈指针SP的值 */
        if (unw_get_reg(&cursor, UNW_REG_SP, &sp) != 0)
        {
            aloge("fatal error! unw_get_reg error.\n");
            return;
        };

        ret = unw_get_proc_name(&cursor, fname, sizeof(fname), &offset);
        if (ret != 0)
        {
            aloge("fatal error! get proc name ret:%d, [%d-%s], offset:%d", ret, errno, strerror(errno), offset);
            snprintf(fname, sizeof(fname), "??????");
            offset = 0;
        }

        /* 获取符号所属二进制文件名 */
        if (dladdr((void *)pc, &dl_info) && dl_info.dli_fname)
        {
            dname = dl_info.dli_fname;
        }
        else
        {
            dname = "(unknown)";
        }

        len += snprintf(buffer + len, (size_t)(max_len - len), "[%2d]-> PC 0x%-8x, SP 0x%-8x: (%s+0x%x) from [%s]\n",
            count++, pc, sp, fname, offset, dname);
        if (len >= max_len - 1)
        {
            aloge("fatal error! buffer len[%d] not enough to contain string, curLen[%d]\n", max_len, len);
            break;
        }
    }

    buffer[sizeof(buffer) - 1] = '\0'; /* Ensure null termination. */
    alogd("%s", buffer);

    return;
}
#endif

#if 1
#include <unwind.h>
typedef struct UnwindTraceState {
    size_t frames_to_skip;
    _Unwind_Word* current;
    _Unwind_Word* end;
}UnwindTraceState;

static _Unwind_Reason_Code UnwindTraceCallback(struct _Unwind_Context* context, void* arg)
{
    // Note: do not write `::_Unwind_GetIP` because it is a macro on some platforms.
    // Use `_Unwind_GetIP` instead!
    UnwindTraceState* const state = (UnwindTraceState*)(arg);
    if (state->frames_to_skip)
    {
        --state->frames_to_skip;
        return _Unwind_GetIP(context) ? _URC_NO_REASON : _URC_END_OF_STACK;
    }

    *state->current = _Unwind_GetIP(context);

    ++state->current;
    //if (!*(state->current - 1) || state->current == state->end)
    if (state->current == state->end)
    {
        return _URC_END_OF_STACK;
    }
    return _URC_NO_REASON;
}

/**
  method3: use _Unwind_Backtrace().
  glibc, muslc all can use it.
  on arm,tina,musl-c platform, when symbol is in dynamic library, its PC address is absolute address, need minus base addr
  of dynamic library. When symbol is in executable file, its PC address is relative address, use it directly, dlinfo.dli_fbase
  returned by dladdr() is 0x10000, it's wrong!

  note: need -funwind-tables
*/
static void Unwind_backtrace(const char* pTag)
{
    int ret;
    _Unwind_Word stacks[MAX_STACK_FRAME_NUMBER];
    UnwindTraceState state;
    state.frames_to_skip = 0;
    state.current = stacks;
    state.end = stacks + MAX_STACK_FRAME_NUMBER;

    _Unwind_Backtrace(&UnwindTraceCallback, &state);

    char process_name[32];
    char buffer[4096];
    int len, max_len;
    /* 获取当前进程名字, 通过prctl */
    memset(process_name, 0, sizeof(process_name));
    if (prctl(PR_GET_NAME, process_name) != 0)
    {
        aloge("fatal error! prctl error : %s(%d).\n", strerror(errno), errno);
        snprintf(process_name, sizeof(process_name), "(unknown)");
    }
    len     = 0;
    max_len = sizeof(buffer);
    len += snprintf(buffer, (size_t)max_len, "\ntag[%s]: Stack trace for [%s]:\n", pTag, process_name);

    size_t frames_count = state.current - &stacks[0];
    if(frames_count <= 0)
    {
        aloge("fatal error! _Unwind_Backtrace() fail! frame count[%d]", frames_count);
    }
    Dl_info dlinfo;
    memset(&dlinfo, 0, sizeof(dlinfo));
    const char *dname;
    _Unwind_Word dbase;
    for (size_t i = 0; i < frames_count; ++ i)
    {
        // stacks[i]是PC，需转换为相对地址和所属二进制文件名
        ret = dladdr((const void*)stacks[i], &dlinfo);
        if(ret != 0)
        {
            //alogd("dladdr ret:%d, base addr:%p", ret, dlinfo.dli_fbase);
            dname = dlinfo.dli_fname;
            /**
              On arm,tina,musl-c platform, when PC is in dynamic library, PC is absolute address.
              when PC is in executable file, PC is relative address.
            */
            if(dlinfo.dli_fbase != (void*)0x10000)
            {
                dbase = (_Unwind_Word)dlinfo.dli_fbase;
            }
            else
            {
                dbase = (_Unwind_Word)0;
            }
        }
        else
        {
            alogd("fatal error! PC[%d] 0x%-8lx could not be matched to a shared object!", i, stacks[i]);
            dname = "(unknown)";
            dbase = (_Unwind_Word)0;
        }
        //[ 0]-> PC 0x55d741e4ca50: (+0xd) from [./dumpCallStack]
        //printf("[%2zu]-> PC 0x%-12lx: (+0x%lx) from [%s]\n", i, stacks[i], stacks[i]-dbase, dname);
        len += snprintf(buffer + len, (size_t)(max_len - len), "[%2zu]-> PC 0x%-8x: (+0x%x) from [%s]\n",
            i, stacks[i], stacks[i]-dbase, dname);
        if (len >= max_len - 1)
        {
            aloge("fatal error! buffer len[%d] not enough to contain string, curLen[%d]\n", max_len, len);
            break;
        }
    }
    buffer[max_len - 1] = '\0'; /* Ensure null termination. */
    alogd("%s", buffer);
}
#endif

void dumpCallStack(const char* pTag)
{
    //method1: use backtrace()
    //do_backtrace(pTag);

    //method2: use libunwind
    //libunwind_backtrace(pTag);

    //method3: use _Unwind_Backtrace()
    Unwind_backtrace(pTag);

}

int tryOpenDev(const char *pDevPath, int nFlags, int nParam3)
{
    int nFd = -1;
    int i;
    for (i = 0; i < 10; i++)
    {
        nFd = open(pDevPath, nFlags, nParam3);
        if (nFd <= 0)
        {
            usleep(50*1000);
        }
        else
        {
            if (i > 0)
            {
                aloge("Be careful! try [%d]times fail to open dev[%s]", i, pDevPath);
            }
            break;
        }
    }
    if (nFd < 0)
    {
        aloge("fatal error! open dev[%s] fail. error[%d-%s]!", pDevPath, errno, strerror(errno));
    }
    return nFd;
}


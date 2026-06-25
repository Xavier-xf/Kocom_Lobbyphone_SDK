#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>

#include "log_print.h"

#define StripFileName(s) (strrchr(s, '/')?(strrchr(s, '/')+1):s)

/* color */
#define LIGHT   "1"
#define DARK    "0"

#define FG      "3"
#define BG      "4"

#define BLACK   "0"
#define RED     "1"
#define GREEN   "2"
#define YELLOW  "3"
#define BLUE    "4"
#define PURPLE  "5"
#define CYAN    "6"
#define WRITE   "7"

#define FG_COLOR(color)    FG color
#define BG_COLOR(color)    BG color

#define FMT_DEFAULT                 "\033[0m"
#define FMT_COLOR_FG(light, color)  "\033[" light ";" FG_COLOR(color) "m"
#define FMT_COLOR_BG(fg, color)     "\033[" BG_COLOR(color) ";" FG_COLOR(fg) "m"

static const char *GLOG_LEVEL_NAME[] =
{
    [_GLOG_INFO] = "I",
    [_GLOG_WARN] = "W",
    [_GLOG_ERROR] = "E",
    [_GLOG_FATAL] = "F",
};

typedef struct PrintfLogHelper
{
    bool FLAGS_colorlogtostderr; // = true;
    int FLAGS_minloglevel; // = _GLOG_INFO;
}PrintfLogHelper;

static PrintfLogHelper *glh = NULL;

static pthread_mutex_t gLogLock = PTHREAD_MUTEX_INITIALIZER;

void log_init(const char *program, GLogConfig *pConfig)
{
    pthread_mutex_lock(&gLogLock);
    if (NULL == glh)
    {
        PrintfLogHelper *pHelper = (PrintfLogHelper *)calloc(1, sizeof(PrintfLogHelper));
        if (pHelper)
        {
            pHelper->FLAGS_colorlogtostderr = (bool)pConfig->FLAGS_colorlogtostderr;
            pHelper->FLAGS_minloglevel = pConfig->FLAGS_minloglevel;
            glh = pHelper;
        }
        else
        {
            printf("E %s:%d <%s> fatal error! malloc fail!\n", StripFileName(__FILE__), __LINE__, __FUNCTION__);
        }
    }
    pthread_mutex_unlock(&gLogLock);
}

void log_quit()
{
    pthread_mutex_lock(&gLogLock);
    if (glh)
    {
        free(glh);
        glh = NULL;
    }
    pthread_mutex_unlock(&gLogLock);
}

/**
  use printf() to log.

  @param level
    _GLOG_INFO, _GLOG_WARN, _GLOG_ERROR, _GLOG_FATAL

  @return
    >= 0: the number of bytes printed (excluding the null byte used to end output to strings)
    -1: error.
*/
int log_printf(const char *file, const char *func, int line, const int level, const char *format, ...)
{
    bool bColorLog = true;
    int nMinLogLevel = _GLOG_INFO;
    if (glh) //if glh is not create, use default value.
    {
        bColorLog = glh->FLAGS_colorlogtostderr;
        nMinLogLevel = glh->FLAGS_minloglevel;
    }

    int nActualLevel = level;
    if (nActualLevel > _GLOG_FATAL)
    {
        nActualLevel = _GLOG_FATAL;
    }
    else if (nActualLevel < _GLOG_INFO)
    {
        nActualLevel = _GLOG_INFO;
    }

    if(nActualLevel < nMinLogLevel)
    {
        return 0;
    }

    int result = 0;
    char buffer[256];

    char *pBuffer = buffer;
    int nBufferSize = sizeof(buffer);

    int nLen0 = snprintf(pBuffer, nBufferSize, "%s %s:%d <%s> ", GLOG_LEVEL_NAME[nActualLevel], StripFileName(file), line,
        func);
    if (nLen0 >= nBufferSize)
    {
        nBufferSize = 2*nLen0;
        pBuffer = (char *)malloc(nBufferSize);
        if (NULL == pBuffer)
        {
            printf("(f:%s, l:%d) fatal error! malloc [%d]bytes fail\n", __FUNCTION__, __LINE__, nBufferSize);
            result = -1;
            goto _exit0;
        }
        nLen0 = snprintf(pBuffer, nBufferSize, "%s %s:%d <%s> ", GLOG_LEVEL_NAME[nActualLevel], StripFileName(file), line,
            func);
        if (nLen0 >= nBufferSize)
        {
            printf("(f:%s, l:%d) fatal error! check code! [%d>=%d]\n", __FUNCTION__, __LINE__, nLen0, nBufferSize);
            result = -1;
            goto _exit1;
        }
    }
    va_list args;
    va_start(args, format);
    int nLen1 = vsnprintf(pBuffer + nLen0, nBufferSize - nLen0, format, args);
    va_end(args);
    if (nLen1 >= nBufferSize - nLen0)
    {
        nBufferSize = nLen0 + nLen1 + 1;
        if (pBuffer != buffer)
        {
            pBuffer = (char *)realloc(pBuffer, nBufferSize);
            if (NULL == pBuffer)
            {
                printf("(f:%s, l:%d) fatal error! realloc [%d]bytes fail\n", __FUNCTION__, __LINE__, nBufferSize);
                result = -1;
                goto _exit1;
            }
        }
        else
        {
            pBuffer = (char *)malloc(nBufferSize);
            if (NULL == pBuffer)
            {
                printf("(f:%s, l:%d) fatal error! malloc [%d]bytes fail\n", __FUNCTION__, __LINE__, nBufferSize);
                result = -1;
                goto _exit1;
            }
            memcpy(pBuffer, buffer, nLen0);
        }
        va_start(args, format);
        nLen1 = vsnprintf(pBuffer + nLen0, nBufferSize - nLen0, format, args);
        va_end(args);
        if (nLen1 >= nBufferSize - nLen0)
        {
            printf("(f:%s, l:%d) fatal error! check code! [%d>=%d]\n", __FUNCTION__, __LINE__, nLen1, nBufferSize - nLen0);
            result = -1;
            goto _exit1;
        }
    }
    if (nLen0 + nLen1 != strlen(pBuffer))
    {
        printf("(f:%s, l:%d) fatal error! string Len:%d != %d bytes\n",  __FUNCTION__, __LINE__, nLen0 + nLen1,
            strlen(pBuffer));
    }
    if (bColorLog)
    {
        switch (nActualLevel)
        {
        case _GLOG_INFO:
            result = printf("%s\n", pBuffer);
            break;
        case _GLOG_WARN:
            result = printf(FMT_COLOR_FG(LIGHT, YELLOW)"%s\n"FMT_DEFAULT, pBuffer);
            break;
        case _GLOG_ERROR:
            result = printf(FMT_COLOR_FG(LIGHT, RED)"%s\n"FMT_DEFAULT, pBuffer);
            break;
        case _GLOG_FATAL:
            result = printf(FMT_COLOR_FG(LIGHT, RED)"%s\n"FMT_DEFAULT, pBuffer);
            break;
        default:
            result = printf("%s\n", pBuffer);
            break;
        }
    }
    else
    {
        result = printf("%s\n", pBuffer);
    }
    if (pBuffer != buffer)
    {
        free(pBuffer);
        pBuffer = NULL;
    }
    return result;

_exit1:
    if ((pBuffer != NULL) && (pBuffer != buffer))
    {
        free(pBuffer);
        pBuffer = NULL;
    }
_exit0:
    return result;
}


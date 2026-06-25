
#ifndef OS_SUPPORT_H
#define OS_SUPPORT_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//#if defined(WIN32)&&!defined(_cplusplus)
#define inline __inline
//#endif

#ifndef WIN32
typedef long long __int64;
#endif

/** Speex wrapper for calloc. To do your own dynamic allocation, all you need to do is replace this function, speex_realloc and speex_free 
    NOTE: speex_alloc needs to CLEAR THE MEMORY */
#ifndef OVERRIDE_SPEEX_ALLOC
//extern int total_heap;
static inline void *speex_alloc (int size)
{
   /* WARNING: this is not equivalent to malloc(). If you want to use malloc() 
      or your own allocator, YOU NEED TO CLEAR THE MEMORY ALLOCATED. Otherwise
      you will experience strange bugs */
//total_heap += size;
   return calloc(size, 1);
}
#endif


/** Speex wrapper for calloc. To do your own dynamic allocation, all you need to do is replace this function, speex_realloc and speex_alloc */
#ifndef OVERRIDE_SPEEX_FREE
static inline void speex_free (void *ptr)
{
   free(ptr);
}
#endif


/** Copy n bytes of memory from src to dst, allowing overlapping regions. The 0* term 
    provides compile-time type checking */
#ifndef OVERRIDE_SPEEX_MOVE
#define SPEEX_MOVE(dst, src, n) (memmove((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src)) ))
#endif



#ifndef OVERRIDE_SPEEX_FATAL
static inline void _speex_fatal(const char *str, const char *file, int line)
{
   fprintf (stderr, "Fatal (internal) error in %s, line %d: %s\n", file, line, str);
   exit(1);
}
#endif

#ifndef OVERRIDE_SPEEX_WARNING
static inline void speex_warning(const char *str)
{
#ifndef DISABLE_WARNINGS
   fprintf (stderr, "warning: %s\n", str);
#endif
}
#endif

#ifndef OVERRIDE_SPEEX_WARNING_INT
static inline void speex_warning_int(const char *str, int val)
{
#ifndef DISABLE_WARNINGS
   fprintf (stderr, "warning: %s %d\n", str, val);
#endif
}
#endif





#define speex_fatal(str) _speex_fatal(str, __FILE__, __LINE__);
//#define speex_assert(cond) {if (!(cond)) {speex_fatal("assertion failed: " #cond);}}



#endif


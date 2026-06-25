/* zconf.h -- configuration of the zlib isp_compression library
 * Copyright (C) 1995-2013 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#ifndef ZCONF_H
#define ZCONF_H

/*
 * If you *really* need a unique prefix for all types and library functions,
 * compile with -DZ_PREFIX. The "standard" zlib should be compiled without it.
 * Even better than compiling with -DZ_PREFIX would be to use configure to set
 * this permanently in zconf.h using "./configure --zprefix".
 */
#ifdef Z_PREFIX     /* may be set to #if 1 by ./configure */
#  define Z_PREFIX_SET

/* all linked symbols */
#  define isp__dist_code            z_isp__dist_code
#  define isp__length_code          z_isp__length_code
#  define isp__tr_align             z_isp__tr_align
#  define isp__tr_flush_bits        z_isp__tr_flush_bits
#  define isp__tr_flush_block       z_isp__tr_flush_block
#  define isp__tr_init              z_isp__tr_init
#  define isp__tr_stored_block      z_isp__tr_stored_block
#  define isp__tr_tally             z_isp__tr_tally
#  define isp_adler32               z_isp_adler32
#  define isp_adler32_combine       z_isp_adler32_combine
#  define isp_isp_adler32_combine64     z_isp_isp_adler32_combine64
#  ifndef Z_SOLO
#    define isp_compress              z_isp_compress
#    define isp_compress2             z_isp_compress2
#    define isp_compressBound         z_isp_compressBound
#  endif
#  define isp_crc32                 z_isp_crc32
#  define isp_crc32_combine         z_isp_crc32_combine
#  define isp_crc32_combine64       z_isp_crc32_combine64
#  define deflate               z_deflate
#  define isp_deflateBound          z_isp_deflateBound
#  define isp_deflateCopy           z_isp_deflateCopy
#  define isp_deflateEnd            z_isp_deflateEnd
#  define isp_deflateInit2_         z_isp_deflateInit2_
#  define isp_deflateInit_          z_isp_deflateInit_
#  define isp_deflateParams         z_isp_deflateParams
#  define isp_deflatePending        z_isp_deflatePending
#  define isp_deflatePrime          z_isp_deflatePrime
#  define isp_deflateReset          z_isp_deflateReset
#  define isp_isp_deflateResetKeep      z_isp_isp_deflateResetKeep
#  define isp_deflateSetDictionary  z_isp_deflateSetDictionary
#  define isp_deflateSetHeader      z_isp_deflateSetHeader
#  define isp_deflateTune           z_isp_deflateTune
#  define deflate_copyright     z_deflate_copyright
#  define isp_get_crc_table         z_isp_get_crc_table
#  ifndef Z_SOLO
#    define isp_gz_error              z_isp_gz_error
#    define gz_intmax             z_gz_intmax
#    define gz_strwinerror        z_gz_strwinerror
#    define isp_gzbuffer              z_isp_gzbuffer
#    define isp_gzclearerr            z_isp_gzclearerr
#    define isp_gzclose               z_isp_gzclose
#    define isp_gzclose_r             z_isp_gzclose_r
#    define isp_gzclose_w             z_isp_gzclose_w
#    define isp_gzdirect              z_isp_gzdirect
#    define isp_gzdopen               z_isp_gzdopen
#    define isp_gzeof                 z_isp_gzeof
#    define isp_gzerror               z_isp_gzerror
#    define isp_gzflush               z_isp_gzflush
#    define isp_gzgetc                z_isp_gzgetc
#    define isp_gzgetc_               z_isp_gzgetc_
#    define isp_gzgets                z_isp_gzgets
#    define isp_zoffset              z_isp_gzoffset
#    define isp_gzoffset64            z_isp_gzoffset64
#    define isp_gzopen                z_isp_gzopen
#    define isp_gzopen64              z_isp_gzopen64
#    ifdef _WIN32
#      define isp_gzopen_w              z_isp_gzopen_w
#    endif
#    define isp_gzprintf              z_isp_gzprintf
#    define isp_gzvprintf             z_isp_gzvprintf
#    define isp_gzputc                z_isp_gzputc
#    define isp_gzputs                z_isp_gzputs
#    define isp_gzread                z_isp_gzread
#    define isp_gzrewind              z_isp_gzrewind
#    define isp_gzseek                z_isp_gzseek
#    define isp_gzseek64              z_isp_gzseek64
#    define isp_gzsetparams           z_isp_gzsetparams
#    define isp_gztell                z_isp_gztell
#    define isp_gztell64              z_isp_gztell64
#    define isp_gzungetc              z_isp_gzungetc
#    define isp_gzwrite               z_isp_gzwrite
#  endif
#  define isp_inflate               z_isp_inflate
#  define isp_inflateBack           z_isp_inflateBack
#  define isp_inflateBackEnd        z_isp_inflateBackEnd
#  define isp_isp_inflateBackInit_      z_isp_isp_inflateBackInit_
#  define isp_inflateCopy           z_isp_inflateCopy
#  define isp_inflateEnd            z_isp_inflateEnd
#  define isp_inflateGetHeader      z_isp_inflateGetHeader
#  define isp_inflateInit2_         z_isp_inflateInit2_
#  define isp_inflateInit_          z_isp_inflateInit_
#  define isp_inflateMark           z_isp_inflateMark
#  define isp_inflatePrime          z_isp_inflatePrime
#  define isp_inflateReset          z_isp_inflateReset
#  define isp_inflateReset2         z_isp_inflateReset2
#  define isp_inflateSetDictionary  z_isp_inflateSetDictionary
#  define isp_inflateGetDictionary  z_isp_inflateGetDictionary
#  define isp_inflateSync           z_isp_inflateSync
#  define isp_isp_inflateSyncPoint      z_isp_isp_inflateSyncPoint
#  define isp_inflateUndermine      z_isp_inflateUndermine
#  define isp_inflateResetKeep      z_isp_inflateResetKeep
#  define isp_inflate_copyright     z_isp_inflate_copyright
#  define isp_inflate_fast          z_isp_inflate_fast
#  define isp_inflate_table         z_isp_inflate_table
#  ifndef Z_SOLO
#    define unisp_compress            z_unisp_compress
#  endif
#  define isp_zError                z_isp_zError
#  ifndef Z_SOLO
#    define isp_zcalloc               z_isp_zcalloc
#    define isp_zcfree                z_isp_zcfree
#  endif
#  define isp_zlibCompileFlags      z_isp_zlibCompileFlags
#  define isp_zlibVersion           z_isp_zlibVersion

/* all zlib typedefs in zlib.h and zconf.h */
#  define Byte                  z_Byte
#  define Bytef                 z_Bytef
#  define alloc_func            z_alloc_func
#  define charf                 z_charf
#  define free_func             z_free_func
#  ifndef Z_SOLO
#    define gzFile                z_gzFile
#  endif
#  define gz_header             z_gz_header
#  define gz_headerp            z_gz_headerp
#  define in_func               z_in_func
#  define intf                  z_intf
#  define out_func              z_out_func
#  define uInt                  z_uInt
#  define uIntf                 z_uIntf
#  define uLong                 z_uLong
#  define uLongf                z_uLongf
#  define voidp                 z_voidp
#  define voidpc                z_voidpc
#  define voidpf                z_voidpf

/* all zlib structs in zlib.h and zconf.h */
#  define gz_header_s           z_gz_header_s
#  define internal_state        z_internal_state

#endif

#if defined(__MSDOS__) && !defined(MSDOS)
#  define MSDOS
#endif
#if (defined(OS_2) || defined(__OS2__)) && !defined(OS2)
#  define OS2
#endif
#if defined(_WINDOWS) && !defined(WINDOWS)
#  define WINDOWS
#endif
#if defined(_WIN32) || defined(_WIN32_WCE) || defined(__WIN32__)
#  ifndef WIN32
#    define WIN32
#  endif
#endif
#if (defined(MSDOS) || defined(OS2) || defined(WINDOWS)) && !defined(WIN32)
#  if !defined(__GNUC__) && !defined(__FLAT__) && !defined(__386__)
#    ifndef SYS16BIT
#      define SYS16BIT
#    endif
#  endif
#endif

/*
 * Compile with -DMAXSEG_64K if the alloc function cannot allocate more
 * than 64k bytes at a time (needed on systems with 16-bit int).
 */
#ifdef SYS16BIT
#  define MAXSEG_64K
#endif
#ifdef MSDOS
#  define UNALIGNED_OK
#endif

#ifdef __STDC_VERSION__
#  ifndef STDC
#    define STDC
#  endif
#  if __STDC_VERSION__ >= 199901L
#    ifndef STDC99
#      define STDC99
#    endif
#  endif
#endif
#if !defined(STDC) && (defined(__STDC__) || defined(__cplusplus))
#  define STDC
#endif
#if !defined(STDC) && (defined(__GNUC__) || defined(__BORLANDC__))
#  define STDC
#endif
#if !defined(STDC) && (defined(MSDOS) || defined(WINDOWS) || defined(WIN32))
#  define STDC
#endif
#if !defined(STDC) && (defined(OS2) || defined(__HOS_AIX__))
#  define STDC
#endif

#if defined(__OS400__) && !defined(STDC)    /* iSeries (formerly AS/400). */
#  define STDC
#endif

#ifndef STDC
#  ifndef const /* cannot use !defined(STDC) && !defined(const) on Mac */
#    define const       /* note: need a more gentle solution here */
#  endif
#endif

#if defined(ZLIB_CONST) && !defined(z_const)
#  define z_const const
#else
#  define z_const
#endif

/* Some Mac compilers merge all .h files incorrectly: */
#if defined(__MWERKS__)||defined(applec)||defined(THINK_C)||defined(__SC__)
#  define NO_DUMMY_DECL
#endif

/* Maximum value for memLevel in deflateInit2 */
#ifndef MAX_MEM_LEVEL
#  ifdef MAXSEG_64K
#    define MAX_MEM_LEVEL 8
#  else
#    define MAX_MEM_LEVEL 9
#  endif
#endif

/* Maximum value for windowBits in deflateInit2 and inflateInit2.
 * WARNING: reducing MAX_WBITS makes minigzip unable to extract .gz files
 * created by gzip. (Files created by minigzip can still be extracted by
 * gzip.)
 */
#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

/* The memory requirements for deflate are (in bytes):
            (1 << (windowBits+2)) +  (1 << (memLevel+9))
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, compile with
     make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
 Of course this will generally degrade isp_compression (there's no free lunch).

   The memory requirements for inflate are (in bytes) 1 << windowBits
 that is, 32K for windowBits=15 (default value) plus a few kilobytes
 for small objects.
*/

                        /* Type declarations */

#ifndef OF /* function prototypes */
#  ifdef STDC
#    define OF(args)  args
#  else
#    define OF(args)  ()
#  endif
#endif

#ifndef Z_ARG /* function prototypes for stdarg */
#  if defined(STDC) || defined(Z_HAVE_STDARG_H)
#    define Z_ARG(args)  args
#  else
#    define Z_ARG(args)  ()
#  endif
#endif

/* The following definitions for FAR are needed only for MSDOS mixed
 * model programming (small or medium model with some far allocations).
 * This was tested only with MSC; for other MSDOS compilers you may have
 * to define NO_MEMCPY in zutil.h.  If you don't need the mixed model,
 * just define FAR to be empty.
 */
#ifdef SYS16BIT
#  if defined(M_I86SM) || defined(M_I86MM)
     /* MSC small or medium model */
#    define SMALL_MEDIUM
#    ifdef _MSC_VER
#      define FAR _far
#    else
#      define FAR far
#    endif
#  endif
#  if (defined(__SMALL__) || defined(__MEDIUM__))
     /* Turbo C small or medium model */
#    define SMALL_MEDIUM
#    ifdef __BORLANDC__
#      define FAR _far
#    else
#      define FAR far
#    endif
#  endif
#endif

#if defined(WINDOWS) || defined(WIN32)
   /* If building or using zlib as a DLL, define ZLIB_DLL.
    * This is not mandatory, but it offers a little performance increase.
    */
#  ifdef ZLIB_DLL
#    if defined(WIN32) && (!defined(__BORLANDC__) || (__BORLANDC__ >= 0x500))
#      ifdef ZLIB_INTERNAL
#        define ZEXTERN extern __declspec(dllexport)
#      else
#        define ZEXTERN extern __declspec(dllimport)
#      endif
#    endif
#  endif  /* ZLIB_DLL */
   /* If building or using zlib with the WINAPI/WINAPIV calling convention,
    * define ZLIB_WINAPI.
    * Caution: the standard ZLIB1.DLL is NOT compiled using ZLIB_WINAPI.
    */
#  ifdef ZLIB_WINAPI
#    ifdef FAR
#      undef FAR
#    endif
#    include <windows.h>
     /* No need for _export, use ZLIB.DEF instead. */
     /* For complete Windows compatibility, use WINAPI, not __stdcall. */
#    define ZEXPORT WINAPI
#    ifdef WIN32
#      define ZEXPORTVA WINAPIV
#    else
#      define ZEXPORTVA FAR CDECL
#    endif
#  endif
#endif

#if defined (__BEOS__)
#  ifdef ZLIB_DLL
#    ifdef ZLIB_INTERNAL
#      define ZEXPORT   __declspec(dllexport)
#      define ZEXPORTVA __declspec(dllexport)
#    else
#      define ZEXPORT   __declspec(dllimport)
#      define ZEXPORTVA __declspec(dllimport)
#    endif
#  endif
#endif

#ifndef ZEXTERN
#  define ZEXTERN extern
#endif
#ifndef ZEXPORT
#  define ZEXPORT
#endif
#ifndef ZEXPORTVA
#  define ZEXPORTVA
#endif

#ifndef FAR
#  define FAR
#endif

#if !defined(__MACTYPES__)
typedef unsigned char  Byte;  /* 8 bits */
#endif
typedef unsigned int   uInt;  /* 16 bits or more */
typedef unsigned long  uLong; /* 32 bits or more */

#ifdef SMALL_MEDIUM
   /* Borland C/C++ and some old MSC versions ignore FAR inside typedef */
#  define Bytef Byte FAR
#else
   typedef Byte  FAR Bytef;
#endif
typedef char  FAR charf;
typedef int   FAR intf;
typedef uInt  FAR uIntf;
typedef uLong FAR uLongf;

#ifdef STDC
   typedef void const *voidpc;
   typedef void FAR   *voidpf;
   typedef void       *voidp;
#else
   typedef Byte const *voidpc;
   typedef Byte FAR   *voidpf;
   typedef Byte       *voidp;
#endif

#if !defined(Z_U4) && !defined(Z_SOLO) && defined(STDC)
#  include <limits.h>
#  if (UINT_MAX == 0xffffffffUL)
#    define Z_U4 unsigned
#  elif (ULONG_MAX == 0xffffffffUL)
#    define Z_U4 unsigned long
#  elif (USHRT_MAX == 0xffffffffUL)
#    define Z_U4 unsigned short
#  endif
#endif

#ifdef Z_U4
   typedef Z_U4 z_crc_t;
#else
   typedef unsigned long z_crc_t;
#endif

#if 1    /* was set to #if 1 by ./configure */
#  define Z_HAVE_UNISTD_H
#endif

#if 1    /* was set to #if 1 by ./configure */
#  define Z_HAVE_STDARG_H
#endif

#ifdef STDC
#  ifndef Z_SOLO
#    include <sys/types.h>      /* for off_t */
#  endif
#endif

#if defined(STDC) || defined(Z_HAVE_STDARG_H)
#  ifndef Z_SOLO
#    include <stdarg.h>         /* for va_list */
#  endif
#endif

#ifdef _WIN32
#  ifndef Z_SOLO
#    include <stddef.h>         /* for wchar_t */
#  endif
#endif

/* a little trick to accommodate both "#define _LARGEFILE64_SOURCE" and
 * "#define _LARGEFILE64_SOURCE 1" as requesting 64-bit operations, (even
 * though the former does not conform to the LFS document), but considering
 * both "#undef _LARGEFILE64_SOURCE" and "#define _LARGEFILE64_SOURCE 0" as
 * equivalently requesting no 64-bit operations
 */
#if defined(_LARGEFILE64_SOURCE) && -_LARGEFILE64_SOURCE - -1 == 1
#  undef _LARGEFILE64_SOURCE
#endif

#if defined(__WATCOMC__) && !defined(Z_HAVE_UNISTD_H)
#  define Z_HAVE_UNISTD_H
#endif
#ifndef Z_SOLO
#  if defined(Z_HAVE_UNISTD_H) || defined(_LARGEFILE64_SOURCE)
#    include <unistd.h>         /* for SEEK_*, off_t, and _LFS64_LARGEFILE */
#    ifdef VMS
#      include <unixio.h>       /* for off_t */
#    endif
#    ifndef z_off_t
#      define z_off_t off_t
#    endif
#  endif
#endif

#if defined(_LFS64_LARGEFILE) && _LFS64_LARGEFILE-0
#  define Z_LFS64
#endif

#if defined(_LARGEFILE64_SOURCE) && defined(Z_LFS64)
#  define Z_LARGE64
#endif

#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS-0 == 64 && defined(Z_LFS64)
#  define Z_WANT64
#endif

#if !defined(SEEK_SET) && !defined(Z_SOLO)
#  define SEEK_SET        0       /* Seek from beginning of file.  */
#  define SEEK_CUR        1       /* Seek from current position.  */
#  define SEEK_END        2       /* Set file pointer to EOF plus "offset" */
#endif

#ifndef z_off_t
#  define z_off_t long
#endif

#if !defined(_WIN32) && defined(Z_LARGE64)
#  define z_off64_t off64_t
#else
#  if defined(_WIN32) && !defined(__GNUC__) && !defined(Z_SOLO)
#    define z_off64_t __int64
#  else
#    define z_off64_t z_off_t
#  endif
#endif

/* MVS linker does not support external names larger than 8 bytes */
#if defined(__MVS__)
  #pragma map(isp_deflateInit_,"DEIN")
  #pragma map(isp_deflateInit2_,"DEIN2")
  #pragma map(isp_deflateEnd,"DEEND")
  #pragma map(isp_deflateBound,"DEBND")
  #pragma map(isp_inflateInit_,"ININ")
  #pragma map(isp_inflateInit2_,"ININ2")
  #pragma map(isp_inflateEnd,"INEND")
  #pragma map(isp_inflateSync,"INSY")
  #pragma map(isp_inflateSetDictionary,"INSEDI")
  #pragma map(isp_compressBound,"CMBND")
  #pragma map(isp_inflate_table,"INTABL")
  #pragma map(isp_inflate_fast,"INFA")
  #pragma map(isp_inflate_copyright,"INCOPY")
#endif

#endif /* ZCONF_H */

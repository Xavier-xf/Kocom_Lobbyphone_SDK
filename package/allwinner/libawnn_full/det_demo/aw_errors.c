//#include "aw_macro.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "aw_errors.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void arg_error(const char *fun, const char *msg)
{
#ifdef ANDROID
	LOGE("> argument error: [%s]: %s \n", fun, msg);
#endif
	fprintf(stderr, "> argument error: [%s]: %s \n", fun, msg);
	assert(0);
}

void calloc_error(const char *fun, const char *msg)
{

#ifdef ANDROID
	LOGE("> calloc error: [%s]: %s \n", fun, msg);
#endif
	fprintf(stderr, "> calloc error: [%s]: %s \n", fun, msg);
	assert(0);
}

void common_error(const char *fun, const char *msg)
{
#ifdef ANDROID
	LOGE("> common_error: [%s]: %s \n", fun, msg);
#endif

	fprintf(stderr, "> common_error: [%s]: %s \n", fun, msg);
	assert(0);
}


void common_warning(const char *fun, const char *msg)
{
#ifdef ANDROID
	LOGE("> common_warning: [%s]: %s \n", fun, msg);
#endif

	fprintf(stderr, "> common_warning: [%s]: %s \n", fun, msg);
	// No assert(0) in debug mode, it only prints log.
}

void fopen_error(const char *fun, const char *msg)
{

#ifdef ANDROID
	LOGE("> fopen error: [%s]: %s \n", fun, msg);
#endif
	fprintf(stderr, "> fopen error: [%s]: %s \n", fun, msg);
	assert(0);
}

void fread_error(const char *fun, const char *msg)
{

#ifdef ANDROID
	LOGE("> fread error: [%s]: %s \n", fun, msg);
#endif

	fprintf(stderr, "> fread error: [%s]: %s \n", fun, msg);
	assert(0);
}

void fwrite_error(const char *fun, const char *msg)
{

#ifdef ANDROID
	LOGE("> fwrite error: [%s]: %s \n", fun, msg);
#endif

	fprintf(stderr, "> fwrite error: [%s]: %s \n", fun, msg);
	assert(0);
}

void malloc_error(const char *fun, const char *msg)
{

#ifdef ANDROID
	LOGE("> malloc error: [%s]: %s \n", fun, msg);
#endif

	fprintf(stderr, "> malloc error: [%s]: %s \n", fun, msg);
	assert(0);
}

void nullptr_error(const char *fun, const char *msg)
{

#ifdef ANDROID
	LOGE("> nullptr error: [%s]: %s \n", fun, msg);
#endif

	fprintf(stderr, "> nullptr error: [%s]: %s \n", fun, msg);
	assert(0);
}

#ifdef __cplusplus
}
#endif // __cplusplus
#ifndef _NS_H_
#define _NS_H_
#include "nosc.h"

void* ns_create(ns_prms_t* prms);

void ns_destroy(void* handle);

int ns_process(void* handle, short* in, short* out);

#endif

#ifndef _AWAGC_H_
#define _AWAGC_H_
#include "drclog.h"
void* awagc_create(drcLog_prms_t* prms);
void awagc_process(void* handle, short* xout,int samplenum, int nch);
void awagc_destroy(void* handle);
#endif
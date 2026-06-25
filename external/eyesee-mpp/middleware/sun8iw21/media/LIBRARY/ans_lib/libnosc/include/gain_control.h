#ifndef _GIAN_CONTROL_H_
#define _GIAN_CONTROL_H_

#define __USE_DRCLOG

#ifdef __USE_DRCLOG

#include "drclog.h"
#define GIAN_CONTROL_CREATE		nosc_drcLog_create
#define GIAN_CONTROL_DESTROY	nosc_drcLog_destroy
#define GIAN_CONTROL_PROCESS	nosc_drcLog_process

#else

#include "awagc.h"
#define GIAN_CONTROL_CREATE		awagc_create
#define GIAN_CONTROL_DESTROY	awagc_destroy
#define GIAN_CONTROL_PROCESS	awagc_process

#endif


#endif

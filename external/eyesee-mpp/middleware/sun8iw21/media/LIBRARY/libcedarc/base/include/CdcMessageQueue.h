/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMessageQueue.h
 * Description : message queue
 * History :
 *
 */

#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#ifdef CONFIG_VIDEO_RT_MEDIA
#include <linux/semaphore.h>
#else
#include <stdint.h>
#include <semaphore.h>

#ifndef uintptr_t
typedef size_t uintptr_t;
#endif

#endif
#include "UserKernelAdapter.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MessageQueueContext CdcMessageQueue;

//typedef void (*msgHandlerT)(CdcMessage *msg, void *arg);

typedef struct CdcMessage {
    int          messageId;
    uintptr_t    params[4];
  //  msgHandlerT  execute;
}CdcMessage;

/**
 * @param nMaxMessageNum How many messages the message queue can hold
 * @param pName The name of the message queue which is used in log output
 * @param nMessageSize sizeof(struct AwMessage)
 */
CdcMessageQueue* CdcMessageQueueCreate(int nMaxMessageNum, const char* pName);

void CdcMessageQueueDestroy(CdcMessageQueue* mq);

int CdcMessageQueuePostMessage(CdcMessageQueue* mq, CdcMessage* m);
int CdcMessageQueueWaitMessage_0(CdcMessageQueue* mq, int64_t timeout);
#if 0
#define CdcMessageQueueWaitMessage(a, b) do {\
	long long startTime = getCurrentTime();\
	CdcMessageQueueWaitMessage_0(a, b);\
	long long endTime = getCurrentTime();\
	long long diffTime = (endTime - startTime);\
	logw("**** wait msg time = %lld us, %lld ms", diffTime, diffTime/1000);\
 } while (0)
#else
#define CdcMessageQueueWaitMessage(a, b) do {\
	CdcMessageQueueWaitMessage_0(a, b);\
 } while (0)
#endif


int CdcMessageQueueGetMessage(CdcMessageQueue* mq, CdcMessage* m);

int CdcMessageQueueTryGetMessage(CdcMessageQueue* mq, CdcMessage* m, int64_t timeout);

int CdcMessageQueueFlush(CdcMessageQueue* mq);

int CdcMessageQueueGetCount(CdcMessageQueue* mq);

//* define a semaphore timedwait method for common use.
int CdcSemTimedWait(SEM_STRUCT* sem, int64_t time_ms);

#ifdef __cplusplus
}
#endif

#endif

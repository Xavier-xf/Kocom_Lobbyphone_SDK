/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : messageQueue.c
 * Description : message queue
 * History :
 *
 */
#ifdef CONFIG_VIDEO_RT_MEDIA

#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/string.h>

#else

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <malloc.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

#endif

#include "cdc_log.h"
#include "CdcMessageQueue.h"

typedef struct MessageNode MessageNode;

struct MessageNode
{
    MessageNode* next;
    int          valid;
    CdcMessage    *msg;
};

typedef struct MessageQueueContext
{
    char*           pName;
    MessageNode*    pHead;
    int             nCount;
    MessageNode*    Nodes;
    int             nMaxMessageNum;
    size_t          nMessageSize;
    MUTEX_STRUCT mutex;
    SEM_STRUCT           sem;
} MessageQueueContext;

CdcMessageQueue* CdcMessageQueueCreate(int nMaxMessageNum, const char* pName)
{
    MessageQueueContext* mqCtx;
    size_t nMessageSize = sizeof(CdcMessage);
    logd("nMessageSize = %d",(int)nMessageSize);

    mqCtx = (MessageQueueContext*)MALLOC(sizeof(MessageQueueContext));
    if(mqCtx == NULL)
    {
        loge("%s, allocate memory fail.", pName);
        return NULL;
    }
    memset(mqCtx, 0, sizeof(MessageQueueContext));

    mqCtx->pName = CALLOC(1, strlen(pName) + 1);
    memcpy(mqCtx->pName, pName, strlen(pName) + 1);

    mqCtx->Nodes = (MessageNode*)CALLOC(nMaxMessageNum, sizeof(MessageNode));
    if(mqCtx->Nodes == NULL)
    {
        loge("%s, allocate memory for message nodes fail.", mqCtx->pName);
        if(mqCtx->pName != NULL)
            FREE(mqCtx->pName);
        FREE(mqCtx);
        return NULL;
    }

    int i;
    for (i = 0; i < nMaxMessageNum; i++)
    {
        mqCtx->Nodes[i].msg = CALLOC(1, nMessageSize);
        if (mqCtx->Nodes[i].msg == NULL)
        {
            int j;
            for (j = 0; j < i; j++)
                FREE(mqCtx->Nodes[j].msg);
            FREE(mqCtx->pName);
            FREE(mqCtx->Nodes);
            FREE(mqCtx);
            return NULL;
        }
    }

    mqCtx->nMaxMessageNum = nMaxMessageNum;
    mqCtx->nMessageSize = nMessageSize;

    MUTEX_INIT(&mqCtx->mutex, NULL);
    SEM_INIT(&mqCtx->sem, 0, 0);

    return (CdcMessageQueue*)mqCtx;
}

void CdcMessageQueueDestroy(CdcMessageQueue* mq)
{
    MessageQueueContext* mqCtx;

    mqCtx = (MessageQueueContext*)mq;

    int i;
    for (i = 0; i < mqCtx->nMaxMessageNum; i++)
        FREE(mqCtx->Nodes[i].msg);

    if(mqCtx->Nodes != NULL)
    {
        FREE(mqCtx->Nodes);
    }

    MUTEX_DESTROY(&mqCtx->mutex);
    SEM_DESTROY(&mqCtx->sem);

    if(mqCtx->pName != NULL)
        FREE(mqCtx->pName);

    FREE(mqCtx);

    return;
}

int CdcMessageQueuePostMessage(CdcMessageQueue* mq, CdcMessage* m)
{
    MessageQueueContext* mqCtx;
    MessageNode*         node;
    MessageNode*         ptr;
    int                  i;

    mqCtx = (MessageQueueContext*)mq;

    MUTEX_LOCK(&mqCtx->mutex);

    if(mqCtx->nCount >= mqCtx->nMaxMessageNum)
    {
        loge("%s, message count exceed, current message count = %d, max message count = %d, msgId = %d",
                mqCtx->pName, mqCtx->nCount, mqCtx->nMaxMessageNum, m->messageId);
        MUTEX_UNLOCK(&mqCtx->mutex);
        return -1;
    }

    node = NULL;
    ptr  = mqCtx->Nodes;
    for(i=0; i<mqCtx->nMaxMessageNum; i++, ptr++)
    {
        if(ptr->valid == 0)
        {
            node = ptr;
            break;
        }
    }

	if (node) {
		memcpy(node->msg, m, mqCtx->nMessageSize);
		node->valid         = 1;
		node->next          = NULL;
	}

    ptr = mqCtx->pHead;
    if(ptr == NULL)
        mqCtx->pHead = node;
    else
    {
        while(ptr->next != NULL)
            ptr = ptr->next;

        ptr->next = node;
    }

    mqCtx->nCount++;

    MUTEX_UNLOCK(&mqCtx->mutex);

    SEM_POST(&mqCtx->sem);

    return 0;
}

int CdcMessageQueueWaitMessage_0(CdcMessageQueue* mq, int64_t timeout)
{
    if (CdcSemTimedWait(&mq->sem, timeout) < 0)
        return -1;

    SEM_POST(&mq->sem);
    return mq->nCount;
}

int CdcMessageQueueGetMessage(CdcMessageQueue* mq, CdcMessage* m)
{
    CEDARC_UNUSE(CdcMessageQueueGetMessage);
    return CdcMessageQueueTryGetMessage(mq, m, -1);
}

int CdcMessageQueueTryGetMessage(CdcMessageQueue* mq, CdcMessage* m, int64_t timeout)
{
    MessageQueueContext* mqCtx;
    MessageNode*         node;

    mqCtx = (MessageQueueContext*)mq;

    if(CdcSemTimedWait(&mqCtx->sem, timeout) < 0)
    {
        return -1;
    }

    MUTEX_LOCK(&mqCtx->mutex);

    if(mqCtx->nCount <= 0)
    {
        logv("%s, no message.", mqCtx->pName);
        MUTEX_UNLOCK(&mqCtx->mutex);
        return -1;
    }

    node = mqCtx->pHead;
    mqCtx->pHead = node->next;

    memcpy(m, node->msg, mqCtx->nMessageSize);
    node->valid = 0;

    mqCtx->nCount--;

    MUTEX_UNLOCK(&mqCtx->mutex);

    return 0;
}

int CdcMessageQueueFlush(CdcMessageQueue* mq)
{
    CEDARC_UNUSE(CdcMessageQueueFlush);
    MessageQueueContext* mqCtx;
    int                  i;

    mqCtx = (MessageQueueContext*)mq;

    logi("%s, flush messages.", mqCtx->pName);

    MUTEX_LOCK(&mqCtx->mutex);

    mqCtx->pHead  = NULL;
    mqCtx->nCount = 0;
    for(i=0; i<mqCtx->nMaxMessageNum; i++)
    {
        mqCtx->Nodes[i].valid = 0;
    }
//this func not use, comment.
    // do
    // {
    //     if(sem_getvalue(&mqCtx->sem, &i) != 0 || i == 0)
    //         break;

    //     sem_trywait(&mqCtx->sem);

    // } while(1);

    MUTEX_UNLOCK(&mqCtx->mutex);

    return 0;
}

int CdcMessageQueueGetCount(CdcMessageQueue* mq)
{
    CEDARC_UNUSE(CdcMessageQueueGetCount);
    MessageQueueContext* mqCtx;

    mqCtx = (MessageQueueContext*)mq;

    return mqCtx->nCount;
}

int CdcSemTimedWait(SEM_STRUCT* sem, int64_t time_ms)
{
    int err = 0;
#ifdef CONFIG_VIDEO_RT_MEDIA
    long int jiff;

    jiff = time_ms/(1000/HZ);

    if(time_ms == -1)
    {
        down(sem);
    }
    else
    {
        err = down_timeout(sem, jiff);
    }
#else
    if(time_ms == -1)
    {
        err = SEM_WAIT(sem);
    }
    else
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += time_ms % 1000 * 1000 * 1000;
        ts.tv_sec += time_ms / 1000 + ts.tv_nsec / (1000 * 1000 * 1000);
        ts.tv_nsec = ts.tv_nsec % (1000*1000*1000);

        err = sem_timedwait(sem, &ts);
    }
#endif
    return err;
}


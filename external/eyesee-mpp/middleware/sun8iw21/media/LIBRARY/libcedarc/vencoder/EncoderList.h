/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : DecoderList.h
* Description :
* Cedarx framework.
* Copyright (c) 2008-2015 Allwinner Technology Co. Ltd.
* Copyright (c) 2014 BZ Chen <bzchen@allwinnertech.com>
*
* This file is part of Cedarx.
*
* Cedarx is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.

* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifndef ENCODER_LIST_H
#define ENCODER_LIST_H

static inline void EncoderPrefetchIon(const void *x)
{
    (void)x;
}

struct EncoderListNodeS
{
    struct EncoderListNodeS *next;
    struct EncoderListNodeS *prev;
};

struct EncoderListS
{
    struct EncoderListNodeS *head;
    struct EncoderListNodeS *tail;
};

#define EncoderListInit(list) do { \
    (list)->head = (list)->tail = (struct EncoderListNodeS *)(list);\
    }while (0)

#define EncoderListNodeInit(node) do { \
    (node)->next = (node)->prev = (node);\
    }while (0)

typedef struct EncoderListNodeS EncoderListNodeT;
typedef struct EncoderListS EncoderListT;

#define EncoderOffsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define EncoderContainerOf(ptr, type, member) ({ \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - EncoderOffsetof(type,member) ); })


#define EncoderListEntry(ptr, type, member) \
    EncoderContainerOf(ptr, type, member)

#define EncoderListFirstEntry(ptr, type, member) \
    EncoderListEntry((ptr)->head, type, member)

#define EncoderListForEach(pos, list) \
    for (pos = (list)->head; \
            pos != (struct EncoderListNodeS *)(list);\
            pos = pos->next)

#define EncoderListForEachPrev(pos, list) \
    for (pos = (list)->tail; \
        pos != (struct EncoderListNodeS *)(list); \
        pos = pos->prev)

#define EncoderListForEachSafe(pos, n, list) \
    for (pos = (list)->head, n = pos->next; \
        pos != (struct EncoderListNodeS *)(list); \
        pos = n, n = pos->next)

#define EncoderListForEachSafeIon(pos, n, list) \
        for (pos = (list)->next, n = pos->next; pos != (list); \
        pos = n, n = pos->next)

#define EncoderListForEachPrevSafe(pos, n, list) \
    for (pos = (list)->tail, n = pos->prev; \
         pos != (struct EncoderListNodeS *)(list); \
         pos = n, n = pos->prev)

#define EncoderListForEachEntry(pos, list, member)                \
    for (pos = EncoderListEntry((list)->head, typeof(*pos), member);    \
         &pos->member != (struct EncoderListNodeS *)(list);     \
         pos = EncoderListEntry(pos->member.next, typeof(*pos), member))

#define EncoderListForEachEntryIon(pos, list, member) \
for (pos = EncoderListEntry((list)->next, typeof(*pos), member); \
     EncoderPrefetchIon(pos->member.next), &pos->member != (list);  \
     pos = EncoderListEntry(pos->member.next, typeof(*pos), member))

#define EncoderListForEachEntryReverse(pos, list, member)            \
    for (pos = EncoderListEntry((list)->tail, typeof(*pos), member);    \
         &pos->member != (struct EncoderListNodeS *)(list);     \
         pos = EncoderListEntry(pos->member.prev, typeof(*pos), member))

#define EncoderListForEachEntrySafe(pos, n, list, member)            \
    for (pos = EncoderListEntry((list)->head, typeof(*pos), member),    \
        n = EncoderListEntry(pos->member.next, typeof(*pos), member);    \
         &pos->member != (struct EncoderListNodeS *)(list);                     \
         pos = n, n = EncoderListEntry(n->member.next, typeof(*n), member))

#define EncoderListForEachEntrySafeReverse(pos, n, list, member)        \
    for (pos = EncoderListEntry((list)->prev, typeof(*pos), member),    \
        n = EncoderListEntry(pos->member.prev, typeof(*pos), member);    \
         &pos->member != (struct EncoderListNodeS *)(list);                     \
         pos = n, n = EncoderListEntry(n->member.prev, typeof(*n), member))

#define ENCODER_LIST_POISON1  ((void *) 0x00700700)
#define ENCODER_LIST_POISON2  ((void *) 0x00900900)

static inline void __EncoderListAdd(struct EncoderListNodeS *newList,
                        struct EncoderListNodeS *prev, struct EncoderListNodeS *next)
{
    next->prev = newList;
    newList->next = next;
    newList->prev = prev;
    prev->next = newList;
}

static inline void EncoderListAdd(struct EncoderListNodeS *newList, struct EncoderListS *list)
{
    __EncoderListAdd(newList, (struct EncoderListNodeS *)list, list->head);
}

static inline void EncoderListAddBefore(struct EncoderListNodeS *newList,
                                    struct EncoderListNodeS *pos)
{
    __EncoderListAdd(newList, pos->prev, pos);
}

static inline void EncoderListAddAfter(struct EncoderListNodeS *newList,
                                    struct EncoderListNodeS *pos)
{
    __EncoderListAdd(newList, pos, pos->next);
}

static inline void EncoderListAddTail(struct EncoderListNodeS *newList, struct EncoderListS *list)
{
    __EncoderListAdd(newList, list->tail, (struct EncoderListNodeS *)list);
}

static inline void __EncoderListDel(struct EncoderListNodeS *prev, struct EncoderListNodeS *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void EncoderListDel(struct EncoderListNodeS *node)
{
    __EncoderListDel(node->prev, node->next);
    node->next = ENCODER_LIST_POISON1;
    node->prev = ENCODER_LIST_POISON2;
}

static inline int EncoderListEmpty(const struct EncoderListS *list)
{
    return (list->head == (struct EncoderListNodeS *)list)
           && (list->tail == (struct EncoderListNodeS *)list);
}

#endif

#ifdef __cplusplus
    }
#endif


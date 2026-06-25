/** @file
  cache xencStream <n>seconds, for impact file generation of CDR.

  @author eric_wang@allwinnertech.com
  @date 2023-06-05
*/

#include <algorithm>

//#define LOG_NDEBUG 0
//#define LOG_TAG "MuxCacheManager"
#include <utils/plat_log.h>

#include "MuxCacheManager.h"

namespace EyeseeLinux {

/**
  try to prefetch first stream node, first from using list, then from ready list.
*/
MuxStreamNode* MuxCacheManager::StreamBufManager::PrefetchFirstStreamNode()
{
    MuxStreamNode *pStreamNode = NULL;
    if(mStreamNodeUsingList.size() > 0)
    {
        pStreamNode = &mStreamNodeUsingList.front();
        if(pStreamNode->mRefCnt <= 0)
        {
            aloge("fatal error! streamId[%d]BufManager using stream node refCnt[%d-%d] <= 0!", mStreamId, pStreamNode->mStreamType, pStreamNode->mRefCnt);
        }
    }
    if(NULL == pStreamNode)
    {
        if(mStreamNodeReadyList.size() > 0)
        {
            pStreamNode = &mStreamNodeReadyList.front();
        }
    }
    return pStreamNode;
}

/**
  get stream node from ready list, then move it to using list, set refCnt to 1.

  @return
    NULL: if no node in ready list.
*/
MuxStreamNode* MuxCacheManager::StreamBufManager::GetStreamNode()
{
    int nReadyNum = mStreamNodeReadyList.size();
    if(nReadyNum > 0)
    {
        mStreamNodeUsingList.splice(mStreamNodeUsingList.end(), mStreamNodeReadyList, mStreamNodeReadyList.begin());
        MuxStreamNode *pNode = &mStreamNodeUsingList.back();
        pNode->mRefCnt = 1;
        return pNode;
    }
    else
    {
        return NULL;
    }
}

status_t MuxCacheManager::StreamBufManager::RefStreamNode(MuxStreamNode *pStreamNode)
{
    pStreamNode->mRefCnt++;
    return NO_ERROR;
}

/**
  release stream node in using list to streamBufManager.

  @return
    NO_ERROR
    UNKNOWN_ERROR
*/
status_t MuxCacheManager::StreamBufManager::ReleaseStreamNode(int nNodeId)
{
    int nUsingNum = mStreamNodeUsingList.size();
    if(nUsingNum > 0)
    {
        std::list<MuxStreamNode>::iterator it;
        for(it=mStreamNodeUsingList.begin(); it!=mStreamNodeUsingList.end(); ++it)
        {
            if(it->GetStreamNodeId() == nNodeId)
            {
                break;
            }
        }
        if(it == mStreamNodeUsingList.end())
        {
            aloge("fatal error! streamId[%d]BufManager not find nodeId[%d]", mStreamId, nNodeId);
            return UNKNOWN_ERROR;
        }
        if(it->mRefCnt > 0)
        {
            it->mRefCnt--;
            if(0 == it->mRefCnt)
            {
                if(it != mStreamNodeUsingList.begin())
                {
                    aloge("fatal error! nodeId[%d-%d] to be removed must be first node of using list!", mStreamId, nNodeId);
                }
                //1. release stream buf, update readPtr and validLen.
                ReleaseStreamBuf(*it);
                //2. remove from using list
                mStreamNodeUsingList.erase(it);
            }
        }
        else
        {
            aloge("fatal error! check code! release stream node info[%d-%d-%d]", mStreamId, nNodeId, it->mRefCnt);
        }
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! streamId[%d]BufManager not find nodeId[%d], using list is empty!", mStreamId, nNodeId);
        return UNKNOWN_ERROR;
    }
}

/**
  release stream node in ready list to streamBufManager.

  @return
    NO_ERROR
    UNKNOWN_ERROR
*/
status_t MuxCacheManager::StreamBufManager::ReleaseReadyStreamNode(int nNodeId)
{
    int nReadyNum = mStreamNodeReadyList.size();
    if(nReadyNum > 0)
    {
        std::list<MuxStreamNode>::iterator it;
        for(it=mStreamNodeReadyList.begin(); it!=mStreamNodeReadyList.end(); ++it)
        {
            if(it->GetStreamNodeId() == nNodeId)
            {
                break;
            }
        }
        if(it == mStreamNodeReadyList.end())
        {
            aloge("fatal error! streamId[%d]BufManager not find nodeId[%d]", mStreamId, nNodeId);
            return UNKNOWN_ERROR;
        }
        if(0 == it->mRefCnt)
        {
            if(it != mStreamNodeReadyList.begin())
            {
                aloge("fatal error! nodeId[%d-%d] to be removed must be first node of ready list!", mStreamId, nNodeId);
            }
            //1. release stream buf, update readPtr and validLen.
            ReleaseStreamBuf(*it);
            //2. remove from ready list
            mStreamNodeReadyList.erase(it);
        }
        else
        {
            aloge("fatal error! check code! release stream node info[%d-%d-%d]", mStreamId, nNodeId, it->mRefCnt);
        }
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! streamId[%d]BufManager not find nodeId[%d], ready list is empty!", mStreamId, nNodeId);
        return UNKNOWN_ERROR;
    }
}

/**
  get total duration, including using list and ready list.

  @return
    >=0:unit:ms.
    -1: error.
*/
int MuxCacheManager::StreamBufManager::GetDuration()
{
    int64_t nFrontPts = -1; //unit:us
    int64_t nBackPts = -1;
    if(mStreamNodeUsingList.size() > 0)
    {
        nFrontPts = mStreamNodeUsingList.front().GetStreamPts();
        nBackPts = mStreamNodeUsingList.back().GetStreamPts();
    }
    if(mStreamNodeReadyList.size() > 0)
    {
        if(-1 == nFrontPts)
        {
            nFrontPts = mStreamNodeReadyList.front().GetStreamPts();
        }
        nBackPts = mStreamNodeReadyList.back().GetStreamPts();
    }
    return (int)((nBackPts - nFrontPts)/1000);
}

MuxCacheManager::StreamBufManager::StreamBufManager(int nStreamId, int nBufSize)
{
    mStreamId = nStreamId;
    if(nBufSize > 0)
    {
        mpBuf = (char*)malloc(nBufSize);
        if(mpBuf != NULL)
        {
            mnBufSize = nBufSize;
        }
        else
        {
            aloge("fatal error! malloc fail");
            mnBufSize = 0;
        }
    }
    else
    {
        mpBuf = NULL;
        mnBufSize = 0;
    }
    if(mpBuf)
    {
        mpWritePos = mpBuf;
        mpReadPos = mpBuf;
    }
    else
    {
        mpWritePos = NULL;
        mpReadPos = NULL;
    }
    mnValidLen = 0;
}
MuxCacheManager::StreamBufManager::~StreamBufManager()
{
    int nReadyNum = mStreamNodeReadyList.size();
    int nUsingNum = mStreamNodeUsingList.size();
    alogd("streamBufMgr[%d]:[%d-%d]nodes, dataLen:%d/%d", mStreamId, nReadyNum, nUsingNum, mnValidLen, mnBufSize);
    if(nUsingNum != 0)
    {
        aloge("fatal error! why streamBufMgr[%d] usingNum[%d]!=0", mStreamId, nUsingNum);
    }
    if(mpBuf)
    {
        free(mpBuf);
        mpBuf = NULL;
    }
}

/**
  update stream buf manager's mpReadPos and mnValidLen to release buf.
*/
status_t MuxCacheManager::StreamBufManager::ReleaseStreamBuf(MuxStreamNode& streamNode)
{
    char *pDatas[3] = {NULL, NULL, NULL};
    int nDataLens[3] = {0, 0, 0};
    if(MuxStreamNode::Type::Video == streamNode.mStreamType)
    {
        pDatas[0] = (char*)streamNode.mVencStream.mpPack[0].mpAddr0;
        pDatas[1] = (char*)streamNode.mVencStream.mpPack[0].mpAddr1;
        pDatas[2] = (char*)streamNode.mVencStream.mpPack[0].mpAddr2;
        nDataLens[0] = streamNode.mVencStream.mpPack[0].mLen0;
        nDataLens[1] = streamNode.mVencStream.mpPack[0].mLen1;
        nDataLens[2] = streamNode.mVencStream.mpPack[0].mLen2;
        if(pDatas[2]!=NULL || nDataLens[2]!=0)
        {
            aloge("fatal error! check code![%p-%d]", pDatas[2], nDataLens[2]);
        }
    }
    else if(MuxStreamNode::Type::Audio == streamNode.mStreamType)
    {
        pDatas[0] = (char*)streamNode.mAencStream.pStream;
        pDatas[1] = (char*)streamNode.mAencStream.pStreamExtra;
        nDataLens[0] = streamNode.mAencStream.mLen;
        nDataLens[1] = streamNode.mAencStream.mExtraLen;
    }
    else if(MuxStreamNode::Type::Text == streamNode.mStreamType)
    {
        pDatas[0] = (char*)streamNode.mTencStream.pStream;
        pDatas[1] = (char*)streamNode.mTencStream.pStreamExtra;
        nDataLens[0] = streamNode.mTencStream.mLen;
        nDataLens[1] = streamNode.mTencStream.mExtraLen;
    }
    else
    {
        aloge("fatal error! unknown streamType:%d", streamNode.mStreamType);
    }
    if(mpReadPos == pDatas[0])
    {
        if(0 == nDataLens[1])
        {
            mpReadPos = pDatas[0] + nDataLens[0];
            if(mpReadPos >= mpBuf + mnBufSize)
            {
                if(mpReadPos > mpBuf + mnBufSize)
                {
                    aloge("fatal error! check streamId[%d]Buf info:[%p-%d,%p-%d-%p-%d]", mStreamId, mpReadPos, mnValidLen, pDatas[0], nDataLens[0], pDatas[1], nDataLens[1]);
                }
                mpReadPos = mpBuf;
            }
            mnValidLen -= nDataLens[0];
        }
        else
        {
            mpReadPos = pDatas[1] + nDataLens[1];
            mnValidLen -= (nDataLens[0] + nDataLens[1]);
            if(pDatas[1] != mpBuf)
            {
                aloge("fatal error! check streamId[%d]Buf info:[%p-%d,%p-%d-%p-%d]", mStreamId, mpReadPos, mnValidLen, pDatas[0], nDataLens[0], pDatas[1], nDataLens[1]);
            }
            if(mpReadPos >= mpBuf + mnBufSize)
            {
                aloge("fatal error! check streamId[%d]Buf info:[%p-%d,%p-%d-%p-%d]", mStreamId, mpReadPos, mnValidLen, pDatas[0], nDataLens[0], pDatas[1], nDataLens[1]);
            }
        }
    }
    else
    {
        aloge("fatal error! check streamId[%d]Buf info:[%p-%d,%p-%d-%p-%d]", mStreamId, mpReadPos, mnValidLen, pDatas[0], nDataLens[0], pDatas[1], nDataLens[1]);
    }
    return NO_ERROR;
}

status_t MuxCacheManager::AddStreamBufManager(int nStreamId, int nBufSize)
{
    std::lock_guard<std::mutex> autoLock(mLock);
    mStreamBufManagerList.emplace_back(nStreamId, nBufSize);
    return NO_ERROR;
}

/**
  control every stream buf manager cache time to mCacheTime.

  not try to keep first video frame is key frame. when use stream buf, user can discard non-keyframes.
  To save time here, we need reduce wait chance to increase speed.
*/
status_t MuxCacheManager::ControlCacheLevel()
{
    std::unique_lock<std::mutex> autoLock(mLock);
    StreamBufManager *pRefStreamManager = NULL;
    int64_t nRefStreamFrontPts = -1; //unit:us
    //control cache of every stream buf.
    for(StreamBufManager& streamManager : mStreamBufManagerList)
    {
        MuxStreamNode *pFirstNode = NULL;
        int nCurDuration = 0; //unit:ms
        //control cache time begin,
        while(1)
        {
            nCurDuration = streamManager.GetDuration();
            if(nCurDuration <= mCacheTime)
            {
                break;
            }
            //try to release oldest stream.
            pFirstNode = streamManager.PrefetchFirstStreamNode();
            if(0 == pFirstNode->mRefCnt)
            {
                streamManager.ReleaseReadyStreamNode(pFirstNode->GetStreamNodeId());
            }
            else if(pFirstNode->mRefCnt > 0)
            {
                //wait cache duration ok or using list empty!
                mbWaitReleasePacketFlag = true;
                bool bSuccess = false;
                int nTimeout = 2*1000; //unit:ms
                auto stop_waiting = [&]
                {
                    bool bStop = false;
                    int nCurDuration = streamManager.GetDuration();
                    if(nCurDuration <= mCacheTime)
                    {
                        bStop = true;
                    }
                    if(streamManager.mStreamNodeUsingList.empty())
                    {
                        bStop = true;
                    }
                    return bStop;
                };
                bSuccess = mcvReleaseUsingPacket.wait_for(autoLock, std::chrono::milliseconds(nTimeout), stop_waiting);
                if(bSuccess)
                {
                }
                else
                {
                    alogw("Be careful! streamId[%d] buf: wait free memory timeout[%d]ms", streamManager.mStreamId, nTimeout);
                }
                mbWaitReleasePacketFlag = false;
            }
            else
            {
                aloge("fatal error! streamId[%d]BufManager stream node refcnt:%d < 0!", streamManager.mStreamId, pFirstNode->mRefCnt);
            }
        }
        //control cache time end.
    }
    return NO_ERROR;
}

MuxCacheManager::MuxCacheManager(int nCacheTime, EyeseeRecorder *pRecorder)
{
    mbWaitReleasePacketFlag = false;
    mCacheTime = nCacheTime;
    mpOwner = pRecorder;
}

MuxCacheManager::~MuxCacheManager()
{
    std::lock_guard<std::mutex> autoLock(mLock);
    mStreamBufManagerList.clear();
}

}


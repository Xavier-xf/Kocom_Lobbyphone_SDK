/** @file
  provide a class to manage message list.
  use shared_ptr to send message, support message reply to implement sync waiting.

  usage without SmartMessage::MessageReply:
    sender:
        class DerivedMsgContent: public BaseMsgContent
        {
            ...
        };
        DerivedMsgContent *pDerived = new DerivedMsgContent();
        pDerived->... = ...;

        SmartMessage stSendMsg;
        stSendMsg.what = 1;
        stSendMsg.args.push_back(2);
        stSendMsg.args.push_back(...);
        stSendMsg.spData = std::shared_ptr<BaseMsgContent>(pDerived);
        pMsgMgr->PutMessage(stSendMsg);

    receiver:
        pMsgMgr->GetMessage(stMsgOut);
        std::shared_ptr<DerivedMsgContent> spData = std::static_pointer_cast<DerivedMsgContent>(stMsgOut.spData);

  usage with SmartMessage::MessageReply:
    sender:
        class FaceDetectResult: public BaseMsgContent
        {
        public:
            std::vector<bool> mFaceInpolyFlags;
            int mFaceInpolyCount;
            std::vector<int> mFaceScores;
            ~FaceDetectResult()
            {
                alogd("~FaceDetectResult is called");
            };
        };
        
        class FaceDetectResultReply: public BaseMsgContent
        {
        public:
            int mParam0;
            std::vector<int> mReplyDatas;
            ~FaceDetectResultReply()
            {
                alogd("~FaceDetectResultReply is called");
            }
        };

        FaceDetectResult *pFaceDetectResult = new FaceDetectResult();
        pFaceDetectResult->... = ...;

        SmartMessage stSendMsg;
        stSendMsg.what = 2;
        stSendMsg.args.push_back(5);
        stSendMsg.spData = std::shared_ptr<BaseMsgContent>(pFaceDetectResult);
        stSendMsg.spMsgReply = std::make_shared<SmartMessage::MessageReply>();
        pMsgMgr->PutMessage(stSendMsg);

        //wait message reply
        int waitRet = stSendMsg.spMsgReply->WaitReply(1000);
        if(0 == waitRet)
        {
            //read message reply information
            alogd("receive FaceDetectResult reply: 0x%x", stSendMsg.spMsgReply->mnReplyResult);
            FaceDetectResultReply *pFaceDetectResultReply = (FaceDetectResultReply*)stSendMsg.spMsgReply->mpReplyData;
            alogd("receive FaceDetectResult replyData:%d,%ld-%d-%d", pFaceDetectResultReply->mParam0,
                pFaceDetectResultReply->mReplyDatas.size(), 
                pFaceDetectResultReply->mReplyDatas[0], pFaceDetectResultReply->mReplyDatas[1]);
        }
        else
        {
            aloge("fatal error! wait message reply timeout, ret:%d", waitRet);
        }

    receiver:
        pMsgMgr->GetMessage(stMsgOut);
        if(2 == stMsgOut.what) //FaceDetectResult
        {
            std::shared_ptr<FaceDetectResult> spFaceDetectResult = std::static_pointer_cast<FaceDetectResult>(stMsgOut.spData);
            alogd("receive face detect result:%d,%d-%d-%d,%d-%d", stMsgOut.args[0],
                spFaceDetectResult->mFaceInpolyCount,
                (bool)spFaceDetectResult->mFaceInpolyFlags[0], (bool)spFaceDetectResult->mFaceInpolyFlags[1],
                spFaceDetectResult->mFaceScores[0], spFaceDetectResult->mFaceScores[1]);

            //sumiluate processing duration
            sleep(2);

            //after processing done, use msgReply to notify sender.
            if(stMsgOut.spMsgReply != NULL)
            {
                stMsgOut.spMsgReply->mnReplyResult = 15;
                FaceDetectResultReply *pFaceDetectResultReply = new FaceDetectResultReply();
                pFaceDetectResultReply->mParam0 = 11;
                pFaceDetectResultReply->mReplyDatas = {8,7,6,5,4};
                stMsgOut.spMsgReply->mpReplyData = pFaceDetectResultReply; //derived class type is based on what.

                stMsgOut.spMsgReply->NotifyOne();
            }
        }

  @author eric_wang@allwinnertech.com
  @date 2023-04-15
*/

#include <algorithm>

//#define LOG_NDEBUG 0
//#define LOG_TAG "MessageManager"
#include <utils/plat_log.h>

#include "MessageManager.h"

namespace EyeseeLinux {

BaseMsgContent::~BaseMsgContent()
{
    //alogd("~BaseMsgContent is called");
}

/**
  receiver use it to notify sender reply is sent.
*/
void SmartMessage::MessageReply::NotifyOne()
{
    std::lock_guard<std::mutex> autoLock(mReplyMutex);
    mnReplySemValue++;
    mReplyCV.notify_one();
}

/**
  sender use it to wait reply.

  @param[in] nTimeout
    unit:ms,
    > 0: timeout duration.
    <=0: wait forever.
  @return
    0: success
    -1: fail, timeout.
*/
int SmartMessage::MessageReply::WaitReply(int nTimeout)
{
    std::unique_lock<std::mutex> autoLock(mReplyMutex);
    if(mnReplySemValue > 0)
    {
        return 0;
    }
    auto stop_waiting = [&]
    {
        bool bStop = true;
        if(mnReplySemValue <= 0)
        {
            //alogw("Be careful! wait msgReply spurious wakeup! continue");
            bStop = false;
        }
        return bStop;
    };
    bool bSuccess = false;
    //wait message reply
    if(nTimeout <= 0)
    {
        mReplyCV.wait(autoLock, stop_waiting);
        bSuccess = true;
    }
    else
    {
        bSuccess = mReplyCV.wait_for(autoLock, std::chrono::milliseconds(nTimeout), stop_waiting);
    }
    if(bSuccess)
    {
        if(mnReplySemValue <= 0)
        {
            aloge("fatal error! why message reply success but semValue[%d]<=0?", mnReplySemValue);
        }
        return 0;
    }
    else
    {
        alogd("wait message reply timeout[%d]ms", nTimeout);
        if(mnReplySemValue > 0)
        {
            aloge("fatal error! why semValue[%d] > 0 when timeout[%d]ms?", mnReplySemValue, nTimeout);
        }
        return -1;
    }
}

SmartMessage::MessageReply::MessageReply()
{
    mnReplyResult = 0;
    mpReplyData = NULL;
    mnReplySemValue = 0;
}
SmartMessage::MessageReply::~MessageReply()
{
    if(mpReplyData)
    {
        delete mpReplyData;
    }
}
/**
  clear smart message content.

  @return
    0: success
*/
int SmartMessage::Reset()
{
    what = 0;
    args.clear();
    spData = NULL;
    spMsgReply = NULL;
    return 0;
}

SmartMessage& SmartMessage::operator=(const SmartMessage& lRef)
{
    what = lRef.what;
    args = lRef.args;
    spData = lRef.spData;
    spMsgReply = lRef.spMsgReply;
    return *this;
}

SmartMessage& SmartMessage::operator=(SmartMessage&& rRef)
{
    what = rRef.what;
    args = std::move(rRef.args);
    spData = std::move(rRef.spData);
    spMsgReply = std::move(rRef.spMsgReply);
    return *this;
}

SmartMessage::SmartMessage()
{
    what = 0;
    args.reserve(2);
}

SmartMessage::SmartMessage(const SmartMessage& lRef)
{
    *this = lRef;
}

SmartMessage::SmartMessage(SmartMessage&& rRef)
{
    *this = rRef;
}

SmartMessage::~SmartMessage()
{
    //alogd("~SmartMessage is called");
}

/**
  put message to interval valid list.
  caller provide message instance, callee copy assignment from it.

  @return
    0: success
    -1: fail
*/
int MessageManager::PutMessage(const SmartMessage& MsgIn)
{
    std::lock_guard<std::mutex> autoLock(mLock);
    //find idle message
    if(mIdleMsgList.empty())
    {
        mIdleMsgList.resize(1);
        mCount++;
        if(0 == mCount%100)
        {
            aloge("fatal error! message count[%d] too many", mCount);
        }
    }
    mIdleMsgList.front() = MsgIn;
    mValidMsgList.splice(mValidMsgList.end(), mIdleMsgList, mIdleMsgList.begin());
    if(mWaitMsgFlag)
    {
        mcvMsgListChanged.notify_one();
        mWaitMsgFlag = false;
    }
    return 0;
}

/**
  get message from valid list.
  caller provide instance, callee fill it.

  @return
    0: success
    -1: fail
*/
int MessageManager::GetMessage(SmartMessage& MsgOut)
{
    std::lock_guard<std::mutex> autoLock(mLock);
    if(mValidMsgList.empty())
    {
        return -1;
    }
    MsgOut = std::move(mValidMsgList.front());
    mIdleMsgList.splice(mIdleMsgList.end(), mValidMsgList, mValidMsgList.begin());
    return 0;
}

/**
  wait valid message to coming.

  @param timeout
    <=0: wait forever
    >0: unit:ms
  @return
    valid message number.
*/
int MessageManager::WaitMessage(unsigned int timeout)
{
    std::unique_lock<std::mutex> autoLock(mLock);
    if(!mValidMsgList.empty())
    {
        return mValidMsgList.size();
    }
    mWaitMsgFlag = true;
    bool bSuccess = false;
    auto stop_waiting = [&]
    {
        bool bStop = !mValidMsgList.empty();
        if(false == bStop)
        {
            //alogw("Be careful! spurious wakeup! continue");
        }
        return bStop;
    };
    if(timeout<=0)
    {
        mcvMsgListChanged.wait(autoLock, stop_waiting);
        bSuccess = true;
    }
    else
    {
        bSuccess = mcvMsgListChanged.wait_for(autoLock, std::chrono::milliseconds(timeout), stop_waiting);
    }
    if(bSuccess)
    {
        if(mValidMsgList.empty())
        {
            aloge("fatal error! why message list is empty, but wait success?");
        }
    }
    else
    {
        //alogd("wait message timeout[%d]ms", timeout);
    }
    mWaitMsgFlag = false;
    return mValidMsgList.size();
}

void MessageManager::ClearMessageList()
{
    std::lock_guard<std::mutex> autoLock(mLock);
    /*
    for(std::list<SmartMessage>::iterator it = mValidMsgList.begin(); it != mValidMsgList.end(); ++it)
    {
        it->Reset();
    }
    */
    /*
    for(auto& i : mValidMsgList)
    {
        i.Reset();
    }
    */
    std::for_each( std::begin(mValidMsgList), std::end(mValidMsgList), [](SmartMessage& i) {
        i.Reset();
    });
    
    mIdleMsgList.splice(mIdleMsgList.end(), mValidMsgList);
}

/**
  get valid message count.
*/
int MessageManager::GetMessageCount()
{
    std::lock_guard<std::mutex> autoLock(mLock);
    return (int)mValidMsgList.size();
}

MessageManager::MessageManager(int nMsgCount)
    :mCount(nMsgCount)
    ,mIdleMsgList(nMsgCount)
{
    mWaitMsgFlag = false;
}

MessageManager::~MessageManager()
{
    //alogd("~MessageManager is called");
    std::lock_guard<std::mutex> autoLock(mLock);
    if(mValidMsgList.size() > 0)
    {
        alogw("Be careful! has %ld msg not processed when destructing MessageManager", mValidMsgList.size());
    }
    alogd("contain [%d]msg when destruct", mCount);
}

}


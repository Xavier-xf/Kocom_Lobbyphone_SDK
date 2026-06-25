/** @file
  mux stream node implementation.

  @author eric_wang@allwinnertech.com
  @date 2023-06-05
*/

//#define LOG_NDEBUG 0
//#define LOG_TAG "MuxStreamNode"
#include <utils/plat_log.h>

#include "MuxStreamNode.h"

namespace EyeseeLinux {

int MuxStreamNode::GetStreamLen()
{
    int nLen = 0;
    switch(mStreamType)
    {
        case Type::Video:
        {
            nLen = mVencStream.mpPack[0].mLen0 + mVencStream.mpPack[0].mLen1 + mVencStream.mpPack[0].mLen2;
            break;
        }
        case Type::Audio:
        {
            nLen = mAencStream.mLen + mAencStream.mExtraLen;
            break;
        }
        case Type::Text:
        {
            nLen = mTencStream.mLen + mTencStream.mExtraLen;
            break;
        }
        default:
        {
            aloge("fatal error! check stream type:%d", mStreamType);
            break;
        }
    }
    return nLen;
}

int64_t MuxStreamNode::GetStreamPts()
{
    int64_t nPts = -1;
    switch(mStreamType)
    {
        case Type::Video:
        {
            nPts = mVencStream.mpPack[0].mPTS;
            break;
        }
        case Type::Audio:
        {
            nPts = mAencStream.mTimeStamp;
            break;
        }
        case Type::Text:
        {
            nPts = mTencStream.mTimeStamp;
            break;
        }
        default:
        {
            aloge("fatal error! check stream type:%d", mStreamType);
            break;
        }
    }
    return nPts;
}

/**
  if video, check if is key frame. if audio, every frame is key frame.

  @return
    true: key frame.
    false: not key frame.
*/
bool MuxStreamNode::CheckKeyFrameFlag(PAYLOAD_TYPE_E ePayloadType)
{
    bool bKeyFrame = false;
    switch(mStreamType)
    {
        case Type::Video:
        {
            if(PT_H264 == ePayloadType)
            {
                if(H264E_NALU_ISLICE == mVencStream.mpPack[0].mDataType.enH264EType)
                {
                    bKeyFrame = true;
                }
            }
            else if(PT_H265 == ePayloadType)
            {
                if(H265E_NALU_ISLICE == mVencStream.mpPack[0].mDataType.enH265EType)
                {
                    bKeyFrame = true;
                }
            }
            else
            {
                bKeyFrame = true;
            }
            break;
        }
        default:
        {
            bKeyFrame = true;
            break;
        }
    }
    return bKeyFrame;
}

int MuxStreamNode::GetStreamNodeId()
{
    int nNodeId = 0;
    switch(mStreamType)
    {
        case Type::Video:
        {
            nNodeId = mVencStream.mSeq;
            break;
        }
        case Type::Audio:
        {
            nNodeId = mAencStream.mId;
            break;
        }
        case Type::Text:
        {
            nNodeId = mTencStream.mId;
            break;
        }
        default:
        {
            aloge("fatal error! check stream type:%d", mStreamType);
            break;
        }
    }
    return nNodeId;
}

MuxStreamNode::MuxStreamNode(const VENC_STREAM_S& VEncStream, int nStreamId)
{
    mStreamId = nStreamId;
    mStreamType = Type::Video;
    mVencStream = VEncStream;
    mVencStream.mpPack = new VENC_PACK_S;
    if(NULL==mVencStream.mpPack)
    {
        aloge("fatal error! malloc fail");
    }
    mVencStream.mPackCount = 1;
    mVencStream.mpPack[0] = VEncStream.mpPack[0];
    mRefCnt = 0;
}
MuxStreamNode::MuxStreamNode(const AUDIO_STREAM_S& AEncStream, int nStreamId)
{
    mStreamId = nStreamId;
    mStreamType = Type::Audio;
    mAencStream = AEncStream;
    mRefCnt = 0;
}
MuxStreamNode::MuxStreamNode(const TEXT_STREAM_S& TEncStream, int nStreamId)
{
    mStreamId = nStreamId;
    mStreamType = Type::Text;
    mTencStream = TEncStream;
    mRefCnt = 0;
}
MuxStreamNode::MuxStreamNode(const MuxStreamNode& lRef)
{
    *this = lRef;
    if(Type::Video == mStreamType)
    {
        mVencStream.mpPack = new VENC_PACK_S;
        if(NULL==mVencStream.mpPack)
        {
            aloge("fatal error! malloc fail");
        }
        mVencStream.mPackCount = 1;
        mVencStream.mpPack[0] = lRef.mVencStream.mpPack[0];
    }
}

MuxStreamNode::~MuxStreamNode()
{
    if(Type::Video == mStreamType)
    {
        if(mVencStream.mpPack)
        {
            delete mVencStream.mpPack;
            mVencStream.mpPack = NULL;
        }
    }
}

}


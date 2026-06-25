/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    ise.cpp
 * @brief   ise基类
 * @author  id:826
 * @version v0.3
 * @date    2017-02-16
 */

#include "ise.h"

using namespace EyeseeLinux;
using namespace std;

ISEBase::ISEBase()
    : channel_id_(0)
    , ise_(NULL)
    , status_(ISE_IDLE)
{
}

ISEBase::~ISEBase()
{
}

void ISEBase::onShutter(int chnId)
{
}

void ISEBase::onPictureTaken(int chnId, const void *data, int size, EyeseeISE *pISE)
{
}

void ISEBase::onError(int chnId, int error, EyeseeISE *pISE)
{
}

void ISEBase::onPreviewFrame(const void *data, int size, EyeseeISE *pISE)
{
}

int ISEBase::SetCamera(const std::vector<CameraChannelInfo> &cam_info)
{
    return 0;
}

int ISEBase::Init()
{
    return 0;
}

int ISEBase::SetParameters(const CameraParameters &param)
{
    return 0;
}

int ISEBase::GetParameters(CameraParameters &param) const
{
    return 0;
}

int ISEBase::StartRecord()
{
    return 0;
}

int ISEBase::StopRecord()
{
    return 0;
}

int ISEBase::TakePicture()
{
    return 0;
}

int ISEBase::GetStatus()
{
    return 0;
}

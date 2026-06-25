/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    ise.h
 * @brief   ise基类
 * @author  id:826
 * @version v0.3
 * @date    2017-02-16
 */

#pragma once

#include <EyeseeISE.h>
#include <vector>

namespace EyeseeLinux {

typedef enum {
    ISE_ERROR        = 0,
    ISE_IDLE         = 1 << 0,
    ISE_CONFIGURED   = 1 << 1,
    ISE_PREPARED     = 1 << 2,
    ISE_RECORDING    = 1 << 3,
    ISE_STOPPED      = 1 << 4,
} ISEStatus;


class ISEBase
    : public EyeseeISE::PreviewCallback
    , public EyeseeISE::PictureCallback
    , public EyeseeISE::ErrorCallback
    , public EyeseeISE::ShutterCallback
{
    public:
        ISEBase();

        virtual ~ISEBase();

        virtual void onShutter(int chnId);

        virtual void onPictureTaken(int chnId, const void *data, int size, EyeseeISE *pISE) = 0;

        virtual void onError(int chnId, int error, EyeseeISE *pISE);

        virtual void onPreviewFrame(const void *data, int size, EyeseeISE *pISE);

        virtual int SetCamera(const std::vector<CameraChannelInfo> &cam_info);

        virtual int Init() = 0;

        virtual int SetParameters(const CameraParameters &param);

        virtual int GetParameters(CameraParameters &param) const;

        virtual int StartRecord() = 0;

        virtual int StopRecord();

        virtual int TakePicture();

        virtual int GetStatus();
    private:
        int channel_id_;
        EyeseeISE *ise_;
        ISEStatus status_;
}; // class ISE

} // namespace EyeseeLinux

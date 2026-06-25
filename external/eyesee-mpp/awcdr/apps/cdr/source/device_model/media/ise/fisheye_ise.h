/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    fisheye_ise.h
 * @brief   单目鱼眼
 * @author  id:826
 * @version v0.3
 * @date    2016-12-05
 */

#pragma once

#include "ise.h"

namespace EyeseeLinux {

class FishEyeISE
    : public ISEBase
{
    public:
        FishEyeISE();

        ~FishEyeISE();

        void onPictureTaken(int chnId, const void *data, int size, EyeseeISE *pISE);

        int Init();

        int StartRecord();

        int StopRecord();
}; // FishEyeISE

} // namespace EyeseeLinux

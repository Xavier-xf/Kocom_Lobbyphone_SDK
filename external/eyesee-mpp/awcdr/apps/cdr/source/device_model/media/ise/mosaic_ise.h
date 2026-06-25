/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    mosaic_ise.h
 * @brief   双目拼接
 * @author  id:826
 * @version v0.3
 * @date    2017-02-16
 */

#pragma once

#include "ise.h"

namespace EyeseeLinux {

class MosaicISE
    : public ISEBase
{
    public:
        MosaicISE();

        ~MosaicISE();

        void onPictureTaken(int chnId, const void *data, int size, EyeseeISE *pISE);

        int Init();

        int StartRecord();

        int StopRecord();
}; // MosaicISE

} // namespace EyeseeLinux

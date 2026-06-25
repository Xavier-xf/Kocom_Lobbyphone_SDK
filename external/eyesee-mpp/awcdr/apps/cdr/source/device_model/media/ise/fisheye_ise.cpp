/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    fisheye_ise.cpp
 * @brief   单目鱼眼/双目鱼眼拼接
 * @author  id:826
 * @version v0.3
 * @date    2016-12-05
 */

#include "fisheye_ise.h"

#include <vector>

using namespace EyeseeLinux;
using namespace std;

FishEyeISE::FishEyeISE()
{

}

FishEyeISE::~FishEyeISE()
{

}

void FishEyeISE::onPictureTaken(int chnId, const void *data, int size, EyeseeISE *pISE)
{

}

int FishEyeISE::Init()
{
    return 0;
}

int FishEyeISE::StartRecord()
{
    return 0;
}

int FishEyeISE::StopRecord()
{
    return 0;
}

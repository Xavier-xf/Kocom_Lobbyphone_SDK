# sample_FaceTrack：演示人脸跟踪功能(周界跟踪样例)

| 版本   | 内容     |
| ---- | ------ |
| v0.1 | 编写初始内容 |
|      |        |
|      |        |

## 1. 示例说明

测试设备通过摄像头实时录制视频图像数据，图像数据经过人脸检测算法、跟踪算法处理得到算法检测数据，将算法处理结果数据添加到视频图像中，最后图像数据通过设备的UVC模块传输到PC端显示。(windows下可用PotPlayer工具)

## 2. 运行示例

- 测试设备安装摄像头，通过USB连接PC主机

- 将应用sample_FaceTrack、sample_FaceTrack.conf以及相关的算法模型拷贝到测试目录。

- ./sample_FaceTrack -path sample_FaceTrack.conf   #注意：将人脸检测模型按照conf文件中的设置拷贝到相关的测试目录

## 3. 使能相关配置

- 打开应用

```
 [*]   mpp sample FaceTrack 
```

- 打开内核USB配置

```
make kernel_menuconfig
[*] Device Driver --->
[*] USB support --->
[*] USB Gadget Support --->
[*] USB functions configuarble through configfs
[*] USB Webcam function
```

## 3. 其他

- 内核usb相关代码修改

```
diff --git a/drivers/usb/gadget/function/f_uvc.c b/drivers/usb/gadget/function/f_uvc.c
index a6dc35fc42e0..4d38c254d392 100644
--- a/drivers/usb/gadget/function/f_uvc.c
+++ b/drivers/usb/gadget/function/f_uvc.c
@@ -1040,7 +1040,7 @@ static struct usb_function_instance *uvc_alloc_inst(void)
        cd->wObjectiveFocalLengthMax    = cpu_to_le16(0);
        cd->wOcularFocalLength          = cpu_to_le16(0);
        cd->bControlSize                = 3;
-       cd->bmControls[0]               = 2;
+       cd->bmControls[0]               = 0;
        cd->bmControls[1]               = 0;
        cd->bmControls[2]               = 0;

@@ -1052,7 +1052,7 @@ static struct usb_function_instance *uvc_alloc_inst(void)
        pd->bSourceID                   = 1;
        pd->wMaxMultiplier              = cpu_to_le16(16*1024);
        pd->bControlSize                = 2;
-       pd->bmControls[0]               = 1;
+       pd->bmControls[0]               = 0;
        pd->bmControls[1]               = 0;
        pd->iProcessing                 = 0;
```

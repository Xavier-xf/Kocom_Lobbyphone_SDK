# sample_IntrusionMonitor：演示人形入侵报警功能(周界报警样例)

| 版本   | 内容     |
| ---- | ------ |
| v0.1 | 编写初始内容 |
|      |        |
|      |        |

## 1. 示例说明

测试设备通过摄像头实时录制视频图像数据，图像数据经过人形检测算法判断确定人形目标，根据人形目标的位置以及监控区域的边界范围判断人形目标是否在监控区域内，如果目标在监控区域内，针对目标进行画框打标签并添加到视频图像中，通过UVC设备传输到PC端显示。(windows下可用PotPlayer工具)。应用模拟两个场景：

- 入侵监控区域报警

- 越过警戒线报警

## 2. 运行示例

- 测试设备安装摄像头，通过USB连接PC主机

- 将应用sample_IntrusionMonitor、sample_IntrusionMonitor.conf以及相关的人形算法模型拷贝到测试目录。

- ./sample_IntrusionMonitor -path sample_IntrusionMonitor.conf   #注意：将人形检测模型按照conf文件中的设置拷贝到相关的测试目录

## 3. 使能相关配置

- 打开应用

```
 [*]   mpp sample IntrusionMonitor 
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
        pd->iProcessing                 = 0; [*]   mpp sample IntrusionMonitor
```

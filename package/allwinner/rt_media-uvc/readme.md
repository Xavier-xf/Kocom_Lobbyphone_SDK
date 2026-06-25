# 使用步骤

## V85X快启方案

### 开启功能驱动

V85X快启方案v851s-fastboot板级默认开启USB、SND功能，因此不需要再开启USB、SND功能。如发现当前板级USB、SND功能未开启请参考v851s-fastboot板级配置开启USB、SND功能。

- UVC

```
make kernel_menuconfig
CONFIG_USB_F_UVC
```

- UAC1

```
make kernel_menuconfig
CONFIG_USB_F_UAC1
```

### 开启功能驱动自动拷贝功能

- UVC

```
make menuconfig
CONFIG_PACKAGE_kmod-uvc
```

- UAC1

```
make menuconfig
CONFIG_PACKAGE_kmod-uac1
```

### 开启rt_media-uvc测试demo

rt_media-uvc依赖与rt_media(取视频流)、依赖于MPP(音频3A算法)，因此需要先开启如下选项。其中rt_media为必选项，mpp为可选项。

- rt_media

```
make menuconfig
CONFIG_PACKAGE_rt_media
```

- mpp

```
make menuconfig
CONFIG_mpp_aec
CONFIG_mpp_libwebrtc
CONFIG_mpp_ai_agc
CONFIG_mpp_ans
CONFIG_mpp_libwebrtc
```

- rt_media-uvc

```
make menuconfig
CONFIG_PACKAGE_rt_media-uvc
```
## 使用方法

参数如下，可输入 rt_media-uvc -h 获取。

```
[DBG]parser_cmdline_param, line: 275 -D           --vipp_dev               select vipp dev. eg: -D 0(means /dev/video0)
[DBG]parser_cmdline_param, line: 275 -d           --uvc_dev                select uvc dev. eg: -d 1(means /dev/video1)
[DBG]parser_cmdline_param, line: 275 -B           --bitrate                set MJPEG/H264 stream bitrate. eg: -B 5(means bitrate 5Mbps)
[DBG]parser_cmdline_param, line: 275 -s           --dual_stream            enable MJPEG insert H264 stream function. eg: -s 1(means enable dual stream function)
[DBG]parser_cmdline_param, line: 275 -s_vipp_dev  --dual_stream_vipp_dev   set dual stream use vipp -dev. eg: -s_vipp_dev 4(means dual stream use /dev/video4)
[DBG]parser_cmdline_param, line: 275 -b           --uvc_bulk_mode          enable uvc bulk transport mode. eg: -b 1(means enable uvc bulk transport mode)
[DBG]parser_cmdline_param, line: 275 -a           --enable_aiisp           enable ai isp function. eg: -a 1(means enable ai isp function)
[DBG]parser_cmdline_param, line: 275 -m           --aiisp_mode             set aiisp function mode. eg: -m 0(means set aiisp mode 0)
[DBG]parser_cmdline_param, line: 275 -t           --tdm_rxbuf_cnt          set tdm rxbuf num when enable aiisp function. eg: -t 5(means set 5 tdm rxbufs)
[DBG]parser_cmdline_param, line: 275 -o           --aiisp_auto_switch      auto switch aiisp and normal isp function base on environment luminace. eg: -o 1(means enable)
[DBG]parser_cmdline_param, line: 275 -n           --aiisp_switch_interval  set switch interval between switch aiisp and normal isp. eg: -n 50(means 50 frames)
[DBG]parser_cmdline_param, line: 275 -uac_in      --uac_in                 enable uac1 in function. eg: -uac_in 1(means enable uac1 in function)
[DBG]parser_cmdline_param, line: 275 -uac_out     --uac_out                enable uac1 out function. eg: -uac_out 1(means enable uac1 out function)
[DBG]parser_cmdline_param, line: 275 -uac_sr      --uac_sample_rate        set uac1 audio sample rate. eg: -uac_sr 16000(means set uac1 audio sample rate 16000)
[DBG]parser_cmdline_param, line: 275 -uac_ch      --uac_channel            set uac1 audio channel. eg: -uac_ch 1(means set uac1 audio channel 1)
[DBG]parser_cmdline_param, line: 275 -uac_bw      --uac_bitwidth           set uac1 audio bitwidth. eg: -uac_bitwidth 16(means set uac1 audio bitwidth 16)
[DBG]parser_cmdline_param, line: 275 -uac_aec     --uac_aec                enable audio aec function. eg: -uac_aec 1(means enable audio aec functiuon)
[DBG]parser_cmdline_param, line: 275 -uac_agc     --uac_agc                enable audio agc function. eg: -uac_agc 1(means enable audio agc functiuon)
[DBG]parser_cmdline_param, line: 275 -uac_ans     --uac_ans                enable audio ans function. eg: -uac_ans 1(means enable audio ans functiuon)
```

例如：

- 打开vipp0获取是频率，打开uvc设备/dev/video1，视频码流设置码率为5M,不开启uac1功能

```
rt_media-uvc -D 0 -d 1 -B 5
```

- 打开vipp0获取是频率，打开uvc设备/dev/video1，视频码流设置码率为5M，开启uac1功能支持uac1 out/in，16k采样率单声道位宽16，并开启音频3A算法(需开启MPP依赖选项)

```
rt_media-uvc -D 0 -d 1 -B 5 -uac_in 1 -uac_out 1 -uac_sr 16000 -uac_bw 16 -uac_ch 1 -uac_aec 1 -uac_agc 1 -uac_ans 1
```

注：AEC功能请确保开启了daudio0声卡，请参考v851s-fastboot板级board.dts中的daudio0_plat和daudio0_mach节点是否开启。

## 修改分辨率和帧率

只需修改setusbconfig脚本，以v851s-fastboot为例路径如下

```
target/allwinner/v851s-fastboot/busybox-init-base-files/usr/bin/setusbconfig
```

在enable_uvc接口中修改如下代码

```
# 创建接口 格式 格式代码 高度 宽度 分辨率索引
uvc_create_frame mjpeg m 1920 1080 1
uvc_create_frame mjpeg m 1280 720 2
uvc_create_frame mjpeg m 640 480 3
uvc_create_frame uncompressed u 320 240 1
uvc_create_frame h264 h 1920 1080 1
uvc_create_frame h264 h 1280 720 2
```

其中格式只支持 mjpeg uncompressed(YUYV) h264，对应的格式代码也必须对应上。每中格式的分辨率索引必须从1开始依次往下。

帧率修改uvc_create_frame如下代码：

```
echo 333333 > $dir/dwFrameInterval
echo 333333 > $dir/dwDefaultFrameInterval
```

333333为每帧间隔单位是10us，所以帧率为 100000000 / 333333 = 30fps。15fps = 10000000 / 15 = 666666。

修改后rt_media-uvc demo会解析uvc configfs来获取分辨率信息。

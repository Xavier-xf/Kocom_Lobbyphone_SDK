sample_uvc_vcodec_vo_uac测试流程：
    mpi_uvc和mpi_vdec使用非绑定方式传输数据，mpi_vdec和mpi_venc使用非绑定方式，mpi_vdec和mpi_vo使用非绑定方式。
    从mpi_uvc组件获取mjpeg编码格式图片，交给mpi_vdec组件解码，单路输出。对mpi_vdec输出的视频帧编码为h264，同时交给mpi_vo
显示。app对mpi_uvc和mpi_vdec的输出帧做管理。app获取mpi_uvc的jpeg图片可以保存作为告警事件图片。同时可通过uac进行语音对
讲。host端从uac声卡获取PCM数据，调用mpi_ao本地播放。host端调用mpi_ai本地采集PCM数据，采集过程中做回声消除、降噪、增益处
理，再将PCM数据通过uac声卡发送出去，也就是发送到device端。从而完成语音对讲。

读取测试参数的流程：
	在文件sample_uvc2vcodec_vo_uac.conf填写配置参数。
	启动app时，在命令行参数中给出sample_uvc2vcodec_vo_uac.conf的具体路径，app读取sample_uvc2vcodec_vo_uac.conf，完成参数
解析。
	然后按照参数运行测试。
	从命令行启动sample_uvc2dec_vo的指令：
	./sample_uvc2vcodec_vo_uac -path /mnt/extsd/sample_uvc2vcodec_vo_uac.conf

测试参数的说明：
(1)dev_name: uvc设备字符串
(2)uac_dev_name: uac设备字符串，使用cat /proc/asound/cards可以读取到系统目前存在的声卡信息(包括uac声卡)，获取声卡名，
    如"hw:UAC1Gadget", "hw:Camera"。
(3)pic_format: uvc输出图像格式
(4)capture_videobufcnt: 配置uvc输出图像的buffer数量
(5)capture_width: uvc输出图像宽度
(6)capture_height: uvc输出图像高度
(7)capture_framerate: uvc采集帧率
(8)capture_maxframesize_ratio: 小数。uvc采集的最大视频帧长度和yuvSize的比例。例如0.3表示帧长度是yuvSize的30%。0表示使用默认值。
(9)vdec_extra_frame_num: 配置视频解码库的额外输出帧数量，-1表示使用cedarx.conf的配置值。
(10)display_main_x: 主图显示区域的左上角起点坐标x
(11)display_main_y: 主图显示区域的左上角起点坐标y
(12)display_main_width: 主图显示区域宽度, 0表示不显示。
(13)display_main_height: 主图显示区域高度
(14)venc_type: 视频编码类型, none表示不编码。
(15)venc_file_path: 保存视频编码码流的文件路径
(16)color_space: 视频解码帧的颜色空间
(17)key_frame_interval: 视频编码的关键帧帧间隔
(18)product_mode: 视频编码的产品模式
(19)rc_mode: 视频编码的码控模式
(20)vbr_opt_en：视频编码的VBR码控模式的新旧模式
(21)vbr_opt_rc_priority: VBR新码控的码率偏好
(22)vbr_opt_rc_qualitylevel: VBR新码控的质量等级
(23)video_bitrate: 视频编码的码率
(24)test_frame_count: 测试帧数，0代表无限。
(25)uac_in: host端从uac接收音频数据，在本地播放。
(26)uac_in_chns: host端配置uac的声道数量。
(27)ao_volume: host端设置本地播放音量。
(28)uac_out: host端本地采集音频数据，向uac发送。
(29)sample_rate: host端本地采集音频的采样率，也是host端配置uac的采样率。
(30)ai_volume: host端设置本地音频采集音量。
(31)mic_num: host端本地采集音频的麦克风数量
(32)aec_en: host端本地采集音频开启回声消除
(33)ans_en: host端本地采集音频开启降噪
(34)agc_en: host端本地采集音频开启增益

sample_uvc_vi_codec功能说明：演示猫眼（后锁板）和手机对讲的场景。uvc摄像头必须使用MJPEG编码格式。
后锁板支持uvc摄像头MJPEG输入、MIPI sensor yuv输入的双目场景。对讲时，双目同时工作，可任意切换到一目进行预览和编码为h264
进行wifi传输。支持PCM 16K音频采集并编码为aac。支持视频流和音频流分别作为裸码流保存到本地文件，本地文件支持循环覆盖。视频
流的内容随双目切换而改变。
竖屏竖用。前后摄像头均逆时针旋转90度安装，小图用g2d逆时针旋转90度预览，大图送编码器旋转编码。

v853使用mjpeg解码库，不支持两路输出。配置conf时注意vdec_sub_ratio只能配0。
v821使用mjpegplus解码库，支持两路输出。

uvc->vdec->venc
         ->g2d_rotate->vo
采取非绑定方式。解码不旋转，解码输出大小图。编码器对大图旋转编码。g2d对小图旋转再预览。

vipp0->venc, 采取绑定方式。编码器旋转编码。
vipp4->g2d_rotate->vo, 采取非绑定方式，g2d对小图旋转再预览。

ai->aenc采取绑定方式。

buffer计算:
mjpeg解码: 2大图 + 2小图, 720P + 360P. g2d分配3小图.
vipp0: 3, 720P
vipp4: 3, 360P. g2d分配3小图。

串口命令行启动指令：/mnt/extsd/sample_uvc_vi_codec -path /mnt/extsd/sample_uvc_vi_codec.conf

测试参数的说明：
(1)uvc_dev: uvc设备字符串
(2)uvc_capture_colorspace: 配置uvc的颜色空间
(3)uvc_capture_framerate: uvc采集帧率
(4)uvc_capture_videobufcnt: uvc输出图像的buffer数量
(5)uvc_capture_width: uvc输出图像宽度
(6)uvc_capture_height: uvc输出图像高度
(7)capture_maxframesize_ratio: 小数。uvc采集的最大视频帧长度和yuvSize的比例。例如0.3表示帧长度是yuvSize的30%。0表示使用默认值。
(8)vdec_extra_frame_num: 配置视频解码库的额外输出帧数量，-1表示使用cedarx.conf的配置值。
(9)vdec_pixel_format: 解码器输出图像格式.nv12, nv21
(10)vdec_sub_ratio: 解码器同时输出小图，小图缩小比例解码器支持1/2, 1/4, 1/8。
(11)uvc_display_rotate: 使用g2d对解码器输出小图的旋转角度
(12)uvc_display_x: uvc解码小图的屏幕目标区域的左上角起点坐标x
(13)uvc_display_y: uvc解码小图的屏幕目标区域的左上角起点坐标y
(14)uvc_display_width: uvc解码小图的屏幕目标区域宽度，0表示不显示。
(15)uvc_display_height: uvc解码小图的屏幕目标区域高度，0表示不显示。

(16)isp_color_space: isp采集的颜色空间
(17)isp_capture_framerate: isp采集帧率
(18)vipp_dev: 本地采集的vipp通道号。
(19)vipp_pixel_format: vipp输出像素格式
(20)vipp_buf_num: vipp的buffer数量。
(21)vipp_capture_width: vipp输出宽度
(22)vipp_capture_height: vipp输出高度
(23)sub_vipp_dev: 子vipp通道号
(24)sub_vipp_pixel_format: 子vipp通道输出像素格式
(25)sub_vipp_buf_num: 子vipp通道的buffer数量
(26)sub_vipp_capture_width: 子vipp输出宽度
(27)sub_vipp_capture_height: 子vipp输出高度
(28)sub_vipp_display_rotate: 使用g2d对子vipp输出帧的旋转角度
(29)sub_vipp_display_x: 屏幕目标区域的左上角起点坐标x
(30)sub_vipp_display_y: 屏幕目标区域的左上角起点坐标y
(31)sub_vipp_display_width: 屏幕目标区域宽度, 0表示不显示。
(32)sub_vipp_display_height: 屏幕目标区域高度, 0表示不显示。

(33)preview_source: 1:预览vipp采集帧, 0:预览uvc的jpeg解码帧。
(34)preview_switch_interval: 预览切换间隔时间，单位：秒。0表示不切换预览。切换预览后，编码码流也随之切换。

(35)venc_type: 视频编码类型, none表示不编码。
(36)venc_file_path: 保存视频编码码流的首个文件路径。
(37)venc_file_duration: 视频裸码流文件的时长。单位：秒。到时长后切换文件。
(38)venc_file_num: 循环录制的视频文件的总数。
(39)rtsp_net_type: RTSP网络类型
(40)rtsp_id: rtsp连接号
(41)rc_mode: 视频编码码率模式。0:CBR,1:VBR
(42)vbr_opt_en: 视频编码的VBR码控模式的新旧模式。0: old vbr bitrate control, 1:new vbr bitrate control
(43)vbr_opt_rc_priority: VBR新码控的码率偏好。only for vbr opt, balance bitrate and quality, 0:quality priority, 1:bitrate priority, 2:avg bitrate first, 3:instantaneous bitrate first
(44)vbr_opt_rc_qualitylevel: VBR新码控的画质等级。only for vbr opt, balance bitrate and quality, 0:quality low level, 1:quality middle level, 2:quality high level

(45)uvc_key_frame_interval: uvc摄像头输出图像的编码关键帧间隔
(46)uvc_video_bitrate: uvc摄像头的编码码率。
(47)uvc_encode_rotate: uvc摄像头一路的编码器旋转角度，硬件旋转。

(48)vipp_key_frame_interval: vipp输出图像的编码关键帧间隔
(49)vipp_video_bitrate: vipp编码码率
(50)vipp_encode_rotate: 配置编码器旋转角度，硬件旋转
(51)vipp_isp2ve_link_en: vipp编码是否开启isp2ve联动。
(52)vipp_ve2isp_link_en: vipp编码是否开启ve2isp反向联动。
(53)vipp_region_link_enable: vipp编码ve2isp反向联动，是否启用region_detect_link。
(54)vipp_region_link_tex_detect_enable: vipp编码ve2isp反向联动，region_detect_link是否启用纹理检测
(55)vipp_region_link_motion_detect_enable: vipp编码ve2isp反向联动，region_detect_link是否启用移动检测
(56)vipp_region_link_motion_detect_interval:vipp编码ve2isp反向联动，region_detect_link启用移动检测的检测帧间隔。[0~10], default 0, like 1: means detect every frame

(57)sample_rate: host端本地采集音频的采样率
(58)ai_volume: host端设置本地音频采集音量。
(59)mic_num: host端本地采集音频的麦克风数量
(60)aec_en: host端本地采集音频开启回声消除
(61)ans_en: host端本地采集音频开启降噪
(62)agc_en: host端本地采集音频开启增益

(63)aenc_type: 音频编码格式。
(64)aenc_attachAACHeader: aac编码是否在每帧前面增加aac header。
(65)aenc_file_path: 保存音频编码码流的首个文件路径。
(66)aenc_file_duration: 音频裸码流文件的时长。单位：秒。到时长后切换文件。
(67)aenc_file_num: 循环录制的音频文件的总数。

(68)test_duration: 测试总时长，单位：秒。0表示无限时长。ctrl+c退出程序。


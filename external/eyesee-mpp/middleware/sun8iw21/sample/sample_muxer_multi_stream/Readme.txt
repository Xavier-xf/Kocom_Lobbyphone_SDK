sample_muxer_multi_stream 用来测试多视频码流封装，支持MP4或TS格式。

参数说明：
stream_0_vi_dev = 0					VIPP设备号
stream_0_isp_dev = 0					ISP设备号
stream_0_cap_width	= 1920				录制分辨率宽度
stream_0_cap_height = 1080				录制分辨率高度
stream_0_cap_frmrate = 20				录制帧率
stream_0_cap_format = "nv21"				录制视频格式
stream_0_vi_bufnum	= 5				配置VIPP设备buffer数量
stream_0_enable_WDR = 0					是否启用WDR模式(0：不启用 1：启用)
stream_0_enc_online = 0					是否启用在线编码模式(0：不启用 1：启用)
stream_0_enc_online_share_bufbum = 2			在线编码共享buffer个数
stream_0_enc_type = "H.265"				编码器类型
stream_0_enc_width = 1920				编码视频分辨率宽度
stream_0_enc_height = 1080				编码视频分辨率高度
stream_0_enc_frmrate = 20				编码视频帧率
stream_0_enc_bitrate = 2097152				编码视频码率
stream_0_enc_rcmode = 0					编码码率控制模式
stream_0_encpp_enable = 1				encpp开关
stream_0_enc_ve_ref_frame_lbc_mode = 0 			编码参考帧LBC模式
stream_0_enc_key_frame_interval = 100			编码帧间隔

stream_num = 3						码流数量
test_duration = 20					测试时长
video_file_max_cnt = 3					循环录制文件最大个数
video_file_max_duration = 20				录制文件时长
video_dst_file = "/mnt/extsd/test.mp4"			录制文件保存路径

支持录制格式：
	nv21、yv12、nv12、yu12、aw_fbc、aw_lbc_2_0x、aw_lbc_2_5x、aw_lbc_1_5x、aw_lbc_1_0x
支持编码格式
	H.264、 H.265、MJPEG
支持封装格式
	MP4、TS
	sample根据录制文件保存路径后缀名区分封装格式
编码参考帧LBC模式：
	0:default(1.5x), 1:1.5x, 2:2.0x, 3:2.5x, 4:no lossy

将vi_dev设置为 -1 ，表示不启用这组配置测试。

测试结果确认：
使用potplayer播放器播放录制视频文件 右键-》视频-》选择图像-》选择对应码流进行播放。

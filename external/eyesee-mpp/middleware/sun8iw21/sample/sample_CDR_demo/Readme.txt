sample_CDR_demo 演示cdr场景一路录制（音视频编码）+一路预览+拍照场景。

参数说明：
enc_rec_ref_buf_reduce_enable				编码参考帧/重建帧复用buffer
product_mode								产品模式
enc_rcmode									编码码率控制模式
region_link_enable							编码区域联动使能
region_link_tex_detect_enable
region_link_motion_detect_enable
region_link_motion_detect_inv				编码区域联动计算间隔

capture_sample_rate							音频采样率
capture_bit_witdh							音频位宽
capture_channel_cnt							音频通道个数
capture_ans_en								ans算法使能
capture_agc_en								agc算法使能
capture_aec_en								aec算法使能
aenc_type = "aac"							音频编码格式
aenc_bitrate								音频编码比特率

recorder_vi_dev								VIPP设备号
recorder_vi_virchn							vi虚拟通道号
recorder_isp_dev							ISP设备号
recorder_cap_width							录制输入分辨率宽度
recorder_cap_heigh							录制输入分辨率高度
recorder_cap_frmrate						录制帧率
recorder_cap_format							录制视频格式
recorder_vi_bufnum							配置VIPP设备buffer数量
recorder_enable_WDR							是否启用WDR模式(0：不启用 1：启用)
recorder_enc_chn							编码通道号
recorder_enc_online							是否启用在线编码模式(0：不启用 1：启用)
recorder_enc_online_share_bufbum			在线编码共享buffer个数
recorder_enc_type							编码器类型
recorder_enc_width							编码视频分辨率宽度
recorder_enc_height							编码视频分辨率高度
recorder_enc_frmrate						编码视频帧率
recorder_enc_bitrate						编码视频码率
recorder_enc_refframelbcmode				编码参考帧压缩格式
recorder_rec_duration						录制视频文件时长
recorder_rec_file_cnt						录制视频文件个数
recorder_rec_file_format					录制文件类型
recorder_rec_file							录制视频文件保存路径

preview_vi_dev								VIPP设备号
preview_vi_virchn							vi虚拟通道号
preview_isp_dev								ISP设备号
preview_cap_width							预览输入分辨率宽度
preview_cap_height							预览输入分辨率高度
preview_cap_frmrate							预览帧率
preview_cap_format							预览视频格式
preview_vi_bufnum							配置VIPP设备buffer数量
preview_enable_WDR							是否启用WDR模式(0：不启用 1：启用)
preview_enc_chn								编码通道号
preview_enc_online							是否启用在线编码模式(0：不启用 1：启用)
preview_enc_online_share_bufbum				在线编码共享buffer个数
preview_enc_type							编码器类型
preview_enc_width							编码视频分辨率宽度
preview_enc_height							编码视频分辨率高度
preview_enc_frmrate							编码视频帧率
preview_enc_bitrate							编码视频码率
preview_enc_refframelbcmode					编码参考帧压缩格式
preview_executor							预览终端选择(0：rtsp 1：hwdisplayer)
preview_rtsp_id								rtsp通道号
preview_disp_x								显示区域X坐标
preview_disp_y								显示区域y坐标
preview_disp_width							显示区域宽度
preview_disp_height							显示区域高度
preview_disp_dev							显示设备类型

takepic_enable								使能拍照
takepic_vi_dev								拍照VIPP设备号
takepic_vi_virchn							拍照vi虚拟通道号
takepic_enc_chn								拍照编码通道号
takepic_file								拍照保存文件路径
test_duration								测试时长

支持录制格式：
	nv21、yv12、nv12、yu12、aw_fbc、aw_lbc_2_0x、aw_lbc_2_5x、aw_lbc_1_5x、aw_lbc_1_0x
支持封装格式
	MP4、TS

将VIPP设备号设置为 -1 ，表示不启用这组配置测试。

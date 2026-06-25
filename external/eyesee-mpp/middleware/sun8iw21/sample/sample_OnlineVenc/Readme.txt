sample_OnlineVenc测试流程：
	该sample演示IPC在线编码使用场景：主码流在线编码、子码流离线编码。
    针对主码流和子码流分别创建mpi_vi和mpi_venc，将它们绑定，再分别启动。mpi_vi采集图像，直接传输给mpi_venc进行编码。

读取测试参数的流程：
    sample提供了配置文件sample_OnlineVenc.conf，测试参数都写在该文件中。

从命令行启动sample_OnlineVenc的指令：
    ./sample_OnlineVenc -path /mnt/extsd/sample_OnlineVenc.conf

测试参数的说明：
main_vipp: 主码流VIPP号
main_viChn：主码流VIPP虚拟通道号
main_src_width: 主VIPP输出图像宽度
main_src_height: 主VIPP输出图像高度
main_pixel_format: 主VIPP输出图像格式。nv21, lbc25等。
main_online_en：主码流开启/关闭在线编码
main_online_share_buf_num：主码流在线编码时，配置单/双buffer。
main_wdr_enable：主码流WDR使能开关。
main_vi_buf_num：配置主码流VI BUFFER个数。

sub_vipp: 子码流VIPP号
sub_viChn：子码流VIPP虚拟通道号
sub_src_width: 子VIPP输出图像宽度
sub_src_height: 子VIPP输出图像高度
sub_pixel_format: 子VIPP输出图像格式。nv21, lbc25等。
sub_wdr_enable：子VIPP WDR使能开关。
sub_vi_buf_num：子VIPP配置VI BUFFER个数。

src_frame_rate: 采集帧率

main_venc_chn：主码流编码通道号
main_encode_type: 主码流编码类型, H.265, H.264等。
main_encode_width: 主码流的编码目标宽度
main_encode_height: 主码流的编码目标高度
main_encode_frame_rate: 主码流的编码目标帧率
main_encode_bitrate: 主码流的编码目标码率, bit/s
main_file_path: 保存主码流的文件路径，空表示不保存主码流。

sub_venc_chn：子码流编码通道号
sub_encode_type: 子码流编码类型, H.265, H.264等。
sub_encode_width: 子码流的编码目标宽度
sub_encode_height: 子码流的编码目标高度
sub_encode_frame_rate: 子码流的编码目标帧率
sub_encode_bitrate: 子码流的编码目标码率, bit/s
sub_file_path: 保存子码流的文件路径，空表示不保存子码流。

test_duration: 测试时间，0表示无限。单位：秒。


验证双目TDM内存优化功能，需要按如下配置：
注意关闭在线编码（main_online_en = 0），ISP和VIPP的配置顺序，帧率，以及main_vi_sync_ctrl_enable的开关配置。

main_isp = 1
main_vipp = 1
main_src_width = 1920
main_src_height = 1080
main_pixel_format = "aw_lbc_2_5x"      #nv21,nv12,yu12,yv12;aw_lbc_2_5x,aw_lbc_2_0x,aw_lbc_1_5x,aw_lbc_1_0x
main_wdr_enable = 0
main_vi_buf_num = 3
main_src_frame_rate = 15               #fps
main_viChn = 0                         #-1:disale main stream
main_venc_chn = 0                      #-1:disale main stream
main_encode_type = "H.265"
main_encode_width = 1920               #1280x720->720p, 1920x1080->1080p, 2304x1296->3M, 2560×1440->2k, 3840x2160->4k, 7680x4320->8k
main_encode_height = 1080
main_encode_frame_rate = 15            #fps
main_encode_bitrate = 1048576          #5M:5242880, 2M:2097152, 1.5M:1572864, 1M:1048576
main_file_path = "/mnt/extsd/mainStream.raw"    #if no path is specified, it will not be saved.
main_online_en = 0
main_online_share_buf_num = 2
main_encpp_enable = 1
main_vi_sync_ctrl_enable = 1

sub_isp = 0
sub_vipp = 0
sub_src_width = 1920
sub_src_height = 1080
sub_pixel_format = "aw_lbc_2_5x"       #nv21,nv12,yu12,yv12;aw_lbc_2_5x,aw_lbc_2_0x,aw_lbc_1_5x,aw_lbc_1_0x
sub_wdr_enable = 0
sub_vi_buf_num = 3
sub_src_frame_rate = 15                #fps

sub_vipp_crop_en = 0
sub_vipp_crop_rect_x = 0
sub_vipp_crop_rect_y = 0
sub_vipp_crop_rect_w = 1888
sub_vipp_crop_rect_h = 1072

sub_viChn = 0                          #-1:disale sub stream
sub_venc_chn = 1                       #-1:disale sub stream
sub_encode_type = "H.265"
sub_encode_width = 1920                 #1280x720->720p, 1920x1080->1080p, 2560×1440->2k, 3840x2160->4k, 7680x4320->8k
sub_encode_height = 1080
sub_encode_frame_rate = 15             #fps
sub_encode_bitrate = 1048576            #5M:5242880, 2M:2097152, 1M:1048576, 0.5M:512000
sub_file_path = "/mnt/extsd/subStream.raw"      #if no path is specified, it will not be saved.
sub_encpp_enable = 1

sub_lapse_viChn = -1                    #-1:disale sub lapse stream
sub_lapse_venc_chn = -1                 #-1:disale sub lapse stream
sub_lapse_encode_type = "H.264"
sub_lapse_encode_width = 640           #1280x720->720p, 1920x1080->1080p, 2560×1440->2k, 3840x2160->4k, 7680x4320->8k
sub_lapse_encode_height = 360
sub_lapse_encode_frame_rate = 20       #fps
sub_lapse_encode_bitrate = 256000      #5M:5242880, 2M:2097152, 1M:1048576, 0.5M:512000
sub_lapse_file_path = "/mnt/extsd/subLapseStream.raw"  #if no path is specified, it will not be saved.
sub_lapse_time = 1000000               #unit:us
sub_lapse_encpp_enable = 1

isp_ve_linkage_enable = 1
isp_ve_linkage_stream_channel = 0      #0:main stream, 1:sub stream, 2:sub lapse stream

wb_yuv_enable = 0
wb_yuv_buf_num = 1
wb_yuv_start_index = 0
wb_yuv_total_cnt = 10
wb_yuv_stream_channel = 0              #0:main stream, 1:sub stream, 2:sub lapse stream
wb_yuv_file_path = "/mnt/extsd/wb_yuv.yuv"

test_duration = 60                     #unit:s

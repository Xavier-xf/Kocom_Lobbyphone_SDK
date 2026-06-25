sample_VideoResizer测试流程：
    解码视频文件（如：xxx.mp4）再重编码为h264/h265，保存为裸码流文件。模拟客户使用场景，mpi_demux和mpi_vdec之间用非绑定
    方式，mpi_vdec和mpi_venc用绑定方式。
    Ctrl-C可中止运行。

读取测试参数的流程：
    sample提供了sample_VideoResizer.conf，测试参数都写在该文件中。
    启动sample_VideoResizer时，在命令行参数中给出sample_VideoResizer.conf的具体路径，sample_VideoResizer会读取
    sample_VideoResizer.conf，完成参数解析。然后按照参数运行测试。
    从命令行启动sample_VideoResizer的指令：
    ./sample_VideoResizer -path /mnt/extsd/sample_VideoResizer.conf
    "-path /mnt/extsd/sample_VideoResizer.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)src_file：原始视频文件的路径
(2)encode_type: 编码类型
(3)dst_width: 编码目标宽度
(4)dst_height: 编码目标高度
(5)rotation: 编码旋转角度
(6)bitrate: 编码码率
(7)key_frame_interval: 关键帧间隔, 0表示使用默认值，默认值为帧率。

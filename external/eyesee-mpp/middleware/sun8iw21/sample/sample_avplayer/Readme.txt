sample_avplayer演示音视频播放和AV同步。演示设定解码输出像素格式、解码缩小输出、解码旋转输出、解码库解码帧数控制、图层编
号、屏幕显示区域、视频显示组件显示帧率、屏蔽解析音频流、音量大小、指定视频流、指定音频流、初始跳播位置、变速播放、循环播
放。
Ctrl-C可中止运行。

sample提供了sample_avplayer.conf，测试参数都写在该文件中。从串口终端启动sample_avplayer的指令：
./sample_avplayer -path /mnt/extsd/sample_avplayer.conf

测试参数的说明：
(1)src_file：指定原始视频文件的路径
(2)seek_position：指定原始视频文件的开始解析位置(ms)
(3)test_duration: sample的测试时间（单位：s）。如果loop循环生效，就以loop为准。
(4)display rect:目标显示区域
(5)loop: 循环播放，0表示不循环。-1表示无限循环。

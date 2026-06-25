sample_WebRtcAec测试流程：
本sample用来测试WebRtcAec的回声消除效果。输入文件tmp_in_ai_pcm是采集音频，输入文件tmp_ref_ai_pcm是回采音频，将它们送入
WebRtcAec库处理后得到输出文件tmp_out_ai_pcm。tmp_out_ai_pcm和电脑仿真的结果文件比较，判定WebRtcAec库是否正常运行。

读取测试参数的流程：
	sample提供了sample_WebRtcAec.conf，包含测试参数。
	启动sample_WebRtcAec时，在命令行参数中给出sample_WebRtcAec.conf的具体路径，sample_WebRtcAec会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_WebRtcAec的指令：
	./sample_WebRtcAec -path /mnt/extsd/sample_WebRtcAec.conf
	"-path /mnt/extsd/sample_WebRtcAec.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pcm_in_path：指定音频采集的pcm文件的路径，该文件在音频采集时开启debug后收集采集数据保存。
(2)pcm_ref_path：指定音频回采的pcm文件的路径，该文件是音频采集时开启debug后收集回采数据保存。
(3)pcm_out_path：指定将采集数据和回采数据经WebRtcAec库回声消除处理后的pcm数据的保存路径。
(4)pcm_sample_rate：指定pcm文件的采样率。
(4)pcm_channel_cnt：指定pcm文件的声道数。
(5)pcm_bit_width：指定位宽，启用aec后，bit_width须为16。


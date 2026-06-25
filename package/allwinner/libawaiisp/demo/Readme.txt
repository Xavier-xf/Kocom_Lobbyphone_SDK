AW AI-ISP Demo使用说明

执行命令：
./awaiisp_demo -path awaiisp_demo.conf

配置参数说明：
npu gamma文件：demo_aiisp_lut_nbg_file_path = "/mnt/extsd/gamma_1080.nb"
npu 模型文件 ：demo_aiisp_nbg_file_path = "/mnt/extsd/FW100A12W0S00.nb"
输入RAW图的宽度：demo_aiisp_width = 1920
输入RAW图的高度：demo_aiisp_height = 1080
AI-ISP模拟缓存buffer个数：demo_aiisp_tdm_rxbuf_num = 5
AI-ISP的模式（0：AWAIISP_MODE_NPU，1：AWAIISP_MODE_NORMAL，2：AWAIISP_MODE_NORMAL_GAMMA）：demo_aiisp_mode = 0
测试预留参数：demo_aiisp_reserve0 = 0
AI-ISP模拟帧率：demo_aiisp_frame_rate = 10
AI-ISP模拟输入源文件：demo_aiisp_input_file = "/mnt/extsd/input.raw"
AI-ISP模拟输出文件：demo_aiisp_output_file = "/mnt/extsd/output.raw"

demo会读取源文件，经过AI-ISP模型处理后，输出处理结果。

源文件：
demo_aiisp_input_file  ：AI-ISP处理前存储的RAW文件。

测试输出：
demo_aiisp_output_file ：输出经过AI-ISP处理后的bin文件。

libawaiisp库

适用范围：V853/V853s/V851s/V851se/V851s3
依赖关系：依赖G2D、NPU、ISP驱动，依赖闭源库libVIPlite、libVIPuser

只使用全志ai-isp：libawaiisp_full库（或者libawaiisp库+libVIP库）
只使用全志npu人形：libawnn_full库（或者libawnn库+libVIP库）
使用全志npu人形和全志ai-isp：libawaiisp库+libawnn_full库（推荐）
使用全志ai-isp和第三方npu算法：libawaiisp库+libawnn库+libVIP库（推荐）

备注：
libawaiisp_full库：包含libawaiisp库和libVIP库（libVIPlite、libVIPuser）
libawnn_full库：全志npu人形库，包含libawnn和libVIP库（libVIPlite、libVIPuser）


1. 库和头文件
sdk
├── include
│   └── awaiisp.h
└── library
    ├── glibc
    │   ├── libawaiisp.a
    │   └── libawaiisp.so
    └── musl
        ├── libawaiisp.a
        └── libawaiisp.so

2. 模型文件
models
├── FW100A10W0S02.nb
├── FW100A11W0S01.nb
├── FW100A12W0S00.nb
├── gamma_1080.nb
├── gamma_1088.nb
└── gamma_720.nb

FWxx.nb为AI-ISP模型，gamma_xx为AI-ISP Gamma模型

2.1 AI-ISP模型命名规则
以FW100A10W0S00为例，

FW	大版本	大的模型/网络结构优化，FW1xx，FW2xx，FW3xx
A/B	小版本	差异化的算力/效果定制（初步分A1x，A2x，A3x代表三个算力档位），大的类型差异用A/B区分
W	区分WDR、HFLIP、VFLIP开关情况，Wx
	0（000b）：不开WDR、不开SENSOR HFLIP、不开SENSOR VFLIP
	1（001b）：不开WDR、不开SENSOR HFLIP、开SENSOR VFLIP
	2（010b）：不开WDR、开SENSOR HFLIP、不开SENSOR VFLIP
	3（011b）：不开WDR、开SENSOR HFLIP、开SENSOR VFLIP
	4（100b）：开WDR、不开SENSOR HFLIP、不开SENSOR VFLIP
	5（101b）：开WDR、不开SENSOR HFLIP、开SENSOR VFLIP
	6（110b）：开WDR、开SENSOR HFLIP、不开SENSOR VFLIP
	7（111b）：开WDR、开SENSOR HFLIP、开SENSOR VFLIP
S	SENSOR	传感器编号	对应的SENSOR传感器型号，Sxx
	00：GC2053
	01：SC200AI
	02：GC1084

2.2 AI-ISP Gamma模型命名规则
gamma_1080.nb：适用于sensor分辨率1920*1080
gamma_1088.nb：适用于sensor分辨率1920*1088
gamma_720.nb ：适用于sensor分辨率1280*720

3. Demo测试

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

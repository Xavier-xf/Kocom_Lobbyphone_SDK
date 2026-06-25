sample_odet_demo
	1    演示智能IPC的场景，使用yolov3_tiny模型检测物体，在检测到的物体上加框显示。
         检测物体：
	     "person","bicycle","car","motorbike","aeroplane","bus","train","truck","boat","traffic light",
	     "fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow",
	     "elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee",
	     "skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard",
	     "tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich",
	     "orange","broccoli","carrot","hot dog","pizza","donut","cake","chair","sofa","pottedplant","bed",
	     "diningtable","toilet","tvmonitor","laptop","mouse","remote","keyboard","cell phone","microwave",
	     "oven","toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"。
	     
	 2   支持加框预览和加框的视频编码存储。
	    		
读取测试参数的流程：
    sample提供了 sample_odet_demo.conf，测试参数都写在该文件中。
启动 sample_odet_demo 时，在命令行参数中给出 sample_odet_demo.conf 的具体路径，sample_odet_demo 会读取 sample_odet_demo.conf，完成参数解析。
然后按照参数运行测试。

从命令行启动 sample_odet_demo 的指令：
    ./sample_odet_demo -path sample_odet_demo.conf
    "-path sample_odet_demo.conf"指定了测试参数配置文件的路径。


配置方法：

venc_vaild = 1   //1-编码存储，0-关闭编码存储，默认编码存储，
venc_vipp_dev = 0
venc_capture_width = 1920
venc_capture_height = 1080
venc_display_width = 1920
venc_display_height = 1080
venc_encode_type = "H.265"  //支持"H.265"和"H.264"
venc_file_path = "/mnt/sdcard/venc_stream.raw" //编码存储路径

preview_vipp_dev = 4
preview_capture_width = 1280
preview_capture_height = 720
preview_layer_num = 4
preview_display_x = 0
preview_display_y = 0
preview_display_width = 1280
preview_display_height = 720

npu_vipp_dev = 8    
npu_nbg_file_path = "/mnt/sdcard/network_binary.nb"  //nb路径

test_duration = 60 #unit:s, 0:Infinite duration. //如果打开编码存储，注意sd卡存储空间   


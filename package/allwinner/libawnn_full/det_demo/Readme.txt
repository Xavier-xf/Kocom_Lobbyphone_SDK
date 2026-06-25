AWNN Demo使用说明

执行命令：
./awnn_det_demo testcase.txt

demo会根据每个模型单独一个线程运行检测，根据输入图像输出检测结果，并根据检测结果保存画框后图像。

测试输出：
post_data/result_TID.txt ：输出每个模型检测结果，其中 TID 为检测线程ID号。
post_data/performance_TID.txt ：输出每个模型检测性能结果，其中 TID 为检测线程ID号；性能评测时建议运行单个模型且将 stdout 重定向为 /dev/null 。
post_inputN ：每个输入的画框结果，其中 inputN 为输入图像路径，直接在路径前添加 post_前缀进行保存，因此输入图像路径请勿添加 ./ 或使用绝对路径。
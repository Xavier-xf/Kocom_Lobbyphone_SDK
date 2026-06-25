libawaiisp_full库

适用范围：V853/V853s/V851s/V851se/V851s3/V837s
依赖关系：依赖G2D、NPU、已包含闭源库libVIPlite、libVIPuser

备注：
libawaisr_full库：包含libawaisr库和libVIP库（libVIPlite、libVIPuser）

1. 库和头文件
sdk
├── include
│   └── awaisr.h
└── library
    ├── glibc
    │   ├── libawaisr_full.a
    │   └── libawaisr_full.so
    └── musl
        ├── libawaisr_full.a
        └── libawaisr_full.so

base
├── include
    └── aisr_version.h 

2. 模型文件
models
├── srx2.nb
└── 

x2_1280x720_2560x1440_c.nb为 1280x720到超分2560x1440 模型
x2_384x288_768x576_c.nb为 384x288超分到768x576 模型
x3_384x288_1152x864_c.nb为 384x288到超分1152x864 模型
x2_4000x3000_8000x6000_g.nb为 4000x3000到超分8000x6000 模型


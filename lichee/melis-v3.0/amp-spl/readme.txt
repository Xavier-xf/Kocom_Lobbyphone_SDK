amp-spl编译步骤

1. 进入amp-spl编译目录
cd lichee/melis-v3.0/amp-spl

2. 配置
make board/sun8iw21p1/sun8iw21p1_axp2101_defconfig

3. 编译
make

4. 取消 lichee/brandy-2.0/spl/board/sun8iw21p1/spinorfastboot.mk 中的 CFG_AMP_BOOT0_BIN_PATH 注释，
（如果是emmc方案，修改mmcfastboot.mk文件）

CFG_AMP_BOOT0_BIN_PATH="../../../melis-v3.0/amp-spl/amp-boot0.bin"

5. 重新编译boot、打包，将amp-spl打包进boot0
mboot0
p


注意事项：
第3步编译时，请先执行如下命令，再执行make编译
如出现找不到 conf.h 报错, 执行: make include/generated/conf.h
如出现找不到 autoconf.h 报错, 执行: make include/generated/autoconf.h


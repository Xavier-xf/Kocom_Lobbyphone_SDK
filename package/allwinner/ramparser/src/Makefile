.PHONY: all clean

# 默认目标，如果未提供输入参数，则使用glibc
all: musl

# 如果输入参数是musl，则创建out目录并拷贝ramparser
musl:
	mkdir -p out
	cp musl/ramparser out/

# 如果输入参数是glibc，则创建out目录并拷贝ramparser
glibc:
	echo "glibc cc"
	mkdir -p out
	cp glibc/ramparser out/

# 清理生成的文件和目录
clean:
	rm -rf out

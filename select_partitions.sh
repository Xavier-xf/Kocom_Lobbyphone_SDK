#!/bin/bash

CONFIG_FILE="$1"
OUTPUT_FILE="flash_cmds.txt"

if [ -z "$CONFIG_FILE" ]; then
    echo "用法: $0 <配置文件>"
    exit 1
fi

if [ ! -f "$CONFIG_FILE" ]; then
    echo "错误: 配置文件 '$CONFIG_FILE' 不存在"
    exit 1
fi

# 提取分区信息到临时文件
TMP_FILE=$(mktemp)
awk '
/^\[partitions\/[^]]+\]/ {
    part = substr($0, 13, length($0)-14)
    in_part = 1
    next
}
/^[[:space:]]*size[[:space:]]*=/ {
    if (in_part) {
        size_str = $0
        gsub(/[[:space:]]*size[[:space:]]*=[[:space:]]*/, "", size_str)
        gsub(/[[:space:]]*$/, "", size_str)
        size_val = size_str + 0
        printf "%s %d\n", part, size_val
        in_part = 0
    }
}
' "$CONFIG_FILE" > "$TMP_FILE"

# 读入数组
partitions=()
sizes=()
while read -r p s; do
    partitions+=("$p")
    sizes+=("$s")
done < "$TMP_FILE"
rm -f "$TMP_FILE"

if [ ${#partitions[@]} -eq 0 ]; then
    echo "未找到任何分区，请检查配置文件。"
    exit 1
fi

# 清空输出文件
> "$OUTPUT_FILE"

# 遍历数组并询问
for idx in "${!partitions[@]}"; do
    part="${partitions[$idx]}"
    size_sectors="${sizes[$idx]}"
    if [ "$size_sectors" -eq 0 ]; then
        size_bytes=0
    else
        size_bytes=$((size_sectors * 512))
    fi

    printf "是否打包分区 '%s' (大小 %d 扇区 = %d 字节) ? (y/n): " "$part" "$size_sectors" "$size_bytes"
    read -r answer
    case "$answer" in
        [yY]|[yY][eE][sS])
            echo "sunxi_flash write $part $size_bytes" >> "$OUTPUT_FILE"
            echo "  已添加"
            ;;
        *)
            echo "  已跳过"
            ;;
    esac
done

echo "================================="
if [ -s "$OUTPUT_FILE" ]; then
    echo "已生成: $OUTPUT_FILE"
    cat "$OUTPUT_FILE"
else
    echo "未选择任何分区，输出文件为空。"
fi
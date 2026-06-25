$(call inherit-product-if-exists, target/allwinner/v851s3-common/v851s3-common.mk)

PRODUCT_PACKAGES +=

PRODUCT_COPY_FILES +=

PRODUCT_AAPT_CONFIG := large xlarge hdpi xhdpi
PRODUCT_AAPT_PERF_CONFIG := xhdpi
PRODUCT_CHARACTERISTICS := musicbox

PRODUCT_BRAND := allwinner
PRODUCT_NAME := v851s3_perf1
PRODUCT_DEVICE := v851s3-perf1
PRODUCT_MODEL := Allwinner v851s3 perf1 board

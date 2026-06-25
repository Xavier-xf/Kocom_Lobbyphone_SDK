$(call inherit-product-if-exists, target/allwinner/v837s-common/v837s-common.mk)

PRODUCT_PACKAGES +=

PRODUCT_COPY_FILES +=

PRODUCT_AAPT_CONFIG := large xlarge hdpi xhdpi
PRODUCT_AAPT_PERF_CONFIG := xhdpi
PRODUCT_CHARACTERISTICS := musicbox

PRODUCT_BRAND := allwinner
PRODUCT_NAME := v837s_perf1
PRODUCT_DEVICE := v837s-perf1
PRODUCT_MODEL := Allwinner v837s perf1 board

$(call inherit-product-if-exists, target/allwinner/v553-common/v553-common.mk)

PRODUCT_PACKAGES +=

PRODUCT_COPY_FILES +=

PRODUCT_AAPT_CONFIG := large xlarge hdpi xhdpi
PRODUCT_AAPT_PERF_CONFIG := xhdpi
PRODUCT_CHARACTERISTICS := musicbox

PRODUCT_BRAND := allwinner
PRODUCT_NAME := v553_qa
PRODUCT_DEVICE := v553-qa
PRODUCT_MODEL := Allwinner v553 qa board

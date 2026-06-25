$(call inherit-product-if-exists, target/allwinner/v851s-common/v851s-common.mk)

PRODUCT_PACKAGES +=

PRODUCT_COPY_FILES +=

PRODUCT_AAPT_CONFIG := large xlarge hdpi xhdpi
PRODUCT_AAPT_PERF_CONFIG := xhdpi
PRODUCT_CHARACTERISTICS := musicbox

PRODUCT_BRAND := allwinner
PRODUCT_NAME := v851s_ipc_dual_camera
PRODUCT_DEVICE := v851s-ipc_dual_camera
PRODUCT_MODEL := Allwinner v851s ipc_dual_camera board

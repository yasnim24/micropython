include sdk/common/sdk.mk

# oberon
INC += -I$(NRF_SDK_DIR)/external/nrf_oberon
INC += -I$(NRF_SDK_DIR)/external/nrf_oberon/include
LIBS += $(NRF_SDK_DIR)/external/nrf_oberon/lib/cortex-m4/hard-float/liboberon_3.0.8.a

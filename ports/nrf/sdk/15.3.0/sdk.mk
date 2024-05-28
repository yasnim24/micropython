include sdk/common/sdk.mk

# oberon
INC += -I$(NRF_SDK_DIR)/external/nrf_oberon
INC += -I$(NRF_SDK_DIR)/external/nrf_oberon/include
LIBS += $(NRF_SDK_DIR)/external/nrf_oberon/lib/cortex-m4/hard-float/liboberon_2.0.7.a

# TODO: Clean up.

NRF_SDK_DIR = sdk/$(NRF_SDK_VERSION)

SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/ble/common/,\
	ble_advdata.c \
	ble_conn_params.c  \
	ble_conn_state.c \
	ble_srv_common.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/ble/peer_manager/,\
    auth_status_tracker.c \
    gatt_cache_manager.c \
    gatts_cache_manager.c \
    id_manager.c \
    nrf_ble_lesc.c \
    peer_database.c \
    peer_data_storage.c \
    peer_id.c \
    peer_manager.c \
    peer_manager_handler.c \
    pm_buffer.c \
    security_dispatcher.c \
    security_manager.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/atomic_fifo/,\
    nrf_atfifo.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/atomic_flags/,\
    nrf_atflags.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/atomic/,\
    nrf_atomic.c \
	)
	
# Needed for log.	
#SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/balloc/,\
    nrf_balloc.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/crypto/,\
    nrf_crypto_ecc.c \
    nrf_crypto_ecdh.c \
    nrf_crypto_error.c \
    nrf_crypto_hash.c \
    nrf_crypto_init.c \
    nrf_crypto_rng.c \
    nrf_crypto_shared.c \
    )

# rng    
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/crypto/backend/nrf_hw/,\
    nrf_hw_backend_init.c \
    nrf_hw_backend_rng.c \
    nrf_hw_backend_rng_mbedtls.c \
    )
    
# oberon
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/crypto/backend/oberon/,\
    oberon_backend_chacha_poly_aead.c \
    oberon_backend_ecc.c \
    oberon_backend_ecdh.c \
    oberon_backend_ecdsa.c \
    oberon_backend_eddsa.c \
    oberon_backend_hash.c \
    oberon_backend_hmac.c \
    )
    
 # cc310
 #SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/crypto/,\
    backend/cc310/cc310_backend_aes_aead.c \
    backend/cc310/cc310_backend_aes.c \
    backend/cc310/cc310_backend_chacha_poly_aead.c \
    backend/cc310/cc310_backend_ecc.c \
    backend/cc310/cc310_backend_ecdh.c \
    backend/cc310/cc310_backend_ecdsa.c \
    backend/cc310/cc310_backend_eddsa.c \
    backend/cc310/cc310_backend_hash.c \
    backend/cc310/cc310_backend_hmac.c \
    backend/cc310/cc310_backend_init.c \
    backend/cc310/cc310_backend_mutex.c \
    backend/cc310/cc310_backend_rng.c \
    backend/cc310/cc310_backend_shared.c \
    backend/nrf_sw/nrf_sw_backend_hash.c \
	)
#    nrf_crypto_svc.c \
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/experimental_section_vars/,\
    nrf_section_iter.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/fds/,\
    fds.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/fstorage/,\
	nrf_fstorage.c \
	nrf_fstorage_sd.c \
	)

#SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/svc/,\
#    nrf_svc_handler.c \
#	)

# Log.	
#SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/log/src/,\
	nrf_log_backend_serial.c \
    nrf_log_default_backends.c \
    nrf_log_frontend.c \
    nrf_log_str_formatter.c \
	)
	
# Needed for log.	
#SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/memobj/,\
    nrf_memobj.c \
	)
	
# Needed for rng.	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/queue/,\
    nrf_queue.c \
	)

# Needed for log.	
#SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/ringbuf/,\
    nrf_ringbuf.c \
	)

# Needed for log.	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/strerror/,\
    nrf_strerror.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/libraries/util/,\
    app_error.c \
    app_error_handler_gcc.c \
    app_util_platform.c \
    nrf_assert.c \
    sdk_mapped_flags.c \
	)
	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/components/softdevice/common/,\
    nrf_sdh_ant.c \
    nrf_sdh_ble.c \
    nrf_sdh.c \
    nrf_sdh_soc.c \
	)
	
# Needed for log.	
#SRC_SDK += $(addprefix $(NRF_SDK_DIR)/external/fprintf/,\
    nrf_fprintf.c \
    nrf_fprintf_format.c \
	)
	
# rng
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/integration/nrfx/legacy/,\
	nrf_drv_rng.c \
	)
	
# Log.	
SRC_SDK += $(addprefix $(NRF_SDK_DIR)/micropython/log/src/,\
    nrf_log_frontend.c \
    nrf_log_str_formatter.c \
	)
	
INC += -I$(NRF_SDK_DIR)
INC += -I$(NRF_SDK_DIR)/components/ble/common
INC += -I$(NRF_SDK_DIR)/components/ble/peer_manager
INC += -I$(NRF_SDK_DIR)/components/libraries/atomic_fifo
INC += -I$(NRF_SDK_DIR)/components/libraries/atomic_flags
INC += -I$(NRF_SDK_DIR)/components/libraries/atomic
# Needed for log.	
#INC += -I$(NRF_SDK_DIR)/components/libraries/balloc
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto
# Includes of all backends have to be available even only certain ones are used. 
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/cc310
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/cc310_bl
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/cifra
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/mbedtls
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/micro_ecc
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/nrf_hw
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/nrf_sw
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/oberon
INC += -I$(NRF_SDK_DIR)/components/libraries/crypto/backend/optiga

INC += -I$(NRF_SDK_DIR)/components/libraries/experimental_section_vars
INC += -I$(NRF_SDK_DIR)/components/libraries/fstorage
INC += -I$(NRF_SDK_DIR)/components/libraries/fds

#INC += -I$(NRF_SDK_DIR)/components/libraries/log
#INC += -I$(NRF_SDK_DIR)/components/libraries/log/src
# Needed for log.	
#INC += -I$(NRF_SDK_DIR)/components/libraries/memobj
INC += -I$(NRF_SDK_DIR)/components/libraries/mutex
# Needed for rng.	
INC += -I$(NRF_SDK_DIR)/components/libraries/queue
# Needed for log.	
#INC += -I$(NRF_SDK_DIR)/components/libraries/ringbuf

# '$(NRF_SDK_DIR)/micropython/stack_info' will be used instead
#INC += -I$(NRF_SDK_DIR)/components/libraries/stack_info

# Needed for log.	
INC += -I$(NRF_SDK_DIR)/components/libraries/strerror
INC += -I$(NRF_SDK_DIR)/components/libraries/svc
INC += -I$(NRF_SDK_DIR)/components/libraries/util
INC += -I$(NRF_SDK_DIR)/components/softdevice/common

# oberon
INC += -I$(NRF_SDK_DIR)/external/nrf_oberon
INC += -I$(NRF_SDK_DIR)/external/nrf_oberon/include
LIBS += $(NRF_SDK_DIR)/external/nrf_oberon/lib/cortex-m4/hard-float/liboberon_2.0.7.a

# cc310
#INC += -I$(NRF_SDK_DIR)/external/nrf_cc310/include
#LIBS += $(NRF_SDK_DIR)/external/nrf_cc310/lib/cortex-m4/hard-float/libnrf_cc310_0.9.12.a

# rng
INC += -I$(NRF_SDK_DIR)/integration/nrfx/legacy

INC += -I$(NRF_SDK_DIR)/micropython/crypto

# Log.
# '$(NRF_SDK_DIR)/micropython/log' replaces '$(NRF_SDK_DIR)/components/libraries/log'.
INC += -I$(NRF_SDK_DIR)/micropython/log
INC += -I$(NRF_SDK_DIR)/micropython/log/src

# '$(NRF_SDK_DIR)/micropython/stack_info' replaces '$(NRF_SDK_DIR)/components/libraries/stack_info'.
INC += -I$(NRF_SDK_DIR)/micropython/stack_info

# Needed for log.	
#INC += -I$(NRF_SDK_DIR)/external/fprintf

# TODO: Experimental - moved to common.ld 
#LD_FILES += $(NRF_SDK_DIR)/generic_gcc_nrf52.ld

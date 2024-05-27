/**
 * Copyright (c) 2016 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

//
// 24-05-11
// Adapted for MicroPython for direct logging without 'log back-ends'.
//

#ifndef NRF_LOG_CTRL_INTERNAL_H
#define NRF_LOG_CTRL_INTERNAL_H
/**
 * @cond (NODOX)
 * @defgroup nrf_log_ctrl_internal Auxiliary internal types declarations
 * @{
 * @internal
 */

#include "sdk_common.h"
#if NRF_MODULE_ENABLED(NRF_LOG)

#define NRF_LOG_LFCLK_FREQ 32768

#ifdef APP_TIMER_CONFIG_RTC_FREQUENCY
#define LOG_TIMESTAMP_DEFAULT_FREQUENCY ((NRF_LOG_TIMESTAMP_DEFAULT_FREQUENCY == 0) ?              \
                                       (NRF_LOG_LFCLK_FREQ/(APP_TIMER_CONFIG_RTC_FREQUENCY + 1)) : \
                                        NRF_LOG_TIMESTAMP_DEFAULT_FREQUENCY)
#else
#define LOG_TIMESTAMP_DEFAULT_FREQUENCY NRF_LOG_TIMESTAMP_DEFAULT_FREQUENCY
#endif

#define NRF_LOG_INTERNAL_INIT(...)               \
        nrf_log_init(GET_VA_ARG_1(__VA_ARGS__),  \
                     GET_VA_ARG_1(GET_ARGS_AFTER_1(__VA_ARGS__, LOG_TIMESTAMP_DEFAULT_FREQUENCY)))

#else // NRF_MODULE_ENABLED(NRF_LOG)
#define NRF_LOG_INTERNAL_INIT(...) NRF_SUCCESS
#endif // NRF_MODULE_ENABLED(NRF_LOG)

/** @}
 * @endcond
 */
#endif // NRF_LOG_CTRL_INTERNAL_H

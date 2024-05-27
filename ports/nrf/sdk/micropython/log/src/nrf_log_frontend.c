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

#include "sdk_common.h"

#if NRF_LOG_CLI_CMDS
#error "NRF_LOG_CLI_CMDS is not supported."
#endif


#if NRF_MODULE_ENABLED(NRF_LOG)

#include "app_util.h"
#include "app_util_platform.h"
#include "nrf_log.h"
#include "nrf_log_internal.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_str_formatter.h"
#include <string.h>


/**
 * brief An internal control block of the logger
 */
typedef struct
{
    nrf_log_timestamp_func_t  timestamp_func;  // A pointer to function that returns timestamp
} log_data_t;

static log_data_t   m_log_data;


NRF_LOG_MODULE_REGISTER();

// Helper macros for section variables.
#define NRF_LOG_DYNAMIC_SECTION_VARS_GET(i)        NRF_SECTION_ITEM_GET(log_dynamic_data, nrf_log_module_dynamic_data_t, (i))
#define NRF_LOG_FILTER_SECTION_VARS_GET(i)         NRF_SECTION_ITEM_GET(log_filter_data, nrf_log_module_filter_data_t, (i))

#define NRF_LOG_CONST_SECTION_VARS_GET(i)          NRF_SECTION_ITEM_GET(log_const_data, nrf_log_module_const_data_t, (i))
#define NRF_LOG_CONST_SECTION_VARS_COUNT           NRF_SECTION_ITEM_COUNT(log_const_data, nrf_log_module_const_data_t)

ret_code_t nrf_log_init(nrf_log_timestamp_func_t timestamp_func, uint32_t timestamp_freq)
{
    (void)NRF_LOG_ITEM_DATA_CONST(app);

    if (NRF_LOG_USES_TIMESTAMP && (timestamp_func == NULL))
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    if (NRF_LOG_USES_TIMESTAMP)
    {
        nrf_log_str_formatter_timestamp_freq_set(timestamp_freq);
        m_log_data.timestamp_func = timestamp_func;
    }

    uint32_t modules_cnt = NRF_LOG_CONST_SECTION_VARS_COUNT;
    uint32_t i;
    if (NRF_LOG_FILTERS_ENABLED)
    {
        uint32_t j;
        //sort modules by name
        for (i = 0; i < modules_cnt; i++)
        {
            uint32_t idx = 0;

            for (j = 0; j < modules_cnt; j++)
            {
                if  (i != j)
                {
                    char const * p_name0 = NRF_LOG_CONST_SECTION_VARS_GET(i)->p_module_name;
                    char const * p_name1 = NRF_LOG_CONST_SECTION_VARS_GET(j)->p_module_name;
                    if (strncmp(p_name0, p_name1, 20) > 0)
                    {
                        idx++;
                    }
                }

            }
            nrf_log_module_dynamic_data_t * p_module_ddata = NRF_LOG_DYNAMIC_SECTION_VARS_GET(i);
            p_module_ddata->order_idx = idx;
        }

        /* Initialize filters */
        for (i = 0; i < modules_cnt; i++)
        {
            nrf_log_module_dynamic_data_t * p_module_ddata = NRF_LOG_DYNAMIC_SECTION_VARS_GET(i);
            nrf_log_module_filter_data_t * p_module_filter = NRF_LOG_FILTER_SECTION_VARS_GET(i);
            p_module_ddata->filter = 0;
            p_module_filter->filter_lvls = 0;
        }
    }

    return NRF_SUCCESS;
}

uint32_t nrf_log_module_cnt_get(void)
{
    return NRF_LOG_CONST_SECTION_VARS_COUNT;
}

static ret_code_t module_idx_get(uint32_t * p_idx, bool ordered_idx)
{
    if (ordered_idx)
    {
        uint32_t module_cnt = nrf_log_module_cnt_get();
        uint32_t i;
        for (i = 0; i < module_cnt; i++)
        {
            nrf_log_module_dynamic_data_t * p_module_data = NRF_LOG_DYNAMIC_SECTION_VARS_GET(i);
            if (p_module_data->order_idx == *p_idx)
            {
                *p_idx = i;
                return NRF_SUCCESS;
            }
        }
        return NRF_ERROR_NOT_FOUND;
    }
    else
    {
        return NRF_SUCCESS;
    }
}
const char * nrf_log_module_name_get(uint32_t module_id, bool ordered_idx)
{
    if (module_idx_get(&module_id, ordered_idx) == NRF_SUCCESS)
    {
        nrf_log_module_const_data_t * p_module_data = NRF_LOG_CONST_SECTION_VARS_GET(module_id);
        return p_module_data->p_module_name;
    }
    else
    {
        return NULL;
    }
}

uint8_t nrf_log_color_id_get(uint32_t module_id, nrf_log_severity_t severity)
{
    nrf_log_module_const_data_t * p_module_data = NRF_LOG_CONST_SECTION_VARS_GET(module_id);
    uint8_t color_id;
    switch (severity)
    {
    case NRF_LOG_SEVERITY_ERROR:
        color_id = NRF_LOG_ERROR_COLOR;
        break;
    case NRF_LOG_SEVERITY_WARNING:
        color_id = NRF_LOG_WARNING_COLOR;
        break;
    case NRF_LOG_SEVERITY_INFO:
        color_id = p_module_data->info_color_id;
        break;
    case NRF_LOG_SEVERITY_DEBUG:
        color_id = p_module_data->debug_color_id;
        break;
    default:
        color_id = 0;
        break;
    }
    return color_id;
}

static uint16_t higher_lvl_get(uint32_t lvls)
{
    uint16_t top_lvl = 0;
    uint16_t tmp_lvl;
    uint32_t i;

    //Find highest level enabled by backends
    for (i = 0; i < (32/NRF_LOG_LEVEL_BITS); i+=NRF_LOG_LEVEL_BITS)
    {
        tmp_lvl = (uint16_t)BF_GET(lvls,NRF_LOG_LEVEL_BITS, i);
        if (tmp_lvl > top_lvl)
        {
            top_lvl = tmp_lvl;
        }
    }
    return top_lvl;
}

void nrf_log_module_filter_set(uint32_t backend_id, uint32_t module_id, nrf_log_severity_t severity)
{
    if (NRF_LOG_FILTERS_ENABLED)
    {
        nrf_log_module_dynamic_data_t * p_module_data = NRF_LOG_DYNAMIC_SECTION_VARS_GET(module_id);
        nrf_log_module_filter_data_t *  p_filter      = NRF_LOG_FILTER_SECTION_VARS_GET(module_id);

        p_filter->filter_lvls &= ~BF_MASK(NRF_LOG_LEVEL_BITS, (NRF_LOG_LEVEL_BITS * backend_id));
        p_filter->filter_lvls |= BF_VAL(severity, NRF_LOG_LEVEL_BITS, (NRF_LOG_LEVEL_BITS * backend_id));

        p_module_data->filter = higher_lvl_get(p_filter->filter_lvls);
    }
}

nrf_log_severity_t nrf_log_module_filter_get(uint32_t backend_id,
                                             uint32_t module_id,
                                             bool ordered_idx,
                                             bool dynamic)
{
    nrf_log_severity_t severity = NRF_LOG_SEVERITY_NONE;
    if (NRF_LOG_FILTERS_ENABLED && dynamic)
    {
        if (module_idx_get(&module_id, ordered_idx) == NRF_SUCCESS)
        {
            nrf_log_module_filter_data_t * p_filter = NRF_LOG_FILTER_SECTION_VARS_GET(module_id);
            severity = (nrf_log_severity_t)BF_GET(p_filter->filter_lvls,
                                                  NRF_LOG_LEVEL_BITS,
                                                  (backend_id*NRF_LOG_LEVEL_BITS));
        }
    }
    else if (!dynamic)
    {
        if (module_idx_get(&module_id, ordered_idx) == NRF_SUCCESS)
        {
            nrf_log_module_const_data_t * p_module_data =
                                                NRF_LOG_CONST_SECTION_VARS_GET(module_id);
            severity = (nrf_log_severity_t)p_module_data->compiled_lvl;
        }
    }
    return severity;
}

static inline void set_formatter_entry_params(
        uint32_t severity_mid,
        nrf_log_str_formatter_entry_params_t *formatter_entry_params)
{
    memset(formatter_entry_params, 0, sizeof(nrf_log_str_formatter_entry_params_t));

    #if NRF_LOG_USES_TIMESTAMP
    formatter_entry_params.timestamp = m_log_data.timestamp_func();
    #endif // NRF_LOG_USES_TIMESTAMP

    formatter_entry_params->module_id = (uint16_t)(severity_mid >> NRF_LOG_MODULE_ID_POS);
    formatter_entry_params->severity = (uint16_t)(severity_mid & NRF_LOG_LEVEL_MASK);
    formatter_entry_params->use_colors = NRF_LOG_USES_COLORS;
}

static inline void std_n(uint32_t           severity_mid,
                         char const * const p_str,
                         uint32_t const *   args,
                         uint32_t           nargs)
{
    nrf_log_str_formatter_entry_params_t formatter_entry_params;

    set_formatter_entry_params(
            severity_mid,
            &formatter_entry_params);

    nrf_log_std_entry_process(
            p_str,
            args,
            nargs,
            &formatter_entry_params);
}

void nrf_log_frontend_std_0(uint32_t severity_mid, char const * const p_str)
{
    std_n(severity_mid, p_str, NULL, 0);
}


void nrf_log_frontend_std_1(uint32_t            severity_mid,
                            char const * const p_str,
                            uint32_t           val0)
{
    uint32_t args[] = {val0};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_2(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1)
{
    uint32_t args[] = {val0, val1};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_3(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2)
{
    uint32_t args[] = {val0, val1, val2};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_4(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2,
                            uint32_t           val3)
{
    uint32_t args[] = {val0, val1, val2, val3};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_5(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2,
                            uint32_t           val3,
                            uint32_t           val4)
{
    uint32_t args[] = {val0, val1, val2, val3, val4};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_std_6(uint32_t           severity_mid,
                            char const * const p_str,
                            uint32_t           val0,
                            uint32_t           val1,
                            uint32_t           val2,
                            uint32_t           val3,
                            uint32_t           val4,
                            uint32_t           val5)
{
    uint32_t args[] = {val0, val1, val2, val3, val4, val5};
    std_n(severity_mid, p_str, args,  ARRAY_SIZE(args));
}


void nrf_log_frontend_hexdump(uint32_t           severity_mid,
                              const void * const p_data,
                              uint16_t           length)
{
    nrf_log_str_formatter_entry_params_t formatter_entry_params;

    set_formatter_entry_params(
            severity_mid,
            &formatter_entry_params);

    nrf_log_hexdump_entry_process(
            (const uint8_t *)p_data,
            length,
            &formatter_entry_params);
}

#endif // NRF_MODULE_ENABLED(NRF_LOG)

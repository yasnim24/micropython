/**
 * Copyright (c) 2018 - 2019, Nordic Semiconductor ASA
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
// 24-05-14
// Adapted for MicroPython - used by 'nrf_crypto_rng_c'.
//

/** @file
 *
 * @defgroup nrf_stack_info Stack info functions and definitions
 * @{
 * @ingroup app_common
 *
 * @brief Functions and definitions used to obtain information about the state of the stack.
 */

#ifndef NRF_STACK_INFO_H__
#define NRF_STACK_INFO_H__

#include <stddef.h>
#include <stdbool.h>
#include "compiler_abstraction.h"

#include "py/stackctrl.h" // MP_STACK_CHECK

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Function for checking if the stack is currently overflowed.
 * 
 * @details This function checks if the stack is currently in an overflowed by comparing the stack
 *          pointer with the stack base address. Intended to be used to be used to ease debugging.
 * 
 * @return      true if stack is currently overflowed, false otherwise.
 */
__STATIC_INLINE bool nrf_stack_info_overflowed(void)
{
     // If MICROPY_STACK_CHECK is set and an overflow occurs
     // MP_STACK_CHECK() will throw an exception.
     MP_STACK_CHECK();

     return false;
}


#ifdef __cplusplus
}
#endif

#endif // NRF_STACK_INFO_H__

/**@} */

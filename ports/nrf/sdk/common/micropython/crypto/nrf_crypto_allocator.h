/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Yasmin Bosch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//
// This header file defines user memory allocation macros for the
// nRF5 SDK 15.3.0 crypto library.
//
// It is included by the nRF5 SDK 15.3.0 crypto library if
// NRF_CRYPTO_ALLOCATOR is set to NRF_CRYPTO_ALLOCATOR_USER (1).
//

#ifndef SDK_MICROPYTHON_CRYPTO_NRF_CRYPTO_ALLOCATOR_H_
#define SDK_MICROPYTHON_CRYPTO_NRF_CRYPTO_ALLOCATOR_H_

#include "py/mpconfig.h"
#include "py/misc.h" // m_malloc, m_free

#if !MICROPY_TRACKED_ALLOC
// It is assumed that this header file will only be used if
// MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING is set.
#error "MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING requires MICROPY_TRACKED_ALLOC."
#endif

// See documentation in 'nrf_crypto_mem.h'.
#define NRF_CRYPTO_ALLOC(size)    (m_tracked_calloc(1, (size_t)(size)))
#define NRF_CRYPTO_FREE(p_buffer) (m_tracked_free((void *)(p_buffer)))
#define NRF_CRYPTO_ALLOC_ON_STACK 0

#endif /* SDK_MICROPYTHON_CRYPTO_NRF_CRYPTO_ALLOCATOR_H_ */

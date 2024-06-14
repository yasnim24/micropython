/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ayke van Laethem
 * Copyright (c) 2019-2020 Jim Mussared
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


#if MICROPY_PY_BLUETOOTH

static mp_obj_t bluetooth_ble_config_ext(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    if (kwargs->used == 0) {
        // Get config value
        if (n_args != 2) {
            mp_raise_TypeError(MP_ERROR_TEXT("must query one param"));
        }

        switch (mp_obj_str_get_qstr(args[1])) {
            case MP_QSTR_gap_name: {
                const uint8_t *buf;
                size_t len = mp_bluetooth_gap_get_device_name(&buf);
                return mp_obj_new_bytes(buf, len);
            }
            case MP_QSTR_mac: {
                uint8_t addr_type;
                uint8_t addr[6];
                mp_bluetooth_get_current_address(&addr_type, addr);
                mp_obj_t items[] = { MP_OBJ_NEW_SMALL_INT(addr_type), mp_obj_new_bytes(addr, MP_ARRAY_SIZE(addr)) };
                return mp_obj_new_tuple(2, items);
            }
            #if !MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS
            case MP_QSTR_rxbuf: {
                mp_obj_bluetooth_ble_t *self = MP_OBJ_TO_PTR(args[0]);
                return mp_obj_new_int(self->ringbuf.size);
            }
            #endif
            case MP_QSTR_mtu:
                return mp_obj_new_int(mp_bluetooth_get_preferred_mtu());
            default:
                mp_raise_ValueError(MP_ERROR_TEXT("unknown config param"));
        }
    } else {
        // Set config value(s)
        if (n_args != 1) {
            mp_raise_TypeError(MP_ERROR_TEXT("can't specify pos and kw args"));
        }

        for (size_t i = 0; i < kwargs->alloc; ++i) {
            if (MP_MAP_SLOT_IS_FILLED(kwargs, i)) {
                mp_map_elem_t *e = &kwargs->table[i];
                switch (mp_obj_str_get_qstr(e->key)) {
                    case MP_QSTR_gap_name: {
                        mp_buffer_info_t bufinfo;
                        mp_get_buffer_raise(e->value, &bufinfo, MP_BUFFER_READ);
                        bluetooth_handle_errno(mp_bluetooth_gap_set_device_name(bufinfo.buf, bufinfo.len));
                        break;
                    }
                    #if !MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS
                    case MP_QSTR_rxbuf: {
                        // Determine new buffer sizes
                        mp_int_t ringbuf_alloc = mp_obj_get_int(e->value);
                        if (ringbuf_alloc < 16 || ringbuf_alloc > 0xffff) {
                            mp_raise_ValueError(NULL);
                        }
                        size_t irq_data_alloc = MICROPY_PY_BLUETOOTH_MAX_EVENT_DATA_BYTES_LEN(ringbuf_alloc);

                        // Allocate new buffers
                        uint8_t *ringbuf = m_new(uint8_t, ringbuf_alloc);
                        uint8_t *irq_data = m_new(uint8_t, irq_data_alloc);

                        // Get old buffer sizes and pointers
                        mp_obj_bluetooth_ble_t *self = MP_OBJ_TO_PTR(args[0]);
                        uint8_t *old_ringbuf_buf = self->ringbuf.buf;
                        size_t old_ringbuf_alloc = self->ringbuf.size;
                        uint8_t *old_irq_data_buf = (uint8_t *)self->irq_data_data.items;
                        size_t old_irq_data_alloc = self->irq_data_data_alloc;

                        // Atomically update the ringbuf and irq data
                        MICROPY_PY_BLUETOOTH_ENTER
                        self->ringbuf.size = ringbuf_alloc;
                        self->ringbuf.buf = ringbuf;
                        self->ringbuf.iget = 0;
                        self->ringbuf.iput = 0;
                        self->irq_data_data_alloc = irq_data_alloc;
                        self->irq_data_data.items = irq_data;
                        MICROPY_PY_BLUETOOTH_EXIT

                        // Free old buffers
                        m_del(uint8_t, old_ringbuf_buf, old_ringbuf_alloc);
                        m_del(uint8_t, old_irq_data_buf, old_irq_data_alloc);
                        break;
                    }
                    #endif
                    case MP_QSTR_mtu: {
                        mp_int_t mtu = mp_obj_get_int(e->value);
                        bluetooth_handle_errno(mp_bluetooth_set_preferred_mtu(mtu));
                        break;
                    }
                    case MP_QSTR_addr_mode: {
                        mp_int_t addr_mode = mp_obj_get_int(e->value);
                        mp_bluetooth_set_address_mode(addr_mode);
                        break;
                    }
                    #if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
                    case MP_QSTR_bond: {
                        bool bonding_enabled = mp_obj_is_true(e->value);
                        mp_bluetooth_set_bonding(bonding_enabled);
                        break;
                    }
                    case MP_QSTR_mitm: {
                        bool mitm_protection = mp_obj_is_true(e->value);
                        mp_bluetooth_set_mitm_protection(mitm_protection);
                        break;
                    }
                    case MP_QSTR_io: {
                        mp_int_t io_capability = mp_obj_get_int(e->value);
                        mp_bluetooth_set_io_capability(io_capability);
                        break;
                    }
                    case MP_QSTR_le_secure: {
                        bool le_secure_required = mp_obj_is_true(e->value);
                        mp_bluetooth_set_le_secure(le_secure_required);
                        break;
                    }
                    #endif // MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
                    default:
                        mp_raise_ValueError(MP_ERROR_TEXT("unknown config param"));
                }
            }
        }

        return mp_const_none;
    }
}

#endif // MICROPY_PY_BLUETOOTH

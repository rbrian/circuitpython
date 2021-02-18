/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
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

#include "py/mphal.h"
#include "shared-bindings/busio/I2C.h"
#include "py/runtime.h"

#include "shared-bindings/microcontroller/__init__.h"
#include "shared-bindings/bitbangio/I2C.h"

#include "src/rp2_common/hardware_gpio/include/hardware/gpio.h"

// Synopsys  DW_apb_i2c  (v2.01)  IP

#define NO_PIN 0xff

STATIC bool never_reset_i2c[2];
STATIC i2c_inst_t* i2c[2] = {i2c0, i2c1};

void reset_i2c(void) {
    for (size_t i = 0; i < 2; i++) {
        if (never_reset_i2c[i]) {
            continue;
        }

        i2c_deinit(i2c[i]);
    }
}

void common_hal_busio_i2c_construct(busio_i2c_obj_t *self,
        const mcu_pin_obj_t* scl, const mcu_pin_obj_t* sda, uint32_t frequency, uint32_t timeout) {
    self->peripheral = NULL;
    // I2C pins have a regular pattern. SCL is always odd and SDA is even. They match up in pairs
    // so we can divide by two to get the instance. This pattern repeats.
    if (scl->number % 2 == 1 && sda->number % 2 == 0 && scl->number / 2 == sda->number / 2) {
        size_t instance = (scl->number / 2) % 2;
        self->peripheral = i2c[instance];
    }
    if (self->peripheral == NULL) {
        mp_raise_ValueError(translate("Invalid pins"));
    }
    if ((i2c_get_hw(self->peripheral)->enable & I2C_IC_ENABLE_ENABLE_BITS) != 0) {
        mp_raise_ValueError(translate("I2C peripheral in use"));
    }
    if (frequency > 1000000) {
        mp_raise_ValueError(translate("Unsupported baudrate"));
    }

#if CIRCUITPY_REQUIRE_I2C_PULLUPS
    // Test that the pins are in a high state. (Hopefully indicating they are pulled up.)
    gpio_set_function(sda->number, GPIO_FUNC_SIO);
    gpio_set_function(scl->number, GPIO_FUNC_SIO);
    gpio_set_dir(sda->number, GPIO_IN);
    gpio_set_dir(scl->number, GPIO_IN);

    gpio_set_pulls(sda->number, false, true);
    gpio_set_pulls(scl->number, false, true);

    common_hal_mcu_delay_us(10);

    gpio_set_pulls(sda->number, false, false);
    gpio_set_pulls(scl->number, false, false);

    // We must pull up within 3us to achieve 400khz.
    common_hal_mcu_delay_us(3);

    if (!gpio_get(sda->number) || !gpio_get(scl->number)) {
        reset_pin_number(sda->number);
        reset_pin_number(scl->number);
        mp_raise_RuntimeError(translate("No pull up found on SDA or SCL; check your wiring"));
    }
#endif

    gpio_set_function(sda->number, GPIO_FUNC_I2C);
    gpio_set_function(scl->number, GPIO_FUNC_I2C);

    self->baudrate = i2c_init(self->peripheral, frequency);

    // Remember these in case we need to use bitbangio to do short writes.
    self->sda = sda;
    self->scl = scl;
    self->frequency = frequency;
    self->timeout = timeout;

    self->sda_pin = sda->number;
    self->scl_pin = scl->number;
    claim_pin(sda);
    claim_pin(scl);
}

bool common_hal_busio_i2c_deinited(busio_i2c_obj_t *self) {
    return self->sda_pin == NO_PIN;
}

void common_hal_busio_i2c_deinit(busio_i2c_obj_t *self) {
    if (common_hal_busio_i2c_deinited(self)) {
        return;
    }
    never_reset_i2c[i2c_hw_index(self->peripheral)] = false;

    i2c_deinit(self->peripheral);

    reset_pin_number(self->sda_pin);
    reset_pin_number(self->scl_pin);
    self->sda_pin = NO_PIN;
    self->scl_pin = NO_PIN;
}

bool common_hal_busio_i2c_probe(busio_i2c_obj_t *self, uint8_t addr) {
    return common_hal_busio_i2c_write(self, addr, NULL, 0, false) == 0;
}

bool common_hal_busio_i2c_try_lock(busio_i2c_obj_t *self) {
    bool grabbed_lock = false;
    if (!self->has_lock) {
        grabbed_lock = true;
        self->has_lock = true;
    }
    return grabbed_lock;
}

bool common_hal_busio_i2c_has_lock(busio_i2c_obj_t *self) {
    return self->has_lock;
}

void common_hal_busio_i2c_unlock(busio_i2c_obj_t *self) {
    self->has_lock = false;
}

uint8_t common_hal_busio_i2c_write(busio_i2c_obj_t *self, uint16_t addr,
                                   const uint8_t *data, size_t len, bool transmit_stop_bit) {
    if (len <= 2) {
        // Give up pin ownership temporarily and use a bitbangio.I2C to do the write.

        reset_pin_number(self->sda_pin);
        reset_pin_number(self->scl_pin);
        bitbangio_i2c_obj_t bitbangio_i2c;
        shared_module_bitbangio_i2c_construct(&bitbangio_i2c, self->sda, self->scl,
                                              self->frequency, self->timeout);
        uint8_t status = shared_module_bitbangio_i2c_write(&bitbangio_i2c, addr, data, len, transmit_stop_bit);
        shared_module_bitbangio_i2c_deinit(&bitbangio_i2c);

        // Take back the pins and restore them to hardware I2C functionality.
        claim_pin(self->sda);
        claim_pin(self->scl);

        gpio_set_function(self->sda_pin, GPIO_FUNC_I2C);
        gpio_set_function(self->scl_pin, GPIO_FUNC_I2C);

        return status;
    }

    int result = i2c_write_blocking(self->peripheral, addr, data, len, !transmit_stop_bit);
    if (result == len) {
        return 0;
    } else if (result == PICO_ERROR_GENERIC) {
        return MP_ENODEV;
    }
    return MP_EIO;
}

uint8_t common_hal_busio_i2c_read(busio_i2c_obj_t *self, uint16_t addr,
        uint8_t *data, size_t len) {
    int result = i2c_read_blocking(self->peripheral, addr, data, len, false);
    if (result == len) {
        return 0;
    } else if (result == PICO_ERROR_GENERIC) {
        return MP_ENODEV;
    }
    return MP_EIO;
}

void common_hal_busio_i2c_never_reset(busio_i2c_obj_t *self) {
    never_reset_i2c[i2c_hw_index(self->peripheral)] = true;

    never_reset_pin_number(self->scl_pin);
    never_reset_pin_number(self->sda_pin);
}

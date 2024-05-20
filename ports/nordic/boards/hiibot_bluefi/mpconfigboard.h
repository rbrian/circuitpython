// This file is part of the CircuitPython project: https://circuitpython.org
//
// SPDX-FileCopyrightText: Copyright (c) 2016 Glenn Ruben Bakke
// SPDX-FileCopyrightText: Copyright (c) 2018 Dan Halbert for Adafruit Industries
//
// SPDX-License-Identifier: MIT

#pragma once

#include "nrfx/hal/nrf_gpio.h"

#define MICROPY_HW_BOARD_NAME       "HiiBot BlueFi"
#define MICROPY_HW_MCU_NAME         "nRF52840"

#define MICROPY_HW_NEOPIXEL     (&pin_P1_10)  // P18 / D18

#define MICROPY_HW_LED_STATUS   (&pin_P1_12)  // P17 / D17

#if QSPI_FLASH_FILESYSTEM
#define MICROPY_QSPI_DATA0      NRF_GPIO_PIN_MAP(1, 1)
#define MICROPY_QSPI_DATA1      NRF_GPIO_PIN_MAP(1, 4)
#define MICROPY_QSPI_DATA2      NRF_GPIO_PIN_MAP(1, 6)
#define MICROPY_QSPI_DATA3      NRF_GPIO_PIN_MAP(1, 5)
#define MICROPY_QSPI_SCK        NRF_GPIO_PIN_MAP(1, 3)
#define MICROPY_QSPI_CS         NRF_GPIO_PIN_MAP(1, 2)
#endif

#if SPI_FLASH_FILESYSTEM
#define SPI_FLASH_MOSI_PIN      &pin_P1_01
#define SPI_FLASH_MISO_PIN      &pin_P1_04
#define SPI_FLASH_SCK_PIN       &pin_P1_03
#define SPI_FLASH_CS_PIN        &pin_P1_02
#endif

// No 32kHz crystal. THere's a 32MHz crystal in the nRF module.
#define BOARD_HAS_32KHZ_XTAL    (0)

#define DEFAULT_I2C_BUS_SCL     (&pin_P0_00)
#define DEFAULT_I2C_BUS_SDA     (&pin_P0_31)

#define DEFAULT_SPI_BUS_SCK     (&pin_P0_06)
#define DEFAULT_SPI_BUS_MOSI    (&pin_P0_26)
#define DEFAULT_SPI_BUS_MISO    (&pin_P0_04)

#define DEFAULT_UART_BUS_RX     (&pin_P0_28)
#define DEFAULT_UART_BUS_TX     (&pin_P0_02)

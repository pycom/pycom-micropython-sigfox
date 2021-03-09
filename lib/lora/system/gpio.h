/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2021, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 *
 * This file contains code under the following copyright and licensing notices.
 * The code has been changed but otherwise retained.
 */

/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Generic GPIO driver implementation

Comment: Relies on the specific board GPIO implementation as well as on
         IO expander driver implementation if one is available on the target
         board.

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#ifndef __LORA_GPIO_H__
#define __LORA_GPIO_H__

#include "py/obj.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "machpin.h"

#include "pinName-board.h"

/*!
 * Board GPIO pin names
 */
typedef enum
{
    MCU_PINS,

    // Not connected
    NC = (int)0xFFFFFFFF
}PinNames;

/*!
 * Operation Mode for the GPIO
 */
typedef enum
{
    PIN_INPUT = GPIO_MODE_INPUT,
    PIN_OUTPUT = GPIO_MODE_OUTPUT
}PinModes;

/*!
 * Add a pull-up, a pull-down or nothing on the GPIO line
 */
typedef enum
{
    PIN_NO_PULL = MACHPIN_PULL_NONE,
    PIN_PULL_UP = MACHPIN_PULL_UP,
    PIN_PULL_DOWN = MACHPIN_PULL_DOWN
}PinTypes;

/*!
 * Define the GPIO as Push-pull type or Open Drain
 */
typedef enum
{
    PIN_PUSH_PULL = 0,
    PIN_OPEN_DRAIN
}PinConfigs;

/*!
 * Define the GPIO IRQ on a rising, falling or both edges
 */
typedef enum
{
    NO_IRQ = 0xFF,
    IRQ_RISING_EDGE = GPIO_INTR_POSEDGE,
    IRQ_FALLING_EDGE = GPIO_INTR_NEGEDGE,
    IRQ_RISING_FALLING_EDGE = GPIO_INTR_ANYEDGE
}IrqModes;

/*!
 * Define the IRQ priority on the GPIO
 */
typedef enum
{
    IRQ_VERY_LOW_PRIORITY = 0,
    IRQ_LOW_PRIORITY,
    IRQ_MEDIUM_PRIORITY,
    IRQ_HIGH_PRIORITY,
    IRQ_VERY_HIGH_PRIORITY
}IrqPriorities;

/*!
 * Structure for the GPIO
 */
typedef struct
{
    pin_obj_t *pin_obj;
} Gpio_t;

/*!
 * GPIO IRQ handler function prototype
 */
typedef void( GpioIrqHandler )( void );

/*!
 * \brief Initializes the given GPIO object
 *
 * \param [IN] obj    Pointer to the GPIO object
 * \param [IN] pin    Pin name ( please look in pinName-board.h file )
 * \param [IN] mode   Pin mode [PIN_INPUT, PIN_OUTPUT,
 *                              PIN_ALTERNATE_FCT, PIN_ANALOGIC]
 * \param [IN] config Pin config [PIN_PUSH_PULL, PIN_OPEN_DRAIN]
 * \param [IN] type   Pin type [PIN_NO_PULL, PIN_PULL_UP, PIN_PULL_DOWN]
 * \param [IN] value  Default output value at initialisation
 */
void GpioInit( Gpio_t *obj, PinNames pin, PinModes mode, PinConfigs config, PinTypes type, uint32_t value );

/*!
 * \brief GPIO IRQ Initialization
 *
 * \param [IN] obj         Pointer to the GPIO object
 * \param [IN] irqMode     IRQ mode [NO_IRQ, IRQ_RISING_EDGE,
 *                                   IRQ_FALLING_EDGE, IRQ_RISING_FALLING_EDGE]
 * \param [IN] irqPriority IRQ priority [IRQ_VERY_LOW_PRIORITY, IRQ_LOW_PRIORITY
 *                                       IRQ_MEDIUM_PRIORITY, IRQ_HIGH_PRIORITY
 *                                       IRQ_VERY_HIGH_PRIORITY]
 * \param [IN] irqHandler  Callback function pointer
 */
void GpioSetInterrupt( Gpio_t *obj, IrqModes irqMode, IrqPriorities irqPriority, GpioIrqHandler *irqHandler );

/*!
 * \brief Removes the interrupt from the object
 *
 * \param [IN] obj Pointer to the GPIO object
 */
void GpioRemoveInterrupt( Gpio_t *obj );

/*!
 * \brief Writes the given value to the GPIO output
 *
 * \param [IN] obj   Pointer to the GPIO object
 * \param [IN] value New GPIO output value
 */
void GpioWrite( Gpio_t *obj, uint32_t value );

#endif // __LORA_GPIO_H__

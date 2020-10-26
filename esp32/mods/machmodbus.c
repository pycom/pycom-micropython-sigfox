/*
 * Copyright (c) 2020, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "readline.h"
#include "serverstask.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "netutils.h"
#include "modwlan.h"

#include "esp_types.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_err.h"

#include "esp32/rom/ets_sys.h"
#include "soc/uart_struct.h"
#include "soc/dport_reg.h"
#include "soc/gpio_sig_map.h"

#include "uart.h"
#include "machuart.h"
#include "mpexception.h"
#include "utils/interrupt_char.h"
#include "moduos.h"
#include "machpin.h"
#include "pins.h"
#include "periph_ctrl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"

#include "mbcontroller.h"       // for mbcontroller defines and api
#include "machmodbus.h"
#include "bufhelper.h"

/// \moduleref machine

/******************************************************************************
 DEFINE CONSTANTS
 *******-***********************************************************************/
////COMMON FOR BOTH MASTER AND SLAVE
#define MB_PORT_NUM     (2)           // Number of UART port used for Modbus connection
#define MB_DEV_ADDR     (1)           // The address of device in Modbus network
#define MB_DEV_SPEED    (9600)      // The communication speed of the UART

////SLAVE
// Defines below are used to define register start address for each type of Modbus registers
#define MB_REG_DISCRETE_INPUT_START         (0x0000)
#define MB_REG_INPUT_START                  (0x0000)
#define MB_REG_HOLDING_START                (0x0000)
#define MB_REG_COILS_START                  (0x0000)

#define MB_READ_MASK                        (MB_EVENT_INPUT_REG_RD \
                                                | MB_EVENT_HOLDING_REG_RD \
                                                | MB_EVENT_DISCRETE_RD \
                                                | MB_EVENT_COILS_RD)
#define MB_WRITE_MASK                       (MB_EVENT_HOLDING_REG_WR \
                                                | MB_EVENT_COILS_WR)
#define MB_READ_WRITE_MASK                  (MB_READ_MASK | MB_WRITE_MASK)

#define MACH_UART_CHECK_INIT(self)                    \
    if(!(init)) {nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "MODBUS not Initialized!"));}

////TCP PORT
#define MB_TCP_PORT_NUMBER      (502)

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

STATIC mp_obj_t mach_modbus_deinit(mp_obj_t self_in);
STATIC void mach_modbus_serial_slave_init (const mach_modbus_obj_t *self);
STATIC void mach_modbus_serial_master_init (const mach_modbus_obj_t *self);
STATIC void mach_modbus_tcp_slave_init (const mach_modbus_obj_t *self);
STATIC void mach_modbus_tcp_master_init (const mach_modbus_obj_t *self);

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
struct _mach_modbus_obj_t {
    mp_obj_base_t base;
    uint8_t n_pins;
    uint8_t bus_id;
    gpio_num_t tx;
    gpio_num_t rx;
    gpio_num_t rts;
    bool init;
    mb_communication_info_t comm_info; // Modbus communication parameters
};

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static mach_modbus_obj_t mach_modbus_obj;

mb_port_type_t port_type;

////SLAVE
mb_param_info_t reg_info; // keeps the Modbus registers access information
mb_register_area_descriptor_t reg_area; // Modbus register area descriptor structure

////MASTER
mb_param_request_t setparam;

////COMMON
uint32_t holding_registers_length = 0;
uint16_t *holding_registers = NULL;

uint32_t input_registers_length = 0;
uint16_t *input_registers = NULL;

uint8_t coils_length = 0;
uint8_t *coils = NULL;

uint8_t discrete_inputs_length = 0;
uint8_t *discrete_inputs = NULL;

uint32_t role;

//added from idf_v4.1
void* mbc_slave_handler = NULL;
void* master_handler = NULL;

static portMUX_TYPE param_lock = portMUX_INITIALIZER_UNLOCKED;

STATIC const mp_obj_t mach_modbus_def_pin[1][3] = { {&PIN_MODULE_P3, &PIN_MODULE_P4, &PIN_MODULE_P11} };

char* slave_ip_address_table[2] = {
    NULL, // ip address [0]
    NULL, // table must be NULL terminated
};

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

void modbus_deinit_all (void) {
    mach_modbus_deinit(&mach_modbus_obj);
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
void setup_reg_data(void) {
    if (holding_registers_length > 0) {
        holding_registers = malloc(sizeof(uint16_t) * holding_registers_length);
        for (int i=0; i<holding_registers_length; i++) {
            holding_registers[i] = 0;
        }
        reg_area.type = MB_PARAM_HOLDING; // Set type of register area
        reg_area.start_offset = MB_REG_HOLDING_START; // Offset of register area in Modbus protocol
        reg_area.address = (void*)holding_registers;
        reg_area.size = holding_registers_length*2;
        mbc_slave_set_descriptor(reg_area); //mbcontroller_set_descriptor(reg_area);
    }
    else nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid holding registers length: %d", holding_registers_length));

    if (input_registers_length > 0) {
        input_registers = malloc(sizeof(uint16_t) * input_registers_length);
        for (int i=0; i<input_registers_length; i++) {
            input_registers[i] = 0;
        }
        reg_area.type = MB_PARAM_INPUT; // Set type of register area
        reg_area.start_offset = MB_REG_INPUT_START; // Offset of register area in Modbus protocol
        reg_area.address = (void*)input_registers;
        reg_area.size = input_registers_length*2;
        mbc_slave_set_descriptor(reg_area); //mbcontroller_set_descriptor(reg_area);
    }
    else nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid input registers length: %d", input_registers_length));

    if (coils_length > 0) {
        uint32_t len = coils_length/8;
        if (coils_length%8) {
            len++;
        }
        coils = malloc(sizeof(uint8_t) * len);
        for (int i=0; i<len; i++) {
            coils[i] = 0;
        }
        reg_area.type = MB_PARAM_COIL; // Set type of register area
        reg_area.start_offset = MB_REG_COILS_START; // Offset of register area in Modbus protocol
        reg_area.address = (void*)coils;
        reg_area.size = len;
        mbc_slave_set_descriptor(reg_area); //mbcontroller_set_descriptor(reg_area);
    }
    else nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid coils length: %d", coils_length));

    if (discrete_inputs_length > 0) {
        uint32_t len = discrete_inputs_length/8;
        if (discrete_inputs_length%8) {
            len++;
        }
        discrete_inputs = malloc(sizeof(uint8_t) * len);
        for (int i=0; i<len; i++) {
            discrete_inputs[i] = 0;
        }
        reg_area.type = MB_PARAM_DISCRETE; // Set type of register area
        reg_area.start_offset = MB_REG_DISCRETE_INPUT_START; // Offset of register area in Modbus protocol
        reg_area.address = (void*)discrete_inputs;
        reg_area.size = len;
        mbc_slave_set_descriptor(reg_area); //mbcontroller_set_descriptor(reg_area);
    }
    else nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid discrete inputs length: %d", discrete_inputs_length));
}

STATIC void mach_modbus_serial_slave_init (const mach_modbus_obj_t *self) {
    port_type = MB_PORT_SERIAL_SLAVE;
    esp_err_t err = mbc_slave_init(port_type, &mbc_slave_handler); // Initialization of Modbus controller
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "slave init error: %d", err));
    }
    err = mbc_slave_setup((void*)&(self->comm_info));
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "slave setup error: %d", err));
    }

    setup_reg_data();

    // Starts of modbus controller and stack
    err = mbc_slave_start();
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "slave start error: %d", err));
    }

    // Set UART pin numbers
    err = uart_set_pin(self->comm_info.port, self->tx, self->rx, self->rts, UART_PIN_NO_CHANGE);
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "uart set pin error: %d", err));
    }

    // Set UART driver mode to Half Duplex
    err = uart_set_mode(self->comm_info.port, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "uart set mode error: %d", err));
    }
}

STATIC void mach_modbus_serial_master_init (const mach_modbus_obj_t *self) {
     // Initialization of device peripheral and objects
    
    //consists of mbc_master_init, mbc_master_setup, uart_set_pin, mbc_master_start, uart_set_mode, mbc_master_set_descriptor
    // master_init(); 
    port_type = MB_PORT_SERIAL_MASTER;
    esp_err_t err = mbc_master_init(port_type, &master_handler);
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "master init error: %d", err));
    }
    err = mbc_master_setup((void*)&(self->comm_info));
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "master setup error: %d", err));
    }
    // Set UART pin numbers
    err = uart_set_pin(self->comm_info.port, self->tx, self->rx, self->rts, UART_PIN_NO_CHANGE);
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "uart set pin error: %d", err));
    }

    err = mbc_master_start();
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "master start error: %d", err));
    }
    // Set UART driver mode to Half Duplex
    err = uart_set_mode(self->comm_info.port, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "uart set mode error: %d", err));
    }
    
    //put proper delay
    vTaskDelay(10);
    
    //we won"t use this because the func which in this func will be in the each request.
    // master_operation_func(NULL);
}

STATIC void mach_modbus_tcp_slave_init (const mach_modbus_obj_t *self) {
    esp_err_t err = mbc_slave_init_tcp(&mbc_slave_handler);
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "modbus_slave_init_tcp error: %d", err));
    }

    err = mbc_slave_setup((void*)&(self->comm_info));
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "modbus_slave_setup error: %d", err));
    }

    setup_reg_data();

    err = mbc_slave_start();
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "modbus_slave_start error: %d", err));
    }
}

STATIC void mach_modbus_tcp_master_init (const mach_modbus_obj_t *self) {

    esp_err_t err = mbc_master_init_tcp(&master_handler);
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "modbus_master_init_tcp error: %d", err));
    }

    err = mbc_master_setup((void*)&(self->comm_info));
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "modbus_master_setup error: %d", err));
    }

    err = mbc_master_start();
    if (err != ERR_OK) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "modbus_master_start error: %d", err));
    }

}

STATIC void mach_modbus_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mach_modbus_obj_t *self = self_in;
    if (self->init) {
        mp_printf(print, "Modbus(%u", self->bus_id);
        if (role == MASTER) {
            mp_printf(print, ", Modbus.%q", MP_QSTR_MASTER);
        }
        else if (role == SLAVE) {
            mp_printf(print, ", Modbus.%q", MP_QSTR_SLAVE);
        }
        if (self->comm_info.mode == MB_MODE_RTU) {
            mp_printf(print, ", mode=Modbus.%q", MP_QSTR_RTU);
        }
        else if (self->comm_info.mode == MB_MODE_ASCII) {
            mp_printf(print, ", mode=Modbus.%q", MP_QSTR_ASCII);
        }
        if (self->comm_info.ip_mode == MB_MODE_TCP) {
            mp_printf(print, ", mode=Modbus.%q", MP_QSTR_TCP);
        }
        if ((self->comm_info.mode == MB_MODE_RTU) || (self->comm_info.mode == MB_MODE_ASCII)) {
            if(role==SLAVE) {
                mp_printf(print, ", slave_addr=0x%X",self->comm_info.slave_addr);
            }
            mp_printf(print, ", port=%d",self->comm_info.port);
            mp_printf(print, ", baudrate=%d",self->comm_info.baudrate);

            if (self->comm_info.parity == UART_PARITY_DISABLE) {
                mp_print_str(print, ", parity=None");
            } else {
                mp_printf(print, ", parity=Modbus.%q", (self->comm_info.parity == UART_PARITY_EVEN) ? MP_QSTR_EVEN : MP_QSTR_ODD);
            }
        }
        else if (self->comm_info.mode == MB_MODE_TCP) {
            mp_printf(print, ", ip_mode=%d",self->comm_info.ip_mode);
            mp_printf(print, ", ip_port=%d",self->comm_info.ip_port);
            mp_printf(print, ", ip_addr_type=%q",MP_QSTR_IPV4);
            // mp_printf(print, ", slave_ip_addr=%d",master_get_slave_ip_stdin(slave_ip_address));

        }
        if(role==SLAVE) {
            mp_printf(print, ", holdingRegs=%d",holding_registers_length);
            mp_printf(print, ", inputRegs=%d",input_registers_length);
            mp_printf(print, ", coils=%d",coils_length);
            mp_printf(print, ", discreteInputs=%d",discrete_inputs_length);
        } 
        mp_printf(print, ")");
    }
    else {
        mp_printf(print, "Modbus(%u)", self->bus_id);
    }
}

STATIC mp_obj_t mach_modbus_init_helper(mach_modbus_obj_t *self, const mp_arg_val_t *args) {

    //role
    role = args[0].u_int;
    if (role != SLAVE && role != MASTER) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid role"));
    }

    //mode
    uint32_t mode = args[1].u_int;
    if (args[1].u_int < 0) {
        goto error;
    } else {
        if (mode != MB_MODE_ASCII && mode != MB_MODE_RTU && mode != MB_MODE_TCP) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid mode"));
        }
    }

    if (mode == MB_MODE_ASCII || mode == MB_MODE_RTU) { 
        //slave_addr
        uint32_t slave_addr;
        if (args[2].u_int <= 0) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid slave address: %d", args[2].u_int));
        }
        else {
            slave_addr = args[2].u_int;
        }

        //port
        uint32_t port;
        if (args[3].u_int < 0 && args[3].u_int > 2) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid port number: %d", args[3].u_int));
        }
        else {
            port = args[3].u_int;
        }

        // get the baudrate
        uint32_t baudrate;
        if (args[4].u_int <= 0) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid baud rate"));
        } else {
            baudrate = args[4].u_int;
        }

        // parity
        uint32_t parity;
        if (args[5].u_obj == mp_const_none) {
            parity = UART_PARITY_DISABLE;
        } else {
            parity = mp_obj_get_int(args[5].u_obj);
            if (parity != UART_PARITY_ODD && parity != UART_PARITY_EVEN) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid parity %d", parity));
            }
        }

        //assign the pins
        mp_obj_t pins_o = args[6].u_obj;
        if (pins_o != mp_const_none) {
            mp_obj_t *pins;
            mp_uint_t n_pins = 3;
            if (pins_o == MP_OBJ_NULL) {
                // use the default pins
                pins = (mp_obj_t *)mach_modbus_def_pin[0];
            } else {
                mp_obj_get_array_fixed_n(pins_o, n_pins, &pins);
                if (n_pins != 3) {
                    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid the number of pins %d", n_pins));
                }
                if (n_pins == 3) {
                    if (pins[0] == mp_const_none || pins[1] == mp_const_none || pins[2] == mp_const_none) {
                        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid pin selection: %d %d %d", pin_find(pins[0])->pin_number, pin_find(pins[1])->pin_number, pin_find(pins[2])->pin_number));
                    }
                }
            }
            self->tx = pin_find(pins[0])->pin_number;
            self->rx = pin_find(pins[1])->pin_number;
            self->rts = pin_find(pins[2])->pin_number;
        }

        self->base.type = &mach_modbus_type;
        self->comm_info.mode = mode;
        self->comm_info.slave_addr = slave_addr;
        self->comm_info.port = port;
        self->comm_info.baudrate = baudrate;
        self->comm_info.parity = parity;
    }

    else if (mode == MB_MODE_TCP) {
        self->base.type = &mach_modbus_type;
        self->comm_info.ip_port = MB_TCP_PORT_NUMBER;
        self->comm_info.ip_addr_type = MB_IPV4;
        self->comm_info.ip_mode = MB_MODE_TCP;

        // uint8_t multiple_flag = args[11].u_int;

        mp_obj_t ip_addr_input_m = args[11].u_obj;
        if (ip_addr_input_m != mp_const_none) {
            // mp_obj_t *ips;
            // mp_uint_t n_pins = 4;
            if (ip_addr_input_m == MP_OBJ_NULL) {
                self->comm_info.ip_addr = NULL;
            }
            else {
                // if (multiple_flag>0) {
                //     char* input[4] = {NULL,NULL,NULL,NULL};
                //     int len[4] = {0,0,0,0};
                //     mp_obj_get_array(ip_addr_input_m, &n_pins, &ips);
                //     for (int ipn = 0; ipn < n_pins; ipn++) { 
                //         input[ipn] = (char *)mp_obj_str_get_str(ips[ipn]);
                //         len[ipn] = strlen(input[ipn]);
                //         slave_ip_address_table[ipn] = calloc(len[ipn], sizeof(char));
                //         strcpy(slave_ip_address_table[ipn], input[ipn]);
                //     }

                //     self->comm_info.ip_addr = (void*)slave_ip_address_table;
                // }
                // else {
                    char* input = (char *)mp_obj_str_get_str(ip_addr_input_m);
                    int len = strlen(input);
                    for(uint8_t j = 0; j < 2-1; j++) { 
                        slave_ip_address_table[j] = calloc(len, sizeof(char));
                    }
                    // Double check that last table entry is still NULL !!
                    strcpy(slave_ip_address_table[0], input);
                    printf("slave address ip = %s\n", slave_ip_address_table[0]);

                    self->comm_info.ip_addr = (void*)slave_ip_address_table;
                // }
            }

            }

        if (!wlan_obj.started) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_AttributeError, "Wifi hasn't started!"));
        }
        if(wlan_obj.esp_netif_STA == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_AttributeError, "The STA interface has not been initialized!"));
        }
        self->comm_info.ip_netif_ptr = (void*)wlan_obj.esp_netif_STA;
    }

    if(role==SLAVE) {
        holding_registers_length = args[7].u_int;
        input_registers_length = args[8].u_int;
        coils_length = args[9].u_int;
        discrete_inputs_length = args[10].u_int;
    }
    
    if (role == SLAVE) {
        if (mode == MB_MODE_RTU || mode == MB_MODE_ASCII) {
            // printf("mach_modbus_serial_slave_init\n");
            mach_modbus_serial_slave_init((const mach_modbus_obj_t *)self);
        }
        else if (mode == MB_MODE_TCP) {
            // printf("mach_modbus_tcp_slave_init\n");
            mach_modbus_tcp_slave_init((const mach_modbus_obj_t *)self);
        }
    }
    else if (role== MASTER) {
        if (mode == MB_MODE_RTU || mode == MB_MODE_ASCII) {
            // printf("mach_modbus_serial_master_init\n");
            mach_modbus_serial_master_init((const mach_modbus_obj_t *)self);
        }
        else if (mode == MB_MODE_TCP) {
            // printf("mach_modbus_tcp_master_init\n");
            mach_modbus_tcp_master_init((const mach_modbus_obj_t *)self);
        }
    }

    // Init Done
    self->init = true;

    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
}

STATIC const mp_arg_t mach_modbus_init_args[] = {
    { MP_QSTR_id,                              MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_role,           MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = SLAVE} },
    { MP_QSTR_mode,           MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = MB_MODE_RTU} },
    { MP_QSTR_slave_addr,     MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = MB_DEV_ADDR} },
    { MP_QSTR_port,           MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = MB_PORT_NUM} },
    { MP_QSTR_baudrate,       MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = MB_DEV_SPEED} },
    { MP_QSTR_parity,         MP_ARG_KW_ONLY | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_pins,           MP_ARG_KW_ONLY | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_holdingRegs,    MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_inputRegs,      MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_coils,          MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_discreteInputs, MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
    // { MP_QSTR_multiple_slave, MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = 0} },
    { MP_QSTR_slave_ip_addr,  MP_ARG_KW_ONLY | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
};
STATIC mp_obj_t mach_modbus_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_modbus_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_modbus_init_args, args);

    uint32_t bus_id = args[0].u_int;

    // get the correct uart instance
    mach_modbus_obj_t *self = (mach_modbus_obj_t *)&mach_modbus_obj; //unless there is bus_id
    self->base.type = &mach_modbus_type;
    self->bus_id = bus_id;

    // start the peripheral
    mach_modbus_init_helper(self, &args[1]);

    return (mp_obj_t)self;
}

STATIC mp_obj_t mach_modbus_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_modbus_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_modbus_init_args[1], args);
    return mach_modbus_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_modbus_init_obj, 1, mach_modbus_init);

STATIC mp_obj_t mach_modbus_deinit(mp_obj_t self_in) {
    mach_modbus_obj_t *self = self_in;

    if (self->comm_info.baudrate > 0) {
        // invalidate the baudrate
        self->comm_info.baudrate = 0;
        // detach the pins
        uart_driver_delete(MB_PORT_NUM);
        if (role == SLAVE) {
            mbc_slave_destroy();
        }
        else if (role == MASTER) {
            mbc_master_destroy();
        }
    }

    else if  (self->comm_info.mode == MB_MODE_TCP) {
        if (role == SLAVE) {
            mbc_slave_destroy();
        }
        else if (role == MASTER) {
            mbc_master_destroy();
        }
    }

    self->init = false;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_modbus_deinit_obj, mach_modbus_deinit);

////SLAVE
//INPUT REGISTERS
STATIC mp_obj_t modbus_readInputReg(mp_obj_t self_in, mp_obj_t reg) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= input_registers_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid input register index  %d", index));
    } 
    // printf("Input Register[%d] = 0x%X\n", index, holding_registers[index]);
    return MP_OBJ_NEW_SMALL_INT(input_registers[index]);
}
MP_DEFINE_CONST_FUN_OBJ_2(modbus_readInputReg_obj, modbus_readInputReg);

STATIC mp_obj_t modbus_writeInputReg(mp_obj_t self_in, mp_obj_t reg, mp_obj_t value) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= input_registers_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid input register index  %d", index));
    }
    portENTER_CRITICAL(&param_lock);
    input_registers[index] = (uint16_t)(mp_obj_get_int(value));
    portEXIT_CRITICAL(&param_lock);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(modbus_writeInputReg_obj, modbus_writeInputReg);

//HOLDING REGISTERS
STATIC mp_obj_t modbus_readHoldingReg(mp_obj_t self_in, mp_obj_t reg) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= holding_registers_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid holding register index  %d", index));
    }
    return MP_OBJ_NEW_SMALL_INT(holding_registers[index]);
}
MP_DEFINE_CONST_FUN_OBJ_2(modbus_readHoldingReg_obj, modbus_readHoldingReg);

STATIC mp_obj_t modbus_writeHoldingReg(mp_obj_t self_in, mp_obj_t reg, mp_obj_t value) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= holding_registers_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid holding register index  %d", index));
    }
    portENTER_CRITICAL(&param_lock);
    holding_registers[index] = (uint16_t)(mp_obj_get_int(value));
    portEXIT_CRITICAL(&param_lock);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(modbus_writeHoldingReg_obj, modbus_writeHoldingReg);

//COILS
STATIC mp_obj_t modbus_readCoil(mp_obj_t self_in, mp_obj_t reg) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= coils_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid coil index %d", index));
    }
    uint8_t coilPort = index/8;
    uint8_t shift = index%8;

    return MP_OBJ_NEW_SMALL_INT((coils[coilPort] >> shift) & 0x01);
}
MP_DEFINE_CONST_FUN_OBJ_2(modbus_readCoil_obj, modbus_readCoil);

STATIC mp_obj_t modbus_writeCoil(mp_obj_t self_in, mp_obj_t reg, mp_obj_t value) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= coils_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid coil index %d", index));
    }
    uint8_t newValue = mp_obj_get_int(value);
    if (newValue>1 || newValue<0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid coil value %d", newValue));
    }

    uint8_t coilPort = index/8;
    uint8_t shift = index%8;
    // printf("coils[%d] = %X\n", coilPort, coils[coilPort]);

    portENTER_CRITICAL(&param_lock);
    coils[coilPort] = (uint8_t)((coils[coilPort] & ~(1 << shift)) | 
                      ((newValue << shift) & 0xFF));
    portEXIT_CRITICAL(&param_lock);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(modbus_writeCoil_obj, modbus_writeCoil);

//DISCRETE INPUTS
STATIC mp_obj_t modbus_readDiscreteInput(mp_obj_t self_in, mp_obj_t reg) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= discrete_inputs_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid discrete input index  %d", index));
    }
    uint8_t dinputPort = index/8;
    uint8_t shift = index%8;

    return MP_OBJ_NEW_SMALL_INT((discrete_inputs[dinputPort] >> shift) & 0x01);
}
MP_DEFINE_CONST_FUN_OBJ_2(modbus_readDiscreteInput_obj, modbus_readDiscreteInput);

STATIC mp_obj_t modbus_writeDiscreteInput(mp_obj_t self_in, mp_obj_t reg, mp_obj_t value) {
    mach_modbus_obj_t *self = self_in;
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    uint32_t index = mp_obj_get_int(reg);
    if (role != SLAVE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    if (index >= discrete_inputs_length) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid discrete input index  %d", index));
    }
    uint8_t newValue = mp_obj_get_int(value);
    if (newValue>1 || newValue<0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid discrete input value %d", newValue));
    }

    uint8_t dinputPort = index/8;
    uint8_t shift = index%8;
    // printf("discrete_inputs[%d] = %X\n", dinputPort, discrete_inputs[dinputPort]);
    portENTER_CRITICAL(&param_lock);
    discrete_inputs[dinputPort] = (uint8_t)((discrete_inputs[dinputPort] & ~(1 << shift)) | 
                                  ((newValue << shift) & 0xFF));
    portEXIT_CRITICAL(&param_lock);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(modbus_writeDiscreteInput_obj, modbus_writeDiscreteInput);

////MASTER FUNCTIONS
STATIC mp_obj_t modbus_sendRequest(mp_uint_t n_args, const mp_obj_t *args) {
    mach_modbus_obj_t *self = args[0];
    if (!self->init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    
    //slaveID, command name, starting reg, quantity or data
    if (role != MASTER) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }

    esp_err_t error = ESP_OK;

    setparam.slave_addr = mp_obj_get_int(args[1]);
    if (self->comm_info.mode == MB_MODE_TCP) {
        setparam.slave_addr = 1;
    }
    printf("slave = %d\n", setparam.slave_addr);
    setparam.command = mp_obj_get_int(args[2]);
    printf("command = %d\n", setparam.command);
    setparam.reg_start = mp_obj_get_int(args[3]);
    printf("reg_start = %d\n", setparam.reg_start);

    if (setparam.command == MB_FUNC_READ_COILS || setparam.command == MB_FUNC_READ_DISCRETE_INPUTS) {
        //slaveaddr, cmd, reg start, quantity
        uint32_t coil_discrete_length = 0;
        uint8_t *coil_discrete = NULL;
        uint8_t portSize = 0;
        uint8_t requestedLen = mp_obj_get_int(args[4])+setparam.reg_start;
        portSize = sizeof(uint8_t)*(requestedLen/8);
        if (requestedLen%8) {
            portSize+=1;
        }
        setparam.reg_size = requestedLen;
        coil_discrete_length = requestedLen;
        coil_discrete = malloc(portSize);
        for (int i=0; i<portSize; i++) {
            coil_discrete[i] = 0;
        }
        error = mbc_master_send_request(&setparam, (void*)coil_discrete);
        // for (int i=0; i<portSize; i++) {
        //     printf("coil_discrete[%d] = %X\n", i,coil_discrete[i]);
        // }
        if (error != ESP_OK) {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid coil index  0x%X", error));
        } 
        else {
            //MULTIPLE COILS FORMATION
            uint16_t pos=0;
            uint16_t portnum=0;
            uint16_t port = setparam.reg_start/8;
            uint16_t value=0;
            mp_obj_t list = mp_obj_new_list(0, NULL);
            if ((coil_discrete_length)>8) {
                //initial to 8
                if (setparam.reg_start%8) {
                    for (pos=setparam.reg_start; pos<(8-(setparam.reg_start%8)+setparam.reg_start); pos++) {
                        value = (coil_discrete[port]>>(pos%8))&(0x01);
                        mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(value));
                        // printf("coil_discrete%d[%d]= %d\n", port, (pos%8), value);
                    }
                    port+=1;
                }
                //full port
                for (portnum = port; portnum<(coil_discrete_length/8); portnum++) {
                    for (pos=(portnum)*8; pos<(portnum+1)*8; pos++) {
                        value = (coil_discrete[portnum]>>(pos%8))&(0x01);
                        mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(value));
                        // printf("coil_discrete%d[%d]= %d\n", portnum, (pos%8), value);
                    }
                    if (coil_discrete_length-pos>=0) {
                        port+=1;
                    }
                }
                //reamining part to 8th multiple
                if (coil_discrete_length%8) {
                    for (pos = pos; pos<coil_discrete_length; pos++) {
                        value = (coil_discrete[port]>>(pos%8))&(0x01);
                        mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(value));
                        // printf("coil_discrete%d[%d]= %d\n", port, (pos%8), value);
                    }
                }
            }
            else {
                //coils to be read is less than 8th multiple
                for (pos=setparam.reg_start; pos<coil_discrete_length; pos++) {
                        value = (coil_discrete[port]>>(pos%8))&(0x01);
                        mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(value));
                        // printf("coil_discrete%d[%d]= %d\n", port, (pos%8), value);
                    }
            }
            free(coil_discrete);
            return list;
        }
    }

    else if (setparam.command == MB_FUNC_WRITE_SINGLE_COIL || setparam.command == MB_FUNC_WRITE_REGISTER) {
        //slaveaddr, cmd, reg start, data
        if (setparam.command == MB_FUNC_WRITE_SINGLE_COIL) {
            uint16_t *write_reg = NULL;
            write_reg = malloc(sizeof(uint16_t));
            setparam.reg_size = 1;
            uint16_t level = (uint16_t)(mp_obj_get_int(args[4]));
            if (level>0) {
                write_reg[0] = 0xFF00;
            } else {
                write_reg[0] = 0x0000;
            }
            error = mbc_master_send_request(&setparam, (void*)write_reg);
            free(write_reg);
            if (error != ESP_OK) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "error: 0x%X", error));
            }
        }
        else if (setparam.command == MB_FUNC_WRITE_REGISTER) {
            uint16_t *write_reg = NULL;
            write_reg = malloc(sizeof(uint16_t));
            setparam.reg_size = 1;
            if (mp_obj_get_int(args[4]) > 0xFFFF) {
                    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "value's length is not 16bits"));
            }
            else {
                write_reg[0] = (uint16_t)(mp_obj_get_int(args[4]));
            }
            error = mbc_master_send_request(&setparam, (void*)write_reg);
            free(write_reg);
            if (error != ESP_OK) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "error: 0x%X", error));
            }
        }
    }

    else if (setparam.command == MB_FUNC_WRITE_MULTIPLE_COILS || setparam.command == MB_FUNC_WRITE_MULTIPLE_REGISTERS) {
        //slaveaddr, cmd, reg start, data list
        if (setparam.command == MB_FUNC_WRITE_MULTIPLE_COILS) {
            mp_obj_t *items;
            uint len;
            mp_obj_get_array(args[4], &len, &items);
            for (int i = 0; i < len; i++) {
                if ((mp_obj_get_int(items[i]) > 1) || (mp_obj_get_int(items[i]) < 0)) {
                    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "%d.value is not bool", i));
                }
            }
            uint8_t coilPort = len/8;
            uint8_t shift = len%8;
            uint8_t rel_shift = 0;
            uint8_t *write_reg = NULL;
            uint8_t remaining = 0;
            uint8_t ith = 0;
            if (shift) {
                coilPort++;
            }
            setparam.reg_size = len;
            write_reg = malloc(sizeof(uint8_t)*coilPort);
            for (int port = 0; port<coilPort; port++) { // clear the values of temporary registers in mcu.
                write_reg[port] = 0;
            }
            if ((len > 8)) {
                remaining = 8;
            }
            else if ((len) <= 8) {
                remaining = len;
            }
            for (int port = 0; port < coilPort; port++) {
                for (rel_shift = 0; rel_shift < remaining; rel_shift++) {
                        write_reg[port] = (write_reg[port] & ~(1 << rel_shift)) | //clear certain bit
                                        (((mp_obj_get_int(items[ith]) & 0x01) << rel_shift) & 0xFF); // add new state of bit
                        ith++; //only item number is changed from 0 to len
                }
                remaining = len - ((port+1)*8); // remaining bits to be written
                if (remaining > 8) { // remaining will be same if it is less than 8
                    remaining = 8;
                }
            }

            error = mbc_master_send_request(&setparam, (void*)write_reg);
            free(write_reg);
            if (error != ESP_OK) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "error: 0x%X", error));
            }
        }
        else if (setparam.command == MB_FUNC_WRITE_MULTIPLE_REGISTERS) {
            mp_obj_t *items;
            uint len;
            mp_obj_get_array(args[4], &len, &items);
            for (int i = 0; i < len; i++) {
                if (mp_obj_get_int(items[i]) > 0xFFFF) {
                    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "The length of %d.value is not 16bits", i));
                }
            }
            uint16_t *write_reg = NULL;
            setparam.reg_size = len;
            write_reg = malloc(sizeof(uint16_t)*setparam.reg_size);
            for (int i = 0; i < len; i++) {
                write_reg[i] = mp_obj_get_int(items[i]);
            }
            error = mbc_master_send_request(&setparam, (void*)write_reg);
            free(write_reg);
            if (error != ESP_OK) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "error: 0x%X", error));
            }
        }
    }

    else if (setparam.command == MB_FUNC_READ_HOLDING_REGISTER || setparam.command == MB_FUNC_READ_INPUT_REGISTER) {
        //slaveaddr, cmd, reg start, quantity
        uint32_t holding_input_length = 0;
        uint16_t *holding_input = NULL;
        holding_input_length = mp_obj_get_int(args[4]);
        // printf("reg_size = %d\n", holding_input_length);
        holding_input = malloc(sizeof(uint16_t)*holding_input_length);
        for (int i=0; i<holding_input_length; i++) {
            holding_input[i] = 0;
        }
        setparam.reg_size = holding_input_length;
        error = mbc_master_send_request(&setparam, (void*)holding_input);
        // for (int i=0; i<setparam.reg_size; i++) {
        //     printf("holding_input[%d] = %X\n", i,holding_input[i]);
        // }
        if (error != ESP_OK) {
            free(holding_input);
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "error: 0x%X", error));
        } 
        else {
            mp_obj_t list = mp_obj_new_list(0, NULL);
            for (int i = 0; i<holding_input_length; i++) {
                mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(holding_input[i]));
            }
            free(holding_input);
            return list;
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(modbus_sendRequest_obj, 4, 7, modbus_sendRequest);


STATIC const mp_map_elem_t mach_modbus_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                    (mp_obj_t)&mach_modbus_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                  (mp_obj_t)&mach_modbus_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readInputReg),            (mp_obj_t)&modbus_readInputReg_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_writeInputReg),           (mp_obj_t)&modbus_writeInputReg_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readHoldingReg),          (mp_obj_t)&modbus_readHoldingReg_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_writeHoldingReg),         (mp_obj_t)&modbus_writeHoldingReg_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readCoil),                (mp_obj_t)&modbus_readCoil_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_writeCoil),               (mp_obj_t)&modbus_writeCoil_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_readDiscreteInput),       (mp_obj_t)&modbus_readDiscreteInput_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_writeDiscreteInput),      (mp_obj_t)&modbus_writeDiscreteInput_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_sendRequest),          (mp_obj_t)&modbus_sendRequest_obj},

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVEN),                MP_OBJ_NEW_SMALL_INT(UART_PARITY_EVEN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ODD),                 MP_OBJ_NEW_SMALL_INT(UART_PARITY_ODD) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RTU),                 MP_OBJ_NEW_SMALL_INT(MB_MODE_RTU) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ASCII),               MP_OBJ_NEW_SMALL_INT(MB_MODE_ASCII) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_TCP),                 MP_OBJ_NEW_SMALL_INT(MB_MODE_TCP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MASTER),              MP_OBJ_NEW_SMALL_INT(MASTER) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SLAVE),               MP_OBJ_NEW_SMALL_INT(SLAVE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IPV4),                 MP_OBJ_NEW_SMALL_INT(MB_IPV4) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IPV6),                 MP_OBJ_NEW_SMALL_INT(MB_IPV6) },
};
STATIC MP_DEFINE_CONST_DICT(mach_modbus_locals_dict, mach_modbus_locals_dict_table);

// Define module object.
const mp_obj_type_t mach_modbus_type = {
    { &mp_type_type },
    .name = MP_QSTR_MODBUS,
    .print = mach_modbus_print,
    .make_new = mach_modbus_make_new,
    .locals_dict = (mp_obj_t)&mach_modbus_locals_dict,
};

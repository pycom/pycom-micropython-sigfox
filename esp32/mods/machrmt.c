/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "py/mpstate.h"
#include "py/runtime.h"
#include "py/obj.h"
#include "py/objtuple.h"
#include "esp_err.h"
#include "machpin.h"
#include "rmt.h"
#include "machrmt.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

/* RMT clock is 80 MHz, clock division is stored in a 8 bit register, max value can be 255 */
#define RMT_RESOLUTION_100NS   ((uint8_t)8)    /* Maximum measured pulse-width: ~3.2768 ms */
#define RMT_RESOLUTION_1000NS  ((uint8_t)80)   /* Maximum measured pulse-width: ~32.768 ms */
#define RMT_RESOLUTION_3125NS  ((uint8_t)250)  /* Maximum measured pulse-width: ~102.4  ms */

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
struct _mach_rmt_obj_t {
    mp_obj_base_t base;
    rmt_config_t config;
    bool is_used;
};

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

static mach_rmt_obj_t mach_rmt_obj[RMT_CHANNEL_MAX] = {
    { .config = {.channel = 0, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_1000NS}, .is_used = true },  /* Channel 0 is used for RGB LED user control */
    { .config = {.channel = 1, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_100NS},  .is_used = true },  /* Channel 1 is used for RGB Led Heartbeat signal */
    { .config = {.channel = 2, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_100NS},  .is_used = false },
    { .config = {.channel = 3, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_100NS},  .is_used = false },
    { .config = {.channel = 4, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_1000NS}, .is_used = false },
    { .config = {.channel = 5, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_1000NS}, .is_used = false },
    { .config = {.channel = 6, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_3125NS}, .is_used = false },
    { .config = {.channel = 7, .mem_block_num = 1, .clk_div = RMT_RESOLUTION_3125NS}, .is_used = false }
};


STATIC mp_obj_t mach_rmt_init_helper(mach_rmt_obj_t *self, const mp_arg_val_t *args) {

    if(args[0].u_obj == mp_const_none) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "GPIO must be defined!"));  
    }

    gpio_num_t gpio = pin_find(args[0].u_obj)->pin_number;

    for(int i = RMT_CHANNEL_2; i < RMT_CHANNEL_MAX; i++) {
        if(mach_rmt_obj[i].is_used == true) {
            if(self != &mach_rmt_obj[i]) {
                if(mach_rmt_obj[i].config.gpio_num == gpio) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "The given Pin is already used by other RMT channel!"));
                }
            }
        }
    }

    /* After it is checked that the given GPIO is correct uninstall the driver if needed */
    if(self->is_used == true) {
        /* Deregister the previously registered GPIO */
        gpio_matrix_out(mach_rmt_obj[self->config.channel].config.gpio_num, SIG_GPIO_OUT_IDX, 0, 0);
        rmt_driver_uninstall(self->config.channel);
        self->is_used = false;
    }

    mach_rmt_obj[self->config.channel].config.gpio_num = gpio;

    if((args[1].u_obj == mp_const_none) && (args[3].u_obj == mp_const_none)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Either RX or TX configuration must be given!"));
    }
    else if((args[1].u_obj != mp_const_none) && (args[3].u_obj != mp_const_none)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "A channel can be configured only for RX or TX at the same time!"));
    }
    /* RX configuration */
    else if(args[1].u_obj != mp_const_none) {
        self->config.rmt_mode = RMT_MODE_RX;
        self->config.rx_config.idle_threshold = mp_obj_get_int(args[1].u_obj); /* Resolution: according to the channel's resolution */

        if(args[2].u_obj == MP_OBJ_NULL) {
            self->config.rx_config.filter_en = false;
            self->config.rx_config.filter_ticks_thresh = 0;
        }
        else {
            self->config.rx_config.filter_en = true;
            /* The register to store filter tick threshold is 8 bit long, counted in APB clock period, maximum value can be 255.
             * 0-31 is accepted by the user because 32x8 is 256 which is not accepted */
            if(mp_obj_get_int(args[2].u_obj) <= 31) { 
                self->config.rx_config.filter_ticks_thresh = mp_obj_get_int(args[2].u_obj) * 8; 
            }
            else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Filter threshold is accepted between values: 0-31, counted 100 nano sec!"));
            }
        }
    }
    /* TX configuration */
    else if(args[3].u_obj != mp_const_none) {
        self->config.rmt_mode = RMT_MODE_TX;
        self->config.tx_config.idle_level = mp_obj_get_int(args[3].u_obj);
        self->config.tx_config.idle_output_en = true;
        self->config.tx_config.loop_en = false;

        if(args[4].u_obj == MP_OBJ_NULL) {
            self->config.tx_config.carrier_en = false;
            self->config.tx_config.carrier_freq_hz = 0;
            self->config.tx_config.carrier_duty_percent = 0;
            self->config.tx_config.carrier_level = RMT_CARRIER_LEVEL_MAX;
        }
        else {
            self->config.tx_config.carrier_en = true;
            mp_obj_t* carrier_cfg;
            mp_obj_get_array_fixed_n(args[4].u_obj, 3, &carrier_cfg);
            self->config.tx_config.carrier_freq_hz = mp_obj_get_int(carrier_cfg[0]);

            mp_int_t duty_percent = mp_obj_get_int(carrier_cfg[1]);
            if(duty_percent > 100 || duty_percent < 0) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Duty_percent argument is invalid!"));
            }
            else {
                self->config.tx_config.carrier_duty_percent = duty_percent;
            }

            rmt_carrier_level_t carrier_level = mp_obj_get_int(carrier_cfg[2]);
            if(carrier_level >= RMT_CARRIER_LEVEL_LOW && carrier_level < RMT_CARRIER_LEVEL_MAX){
                self->config.tx_config.carrier_level = carrier_level;
            }
            else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Carrier level argument is invalid!"));
            }
        }
    }

    esp_err_t retval = rmt_config(&(self->config));
    if(retval != ESP_OK) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "RMT parameter error!"));
    }

    if(self->config.rmt_mode == RMT_MODE_TX) {
        rmt_driver_install(self->config.channel,0,0);
    }
    else {
        /* 2 pulses are stored in 1 rmt_item32_t
         * Because RMT memory block / channel is 1, maximum of 128 pulses can be received 
         * 130*sizeof(rmt_item32_t) + sizeof(size_t) + sizeof(int) of Ringbuffer space is needed,
         * where sizeof(size_t) + sizeof(int) is the header/administration cost of the Ringbuffer.
         * 128*sizeof(rmt_item32_t) + sizeof(size_t) + sizeof(int) is not enough, it does not allow to receive
         * 128 pulses after each other due to a bug/behavior in Ringbuffer's implementation
         **/
        retval = rmt_driver_install(self->config.channel, 130*sizeof(rmt_item32_t) + sizeof(size_t) + sizeof(int), 0);
        if(retval != ESP_OK) {
            if(retval == ESP_ERR_NO_MEM) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Not enough memory to initialize RMT driver!"));
            }
            else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Error during RMT driver install"));
            }
        }
    }

    self->is_used = true;
    return mp_const_none;
}

STATIC const mp_arg_t mach_rmt_init_args[] = {
        { MP_QSTR_channel,                  MP_ARG_KW_ONLY | MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_gpio,                     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
        { MP_QSTR_rx_idle_threshold,        MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
        { MP_QSTR_rx_filter_threshold,      MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        { MP_QSTR_tx_idle_level,            MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none}},
        { MP_QSTR_tx_carrier,               MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
};

STATIC mp_obj_t mach_rmt_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {

    /* Validate the channel argument only */
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, 1, all_args + n_args);
    mp_arg_val_t args[1];
    mp_arg_parse_all(0, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_rmt_init_args, args);

    mach_rmt_obj_t *self;

    mp_int_t channel = args[0].u_int;
    /* Do not allow to use Channel0 and Channel1 freely, they are used for other purposes */
    /* Do not allow to use any non existing channels */
    if(channel <= RMT_CHANNEL_1 || channel >= RMT_CHANNEL_MAX) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Channel is invalid!"));
    }
    
    self = &(mach_rmt_obj[channel]);
    self->base.type = &mach_rmt_type;

    /* Initialize the driver only if the init parameters are given */
    if(n_kw > 1) {
        /* Parse all arguments against the schema */
        mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
        mp_arg_val_t args_init[MP_ARRAY_SIZE(mach_rmt_init_args)];
        mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args_init), mach_rmt_init_args, args_init);

        mach_rmt_init_helper(self, &args_init[1]);
    }

    return self;
}

STATIC mp_obj_t mach_rmt_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    /* Parse the arguments against the schema skipping the "channel" argument */
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_rmt_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_rmt_init_args[1], args);

    return mach_rmt_init_helper(pos_args[0], args);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_rmt_init_obj, 1, mach_rmt_init);

STATIC mp_obj_t mach_rmt_deinit(mp_obj_t self_in) {

    mach_rmt_obj_t *self = self_in;

    if(self->is_used == true){
        gpio_matrix_out(mach_rmt_obj[self->config.channel].config.gpio_num, SIG_GPIO_OUT_IDX, 0, 0);
        rmt_driver_uninstall(self->config.channel);
        self->is_used = false;
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mach_rmt_deinit_obj, mach_rmt_deinit);

STATIC mp_obj_t mach_rmt_pulses_send(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mach_rmt_pulses_send_args[] = {
        { MP_QSTR_id,                     MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_duration,               MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_data,                   MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_start_level,            MP_ARG_OBJ | MP_ARG_KW_ONLY,  {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_wait_tx_done,           MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_obj = mp_const_true} }
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_rmt_pulses_send_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(mach_rmt_pulses_send_args), mach_rmt_pulses_send_args, args);

    mach_rmt_obj_t *self = args[0].u_obj;

    if(self->is_used == false){
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "RMT channel is not initialized!"));
    }

    if(self->config.rmt_mode != RMT_MODE_TX) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "RMT channel is configured for RX!"));
    }

    mp_uint_t start_level = 0;
    mp_uint_t data_length = 0;
    bool start_level_needed = false;
    mp_uint_t duration_length = 0;
    mp_int_t duration = 0;
    mp_obj_t* data_ptr = NULL;
    mp_obj_t* duration_ptr = NULL;
    bool wait_tx_done = args[4].u_bool;

    /* Get the "duration" mandatory parameter */
    if(MP_OBJ_IS_SMALL_INT(args[1].u_obj) == true) {
        /* Duration is given as a single number */
        duration_length = 1;
        duration = mp_obj_get_int(args[1].u_obj);
    }
    else {
        if(MP_OBJ_IS_TYPE(args[1].u_obj, &mp_type_tuple) == true) {
            /* Duration is given as a tuple */
            mp_obj_tuple_get(args[1].u_obj, &duration_length, &duration_ptr);
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "\"Duration\" can be a single integer or a tuple!"));
        }
    }

    if(args[2].u_obj == MP_OBJ_NULL)
    {
        /* If duration is not a tuple */
        if(duration_length == 1)
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "\"Duration\" must be a tuple if \"data\" parameter left empty!"));
        }
        else
        {
            /* If the "data" was not given use the length of the duration if it is a tuple*/
            data_length = duration_length;
            start_level_needed = true;
        }
    }
    else if(MP_OBJ_IS_TYPE(args[2].u_obj, &mp_type_tuple) == true) {
        /* In this case the data parameter is a tuple containing the pulses to be sent out */
        mp_obj_tuple_get(args[2].u_obj, &data_length, &data_ptr);

        for(mp_uint_t i = 0; i < data_length; i++) {
            if( mp_obj_get_int(data_ptr[i]) > 1){
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Only 0 or 1 can be given if the \"data\" is provided as a tuple!"));
            }
        }
    }
    else if(MP_OBJ_IS_INT(args[2].u_obj) == true) {
        /* In this case the data parameter is interpreted as a number indicating how many pulses will be sent out */
        data_length = mp_obj_get_int(args[2].u_obj);
        start_level_needed = true;
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "\"data\" parameter must be a single integer, a tuple or left empty!"));
    }

    /* If both duration and data are given as a tuple, their length must be equal */
    if((duration_length != 1) && (duration_length != data_length)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Number of the given \"duration\" and \"data\" values must be equal!"));
    }

    if(start_level_needed == true)
    {
        /* Get the start_level as the "data" is not given, or given as a single number indicating the length */
        if(args[3].u_obj == MP_OBJ_NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "\"start_level\" parameter must be given!"));
        }
        else if(MP_OBJ_IS_INT(args[3].u_obj))
        {
            start_level = mp_obj_get_int(args[3].u_obj);
            if(start_level > 1) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "\"start_level\" can be 0 or 1"));
            }

            mp_obj_tuple_t* tmp_tuple = mp_obj_new_tuple(data_length, NULL);
            data_ptr = tmp_tuple->items;

            /* Compose a list of 0/1 up to number of "data" parameter and starting value of "start_level" parameter */
            for(mp_uint_t i = 0; i < data_length; i++) {
                if(start_level == 0) {
                    data_ptr[i] = mp_obj_new_int(0);
                    start_level = 1;
                }
                else {
                    data_ptr[i] = mp_obj_new_int(1);
                    start_level = 0;
                }
            }
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "\"start_level\" can be 0 or 1"));
        }
    }

    /* An rmt_item32_t can contain 2 bits, calculate the number of the necessary objects needed to store the input data */
    mp_uint_t items_to_send_count = (data_length / 2) + (data_length % 2);
    rmt_item32_t* items_to_send = (rmt_item32_t*)malloc(items_to_send_count * sizeof(rmt_item32_t));
    for(mp_uint_t i = 0, j = 0; i < items_to_send_count; i++, j++) {

        items_to_send[i].level0 = mp_obj_get_int(data_ptr[j]);
        if(duration_length == 1) {
            items_to_send[i].duration0 = duration;
        }
        else {
            items_to_send[i].duration0 = mp_obj_get_int(duration_ptr[j]);
        }

        /* Check whether the last rmt_item32_t's "level1" field is needed */
        if(++j == data_length) {
            items_to_send[i].level1 = 0;
            items_to_send[i].duration1 = 0;
        }
        else {
            items_to_send[i].level1 = mp_obj_get_int(data_ptr[j]);
            if(duration_length == 1) {
                items_to_send[i].duration1 = duration;
            }
            else {
                items_to_send[i].duration1 = mp_obj_get_int(duration_ptr[j]);
            }
        }
    }

    MP_THREAD_GIL_EXIT();
    esp_err_t retval = rmt_write_items(self->config.channel, items_to_send, items_to_send_count, wait_tx_done);
    MP_THREAD_GIL_ENTER();

    free(items_to_send);

    if (retval != ESP_OK) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Could not send data!"));
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mach_rmt_pulses_send_obj, 1, mach_rmt_pulses_send);


STATIC mp_obj_t mach_rmt_pulses_get(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mach_rmt_pulses_get_args[] = {
        { MP_QSTR_id,                     MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_pulses,                 MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_timeout,                MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_rmt_pulses_get_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(mach_rmt_pulses_get_args), mach_rmt_pulses_get_args, args);

    mach_rmt_obj_t *self = args[0].u_obj;

    if(self->is_used == false){
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "RMT channel is not initialized!"));
    }

    if(self->config.rmt_mode != RMT_MODE_RX) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "RMT channel is configured for TX!"));
    }

    mp_uint_t pulses;
    TickType_t timeout;

    if(args[1].u_obj == MP_OBJ_NULL){
        pulses = 1; /* If not specified set it to one so the pulses will be fetched according to the configured timeout */
    }
    else
    {
        if(MP_OBJ_IS_SMALL_INT(args[1].u_obj) == true) {
            pulses = mp_obj_get_int(args[1].u_obj);
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "If pulses is specified it must be a valid integer number"));
        }
    }

    if(args[2].u_obj == MP_OBJ_NULL){
        timeout = portMAX_DELAY;
    }
    else
    {
        if(MP_OBJ_IS_SMALL_INT(args[2].u_obj) == true) {
            timeout = mp_obj_get_int(args[2].u_obj);
        }
        else
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "If timeout is specified it must be a valid integer number"));
        }
    }

    RingbufHandle_t ringbuf = NULL;
    mp_uint_t total_received = 0;
    mp_uint_t currently_received = 0;
    /* An rmt_item holds 2 bit*/
    mp_uint_t number_of_rmt_item_to_receive =  pulses / 2 + (pulses % 2);
    mp_obj_t* ret_items = mp_obj_new_list(0, NULL);

    rmt_wait_tx_done(self->config.channel, portMAX_DELAY);
    rmt_get_ringbuf_handle(self->config.channel, &ringbuf);
    rmt_rx_start(self->config.channel, true);

    /* Wait until required number of pulses received */
    while(total_received < number_of_rmt_item_to_receive) {

        MP_THREAD_GIL_EXIT();
        /* Wait until any data is received */
        rmt_item32_t* items = (rmt_item32_t*)xRingbufferReceive(ringbuf, &currently_received, timeout);
        MP_THREAD_GIL_ENTER();
        if(items != NULL) {
            /* An rmt_item32_t is 4 byte, xRingbufferReceive returns the length in bytes */
            currently_received /= 4;
            for(mp_uint_t i = 0; i < currently_received; i++) {

                mp_obj_t tuple[2];
                if(items[i].duration0 != 0)
                {
                    tuple[0] = mp_obj_new_int(items[i].level0);
                    tuple[1] = mp_obj_new_int(items[i].duration0);
                    mp_obj_list_append(ret_items, mp_obj_new_tuple(2, tuple));
                }

                if(items[i].duration1 != 0)
                {
                    tuple[0] = mp_obj_new_int(items[i].level1);
                    tuple[1] = mp_obj_new_int(items[i].duration1);
                    mp_obj_list_append(ret_items, mp_obj_new_tuple(2, tuple));
                }
            }

            /* Free the fetched memory area in the buffer */
            vRingbufferReturnItem(ringbuf, (void*)items);

            total_received += currently_received;
            currently_received = 0;
        }

        /* Break out after the first cycle if the maximum timeout was specified */
        if(timeout != portMAX_DELAY) {
            break;
        }
    }
    rmt_rx_stop(self->config.channel);

    return mp_obj_new_tuple(((mp_obj_list_t*)ret_items)->len, ((mp_obj_list_t*)ret_items)->items);

}
MP_DEFINE_CONST_FUN_OBJ_KW(mach_rmt_pulses_get_obj, 0, mach_rmt_pulses_get);

STATIC const mp_map_elem_t mach_rmt_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&mach_rmt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&mach_rmt_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pulses_send),         (mp_obj_t)&mach_rmt_pulses_send_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pulses_get),          (mp_obj_t)&mach_rmt_pulses_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_LOW),                 MP_OBJ_NEW_SMALL_INT(RMT_CARRIER_LEVEL_LOW) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_HIGH),                MP_OBJ_NEW_SMALL_INT(RMT_CARRIER_LEVEL_HIGH) },
};

STATIC MP_DEFINE_CONST_DICT(mach_rmt_locals_dict, mach_rmt_locals_dict_table);

const mp_obj_type_t mach_rmt_type = {
    { &mp_type_type },
    .name = MP_QSTR_RMT,
    .make_new = mach_rmt_make_new,
    .locals_dict = (mp_obj_t)&mach_rmt_locals_dict,
};

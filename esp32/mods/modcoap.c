/*
 * Copyright (c) 2018, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/gc.h"

#include "coap.h"

#include "modcoap.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "lwipsocket.h"
#include "netutils.h"

#include "freertos/semphr.h"


/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define MODCOAP_DEFAULT_PORT    (5683)
#define MODCOAP_IP4_MULTICAST   ("224.0.1.187")
#define MODCOAP_REQUEST_GET     (0x01)
#define MODCOAP_REQUEST_PUT     (0x02)
#define MODCOAP_REQUEST_POST    (0x04)
#define MODCOAP_REQUEST_DELETE  (0x08)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
#ifdef ENABLE_COAP_SERVER
typedef struct mod_coap_resource_obj_s {
    mp_obj_base_t base;
    coap_resource_t* coap_resource;
    struct mod_coap_resource_obj_s* next;
    uint8_t* value;
    unsigned char* uri;
    uint32_t max_age;
    uint16_t etag_value;
    uint16_t value_len;
    uint8_t mediatype;
    bool etag;
}mod_coap_resource_obj_t;
#endif

typedef struct mod_coap_obj_s {
    mp_obj_base_t base;
    coap_context_t* context;
    mod_network_socket_obj_t* socket;
#ifdef ENABLE_COAP_SERVER
    mod_coap_resource_obj_t* resources;
#endif
    SemaphoreHandle_t semphr;
}mod_coap_obj_t;



/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
#ifdef ENABLE_COAP_SERVER
STATIC mod_coap_resource_obj_t* find_resource(coap_resource_t* resource);
STATIC mod_coap_resource_obj_t* find_resource_by_key(coap_key_t key);
STATIC mod_coap_resource_obj_t* add_resource(const char* uri, uint8_t mediatype, uint8_t max_age, mp_obj_t value, bool etag);
STATIC void remove_resource_by_key(coap_key_t key);
STATIC void remove_resource(const char* uri);
STATIC void resource_update_value(mod_coap_resource_obj_t* resource, mp_obj_t new_value);

STATIC void coap_resource_callback_get(coap_context_t * context,
                                       struct coap_resource_t * resource,
                                       const coap_endpoint_t * endpoint,
                                       coap_address_t * address,
                                       coap_pdu_t * request,
                                       str * token,
                                       coap_pdu_t * response);

STATIC void coap_resource_callback_put(coap_context_t * context,
                                       struct coap_resource_t * resource,
                                       const coap_endpoint_t * endpoint,
                                       coap_address_t * address,
                                       coap_pdu_t * request,
                                       str * token,
                                       coap_pdu_t * response);

STATIC void coap_resource_callback_post(coap_context_t * context,
                                        struct coap_resource_t * resource,
                                        const coap_endpoint_t * endpoint,
                                        coap_address_t * address,
                                        coap_pdu_t * request,
                                        str * token,
                                        coap_pdu_t * response);

STATIC void coap_resource_callback_delete(coap_context_t * context,
                                          struct coap_resource_t * resource,
                                          const coap_endpoint_t * endpoint,
                                          coap_address_t * address,
                                          coap_pdu_t * request,
                                          str * token,
                                          coap_pdu_t * response);

#endif
/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/
#ifdef ENABLE_COAP_SERVER
STATIC const mp_obj_type_t mod_coap_resource_type;
#endif
// Only 1 context is supported
STATIC mod_coap_obj_t* coap_obj_ptr;
STATIC bool initialized = false;


/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
#ifdef ENABLE_COAP_SERVER
// Get the resource if exists
STATIC mod_coap_resource_obj_t* find_resource(coap_resource_t* resource) {

    if(coap_obj_ptr->resources != NULL) {
        mod_coap_resource_obj_t* current = coap_obj_ptr->resources;
        for(; current != NULL; current = current->next) {
            // The hash key is generated from Uri
            if(memcmp(current->coap_resource->key, resource->key, sizeof(current->coap_resource->key)) == 0) {
                return current;
            }
        }
    }
    return NULL;
}

// Get the resource if exists by its key
STATIC mod_coap_resource_obj_t* find_resource_by_key(coap_key_t key) {

    if(coap_obj_ptr->resources != NULL) {
        mod_coap_resource_obj_t* current = coap_obj_ptr->resources;
        for(; current != NULL; current = current->next) {
            // The hash key is generated from Uri
            if(memcmp(current->coap_resource->key, key, sizeof(current->coap_resource->key)) == 0) {
                return current;
            }
        }
    }
    return mp_const_none;
}


// Create a new resource in the scope of the only context
STATIC mod_coap_resource_obj_t* add_resource(const char* uri, uint8_t mediatype, uint8_t max_age, mp_obj_t value, bool etag) {

    // Currently only 1 context is supported
    mod_coap_obj_t* context = coap_obj_ptr;

    coap_key_t key;
    (void)coap_hash_path((const unsigned char*)uri, strlen(uri), key);

    // Check if there is at least 1 resource
    if(context->resources != NULL) {
        mod_coap_resource_obj_t* current = context->resources;
        // Iterate through the resources and check whether the new one exists
        for(; current != NULL; current = current->next) {
            // The hash key is generated from Uri
            if(memcmp(key, current->coap_resource->key, sizeof(key)) == 0) {
                // Resource already exists
                return NULL;
            }
        }
    }

    // Resource does not exist, create a new resource object
    mod_coap_resource_obj_t* resource = m_new_obj(mod_coap_resource_obj_t);
    resource->base.type = &mod_coap_resource_type;

    // Get media type
    resource->mediatype = mediatype; // -1 means no media type is specified
    // Get max age
    resource->max_age = max_age; // -1 means no max_age is specified
    // Get ETAG
    resource->etag = etag; // by default it is false
    resource->etag_value = 0; // start with 0, resource_update_value() will update it (0 is incorrect for E-Tag value)

    // No next elem
    resource->next = NULL;

    // uri parameter pointer will be destroyed, pass a pointer to a permanent location
    resource->uri = gc_alloc(strlen(uri),false);
    memcpy(resource->uri, uri, strlen(uri));
    resource->coap_resource = coap_resource_init((const unsigned char* )resource->uri, strlen(uri), 0);
    if(resource->coap_resource != NULL) {
        // Add the resource to the Coap context
        coap_add_resource(context->context, resource->coap_resource);

        // If no default value is given set it to 0
        if(value == MP_OBJ_NULL) {
            value = mp_obj_new_int(0);
        }
        // Initialize default value
        resource_update_value(resource, value);

        // Add the resource to our context
        if(context->resources == NULL) {
            // No resource exists, add as first element
            context->resources = resource;
        }
        else {
            mod_coap_resource_obj_t* current = context->resources;
            // Find the last resource
            for(; current->next != NULL; current = current->next) {}
            // Append the new resource to the end of the list
            current->next = resource;
        }

        return resource;
    }
    else {
        gc_free(resource->uri);
        m_del_obj(mod_coap_resource_obj_t, resource);
        // Resource cannot be created
        return NULL;
    }

    // Just for the compiler
    return mp_const_none;
}

// Remove the resource in the scope of the only context by its key
STATIC void remove_resource_by_key(coap_key_t key) {

    // Currently only 1 context is supported
    mod_coap_obj_t* context = coap_obj_ptr;

    if(context->resources != NULL) {
        mod_coap_resource_obj_t* current = context->resources;
        mod_coap_resource_obj_t* previous = context->resources;
        for(; current != NULL; current = current->next) {

            // The hash key is generated from Uri
            if(memcmp(current->coap_resource->key, key, sizeof(coap_key_t)) == 0) {
                // Resource found, remove from the list
                // Check if it is the first element in the list
                if(context->resources == current) {
                    // If no more element in the list then invalidate the list
                    if(current->next == NULL) {
                        context->resources = NULL;
                    }
                    // Other elements are in the list
                    else {
                        context->resources = current->next;
                    }
                }
                else {
                    // It is not the first element
                    previous->next = current->next;
                }

                // Free the URI
                gc_free(current->uri);
                // Free the resource in coap's scope
                coap_delete_resource(context->context, key);
                // Free the element in MP scope
                gc_free(current->value);
                // Free the resource itself
                gc_free(current);

                return;
            }

            // Mark the current element as previous, needed when removing the actual current element from the list
            previous = current;
        }
    }
}


// Remove the resource in the scope of the only context
STATIC void remove_resource(const char* uri) {

    coap_key_t key;
    (void)coap_hash_path((const unsigned char*)uri, strlen(uri), key);

    remove_resource_by_key(key);
}

// Update the value of a resource
STATIC void resource_update_value(mod_coap_resource_obj_t* resource, mp_obj_t new_value) {

    // If ETAG value is needed then update it
    if(resource->etag == true) {
        resource->etag_value += 1;
        // 0 as E-Tag value is not correct...
        if(resource->etag_value == 0) {
            resource->etag_value += 1;
        }
    }

    // Invalidate current data first
    resource->value_len = 0;
    gc_free(resource->value);

    if (mp_obj_is_integer(new_value)) {

        uint32_t value = mp_obj_get_int_truncated(new_value);
        if (value > 0xFF) {
            resource->value_len = 2;
        } else if (value > 0xFFFF) {
            resource->value_len = 4;
        } else {
            resource->value_len = 1;
        }

        // Allocate memory for the new data
        resource->value = gc_alloc(resource->value_len, false);
        memcpy(resource->value, &value, sizeof(value));

    } else {

        mp_buffer_info_t value_bufinfo;
        mp_get_buffer_raise(new_value, &value_bufinfo, MP_BUFFER_READ);
        resource->value_len = value_bufinfo.len;

        // Allocate memory for the new data
        resource->value = gc_alloc(resource->value_len, false);
        memcpy(resource->value, value_bufinfo.buf, resource->value_len);
    }
}


// Callback function when GET method is received
STATIC void coap_resource_callback_get(coap_context_t * context,
                                       struct coap_resource_t * resource,
                                       const coap_endpoint_t * endpoint,
                                       coap_address_t * address,
                                       coap_pdu_t * request,
                                       str * token,
                                       coap_pdu_t * response)
{

    mod_coap_resource_obj_t* resource_obj = find_resource(resource);

    // Check if the resource exists. (e.g.: has not been removed in the background before we got the semaphore in mod_coap_read())
    if(resource_obj != NULL) {

        // Check if media type of the resource is given
        if(resource_obj->mediatype != -1) {
            coap_opt_iterator_t opt_it;
            // Need to check if ACCEPT option is specified and we can serve it
            coap_opt_t *opt = coap_check_option(request, COAP_OPTION_ACCEPT, &opt_it);
            if(opt != NULL) {

                unsigned short length = coap_opt_length(opt);
                unsigned int decoded = COAP_MEDIATYPE_TEXT_PLAIN;

                if(length != 0) { // 0 as length means the value is 0, which is MEDIATYPE TEXT PLAIN
                    unsigned char* value = coap_opt_value(opt);
                    decoded = coap_decode_var_bytes(value, length);
                }

                // If the accepted media type and stored one does not match respond with 4.06 Not Acceptable
                if(decoded != resource_obj->mediatype) {
                    response->hdr->code = COAP_RESPONSE_CODE(406);
                    const char* error_message = coap_response_phrase(response->hdr->code);
                    coap_add_data(response, strlen(error_message), (unsigned char *)error_message);
                    return;
                }
            }
        }

        // If no ETAG option is specified in the request than the response code will be 205
        response->hdr->code = COAP_RESPONSE_CODE(205);

        // Check if ETAG value is maintained for the resource
        if(resource_obj->etag == true) {

            coap_opt_iterator_t opt_it;
            // Need to check if E-TAG option is specified and we can serve it
            coap_opt_t *opt = coap_check_option(request, COAP_OPTION_ETAG, &opt_it);
            if(opt != NULL) {

                unsigned short length = coap_opt_length(opt);
                unsigned int decoded = 0;

                if(length != 0) { // 0 as length means the value is 0
                    unsigned char* value = coap_opt_value(opt);
                    decoded = coap_decode_var_bytes(value, length);
                }

                if(decoded == resource_obj->etag_value) {
                    // If the resource has not been updated since the last request
                    // Response must include the E-Tag option in this case, this is ensured to be happened
                    response->hdr->code = COAP_RESPONSE_CODE(203);
                }
            }
        }

        // Add the options if configured
        unsigned char buf[3];

        if(resource_obj->etag == true) {
            coap_add_option(response, COAP_OPTION_ETAG, coap_encode_var_bytes(buf, resource_obj->etag_value), buf);
        }

        if(resource_obj->mediatype != -1) {
            coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, resource_obj->mediatype), buf);
        }

        if(resource_obj->max_age != -1) {
            coap_add_option(response, COAP_OPTION_MAXAGE, coap_encode_var_bytes(buf, resource_obj->max_age), buf);
        }

        // Add the data itself if updated
        if(response->hdr->code == COAP_RESPONSE_CODE(205)) {
            coap_add_data(response, resource_obj->value_len, (unsigned char *)resource_obj->value);
        }
    }
    else {
        // 2.02 Deleted: The entry was deleted by another thread in the background
        response->hdr->code = COAP_RESPONSE_CODE(202);
        const char* error_message = coap_response_phrase(response->hdr->code);
        coap_add_data(response, strlen(error_message), (unsigned char *)error_message);
    }
}


// Callback function when PUT method is received
STATIC void coap_resource_callback_put(coap_context_t * context,
                                       struct coap_resource_t * resource,
                                       const coap_endpoint_t * endpoint,
                                       coap_address_t * address,
                                       coap_pdu_t * request,
                                       str * token,
                                       coap_pdu_t * response)
{

    /* Due to limitation of libcoap, a previously not existed resource cannot be created with PUT
     * As a result the If-Non-Match option does not work as execution will not reach this function
     * if the object with the given URI does not exist
     * https://sourceforge.net/p/libcoap/mailman/message/36177974
     * https://github.com/obgm/libcoap/pull/225
    */

    mod_coap_resource_obj_t* resource_obj = find_resource(resource);

    // Check if the resource exists. (e.g.: has not been removed in the background before we got the semaphore in mod_coap_read())
    if(resource_obj != NULL) {

        bool precondition = false;
        coap_opt_iterator_t opt_it;

        // Check for If-Match option, currently only 1 If-Match option is supported
        coap_opt_t *opt = coap_check_option(request, COAP_OPTION_IF_MATCH, &opt_it);

        if(opt != NULL) {

            unsigned short length = coap_opt_length(opt);
            if(length != 0) { // 0 as length means the value is 0

                // The value is an E-TAG
                unsigned char* value = coap_opt_value(opt);
                unsigned int etag = coap_decode_var_bytes(value, length);

                // If we maintain the E-TAG of the resource then check for equality
                if((resource_obj->etag == true) && (etag == resource_obj->etag_value)) {
                    precondition = true;
                }
            }
            // If no value is specified for If-Match option then update the resource anyway
            else
            {
                precondition = true;
            }
        }
        // If no If-Match option given, update the value
        else {
            precondition = true;
        }

        if(precondition == true) {
            // Need to check if CONTENT-FORMAT option is specified and update the stored media type accordingly
            coap_opt_t *opt = coap_check_option(request, COAP_OPTION_CONTENT_FORMAT, &opt_it);

            if(opt != NULL) {

                unsigned short length = coap_opt_length(opt);

                if(length != 0) { // 0 as length means the value is 0
                    unsigned char* value = coap_opt_value(opt);
                    resource_obj->mediatype = coap_decode_var_bytes(value, length);
                }
                else {
                    resource_obj->mediatype = 0;
                }
            }
            // If no CONTENT-FORMAT is specified set the media type to unknown
            else {
                resource_obj->mediatype = -1;
            }

            // Update the data and set response code and add E-Tag option if needed
            size_t size;
            unsigned char *data;
            int ret = coap_get_data(request, &size, &data);
            if(ret == 1) {
                mp_obj_t new_value = mp_obj_new_str((const char*)data, size);
                resource_update_value(resource_obj, new_value);

                // Value is updated
                response->hdr->code = COAP_RESPONSE_CODE(204);

                // Add E-Tag option if configured
                unsigned char buf[3];
                if(resource_obj->etag == true) {
                    coap_add_option(response, COAP_OPTION_ETAG, coap_encode_var_bytes(buf, resource_obj->etag_value), buf);
                }
            }
            else {
                // 5.00 Internal Server error occurred
                response->hdr->code = COAP_RESPONSE_CODE(500);
                const char* error_message = coap_response_phrase(response->hdr->code);
                coap_add_data(response, strlen(error_message), (unsigned char *)error_message);
            }
        }
        else {
            // 4.12 Precondition failed
            response->hdr->code = COAP_RESPONSE_CODE(412);
            const char* error_message = coap_response_phrase(response->hdr->code);
            coap_add_data(response, strlen(error_message), (unsigned char *)error_message);
        }
    }
    else {
        // 2.02 Deleted: The entry was deleted by another thread in the background
        response->hdr->code = COAP_RESPONSE_CODE(202);
        const char* error_message = coap_response_phrase(response->hdr->code);
        coap_add_data(response, strlen(error_message), (unsigned char *)error_message);
    }
}


// Callback function when POST method is received
STATIC void coap_resource_callback_post(coap_context_t * context,
                                       struct coap_resource_t * resource,
                                       const coap_endpoint_t * endpoint,
                                       coap_address_t * address,
                                       coap_pdu_t * request,
                                       str * token,
                                       coap_pdu_t * response)
{

    /* Post does not really make sense to use over PUT as the URI of the resource must be specified by the requester
     * Due to limitation of libcoap, a previously not existed resource cannot be created with POST
     * https://sourceforge.net/p/libcoap/mailman/message/36177974
     * https://github.com/obgm/libcoap/pull/225
    */

    mod_coap_resource_obj_t* resource_obj = find_resource(resource);

    // Check if the resource exists. (e.g.: has not been removed in the background before we got the semaphore in mod_coap_read())
    if(resource_obj != NULL) {

        coap_opt_iterator_t opt_it;
        // Need to check if CONTENT-FORMAT option is specified and update the stored media type accordingly
        coap_opt_t *opt = coap_check_option(request, COAP_OPTION_CONTENT_FORMAT, &opt_it);

        if(opt != NULL) {

            unsigned short length = coap_opt_length(opt);

            if(length != 0) { // 0 as length means the value is 0
                unsigned char* value = coap_opt_value(opt);
                resource_obj->mediatype = coap_decode_var_bytes(value, length);
            }
            else {
                resource_obj->mediatype = 0;
            }
        }
        // If no CONTENT-FORMAT is specified set the media type to unknown
        else {
            resource_obj->mediatype = -1;
        }

        // Update the data and set response code and add E-Tag option if needed
        size_t size;
        unsigned char *data;
        int ret = coap_get_data(request, &size, &data);
        if(ret == 1) {
            mp_obj_t new_value = mp_obj_new_str((const char*)data, size);
            resource_update_value(resource_obj, new_value);

            // Value is updated
            response->hdr->code = COAP_RESPONSE_CODE(204);

            // Add E-Tag option if configured
            unsigned char buf[3];
            if(resource_obj->etag == true) {
                coap_add_option(response, COAP_OPTION_ETAG, coap_encode_var_bytes(buf, resource_obj->etag_value), buf);
            }
        }
        else {
            // 5.00 Internal Server error occurred
            response->hdr->code = COAP_RESPONSE_CODE(500);
            const char* error_message = coap_response_phrase(response->hdr->code);
            coap_add_data(response, strlen(error_message), (unsigned char *)error_message);
        }
    }
    else {
        // 2.02 Deleted: The entry was deleted by another thread in the background
        response->hdr->code = COAP_RESPONSE_CODE(202);
        const char* error_message = coap_response_phrase(response->hdr->code);
        coap_add_data(response, strlen(error_message), (unsigned char *)error_message);
    }
}


// Callback function when DELETE method is received
STATIC void coap_resource_callback_delete(coap_context_t * context,
                                           struct coap_resource_t * resource,
                                           const coap_endpoint_t * endpoint,
                                           coap_address_t * address,
                                           coap_pdu_t * request,
                                           str * token,
                                           coap_pdu_t * response)
{
    mod_coap_resource_obj_t* resource_obj = find_resource(resource);

    if(resource_obj != NULL) {
        // Remove it if exists
        remove_resource_by_key(resource->key);
    }

    // Reply with DELETED response
    response->hdr->code = COAP_RESPONSE_CODE(202);
}
#endif

/******************************************************************************
 DEFINE COAP RESOURCE CLASS FUNCTIONS
 ******************************************************************************/

#ifdef ENABLE_COAP_SERVER
// Add attribute to a resource
STATIC mp_obj_t mod_coap_resource_add_attribute(mp_obj_t self_in, mp_obj_t name, mp_obj_t val) {

    mod_coap_resource_obj_t* self = (mod_coap_resource_obj_t*)self_in;

    const char* name_string = mp_obj_str_get_str(name);
    const char* val_string = mp_obj_str_get_str(val);

    coap_attr_t * attribute = coap_add_attr(self->coap_resource,
                                           (const unsigned char*)name_string,
                                           strlen(name_string),
                                           (const unsigned char*)val_string,
                                           strlen(val_string),
                                           0);

    if(attribute == NULL) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_MemoryError, "Attribute cannot be added"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mod_coap_resource_add_attribute_obj, mod_coap_resource_add_attribute);


// Gets or sets the value of a resource
STATIC mp_obj_t mod_coap_resource_value(mp_uint_t n_args, const mp_obj_t *args) {

    mod_coap_resource_obj_t* self = (mod_coap_resource_obj_t*)args[0];
    mp_obj_t ret = mp_const_none;

    xSemaphoreTake(coap_obj_ptr->semphr, portMAX_DELAY);
    // If the value exists, e.g.: not deleted from another task before we got the semaphore
    if(self->value != NULL) {
        if (n_args == 1) {
            // get
            ret = mp_obj_new_bytes(self->value, self->value_len);
        } else {
            // set
            resource_update_value(self, (mp_obj_t)args[1]);
        }
    }
    xSemaphoreGive(coap_obj_ptr->semphr);

    return ret;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_coap_resource_value_obj, 1, 2, mod_coap_resource_value);


// Enable or disable a specific action on a resource
STATIC mp_obj_t mod_coap_resource_callback_enable(mp_obj_t self_in, mp_obj_t request_type_in, mp_obj_t enable_in) {

    mod_coap_resource_obj_t* self = (mod_coap_resource_obj_t*)self_in;

    mp_int_t request_type = mp_obj_get_int(request_type_in);
    bool enable = mp_obj_get_int(request_type_in) == 0 ? false : true;

    if(request_type & MODCOAP_REQUEST_GET) {
        if(enable)    coap_register_handler(self->coap_resource, COAP_REQUEST_GET, coap_resource_callback_get);
        else          coap_register_handler(self->coap_resource, COAP_REQUEST_GET, NULL);
    }

    if(request_type & MODCOAP_REQUEST_PUT) {
        if(enable)    coap_register_handler(self->coap_resource, COAP_REQUEST_PUT, coap_resource_callback_put);
        else          coap_register_handler(self->coap_resource, COAP_REQUEST_PUT, NULL);
    }

    if(request_type & MODCOAP_REQUEST_POST) {
        if(enable)    coap_register_handler(self->coap_resource, COAP_REQUEST_POST, coap_resource_callback_post);
        else          coap_register_handler(self->coap_resource, COAP_REQUEST_POST, NULL);
    }

    if(request_type & MODCOAP_REQUEST_DELETE) {
        if(enable)    coap_register_handler(self->coap_resource, COAP_REQUEST_DELETE, coap_resource_callback_delete);
        else          coap_register_handler(self->coap_resource, COAP_REQUEST_DELETE, NULL);
    }

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mod_coap_resource_callback_enable_obj, mod_coap_resource_callback_enable);


STATIC const mp_map_elem_t coap_resource_locals_table[] = {
    // instance methods
        { MP_OBJ_NEW_QSTR(MP_QSTR_add_attribute),               (mp_obj_t)&mod_coap_resource_add_attribute_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_value),                       (mp_obj_t)&mod_coap_resource_value_obj },
        { MP_OBJ_NEW_QSTR(MP_QSTR_callback),                    (mp_obj_t)&mod_coap_resource_callback_enable_obj },
};
STATIC MP_DEFINE_CONST_DICT(coap_resource_locals, coap_resource_locals_table);

STATIC const mp_obj_type_t mod_coap_resource_type = {
    { &mp_type_type },
    .name = MP_QSTR_CoapResource,
    .locals_dict = (mp_obj_t)&coap_resource_locals,
};
#endif

/******************************************************************************
 DEFINE COAP CLASS FUNCTIONS
 ******************************************************************************/


STATIC void mod_coap_init_helper(mp_obj_t address, bool service_discovery) {

    coap_address_t server_address;

    // Create a new Coap context
    coap_address_init(&server_address);
    server_address.addr.sin.sin_family = AF_INET;
    // The address will be in Big Endian order
    uint16_t port  = netutils_parse_inet_addr(address, (uint8_t*)&server_address.addr.sin.sin_addr.s_addr, NETUTILS_BIG);
    // Store the port in network byte order
    server_address.addr.sin.sin_port = htons(port);

    // Will create a new socket and bind it to the address and port
    coap_obj_ptr->context = coap_new_context(&server_address);

    // Create socket object
    coap_obj_ptr->socket = m_new_obj_with_finaliser(mod_network_socket_obj_t);
    coap_obj_ptr->socket->sock_base.nic_type = MP_OBJ_NULL;
    coap_obj_ptr->socket->base.type = (mp_obj_t)&socket_type;
    coap_obj_ptr->socket->sock_base.u.u_param.domain = AF_INET;
    // Coap protocol works on UDP
    coap_obj_ptr->socket->sock_base.u.u_param.type = SOCK_DGRAM;
    coap_obj_ptr->socket->sock_base.u.u_param.proto = IPPROTO_UDP;
    coap_obj_ptr->socket->sock_base.nic = MP_OBJ_NULL;
    coap_obj_ptr->socket->sock_base.nic_type = NULL;
    coap_obj_ptr->socket->sock_base.u.u_param.fileno = -1;
    coap_obj_ptr->socket->sock_base.timeout = 0;
    coap_obj_ptr->socket->sock_base.is_ssl = false;
    coap_obj_ptr->socket->sock_base.connected = false;

    // Find and register the NIC
    coap_obj_ptr->socket->sock_base.nic = mod_network_find_nic(coap_obj_ptr->socket, (const byte *)"");
    coap_obj_ptr->socket->sock_base.nic_type = (mod_network_nic_type_t*)mp_obj_get_type(coap_obj_ptr->socket->sock_base.nic);
    // Register the socket number
    coap_obj_ptr->socket->sock_base.u.sd = coap_obj_ptr->context->sockfd;

    // Add the socket to the list
    modusocket_socket_add(coap_obj_ptr->socket->sock_base.u.sd, true);

    // Listen on coap multicast ip address for service discovery if enabled
    if(service_discovery == true) {
        // Compose the address structure
        struct ip_mreq mreq;
        memcpy(&mreq.imr_interface, &server_address.addr.sin.sin_addr, sizeof(mreq.imr_interface));
        int _errno = 0;
        mp_obj_t list = mp_obj_new_list(0, NULL);
        mp_obj_list_append(list, mp_obj_new_str(MODCOAP_IP4_MULTICAST, strlen(MODCOAP_IP4_MULTICAST)));
        mp_obj_list_append(list, mp_obj_new_int(0)); // Port does not matter
        netutils_parse_inet_addr(list, (uint8_t*)&mreq.imr_multiaddr, NETUTILS_BIG);

        // Set socket option to join multicast group
        coap_obj_ptr->socket->sock_base.nic_type->n_setsockopt(coap_obj_ptr->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq), &_errno);
    }

}

STATIC const mp_arg_t mod_coap_init_args[] = {
        { MP_QSTR_address,                  MP_ARG_OBJ  | MP_ARG_REQUIRED, },
        { MP_QSTR_port,                     MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_int = MODCOAP_DEFAULT_PORT}},
        { MP_QSTR_service_discovery,        MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = false}},
};

// Initialize the module
STATIC mp_obj_t mod_coap_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    // The Coap module should be initialized only once
    if(initialized == false) {

        MP_STATE_PORT(coap_ptr) = gc_alloc(sizeof(mod_coap_obj_t), false);
        coap_obj_ptr = MP_STATE_PORT(coap_ptr);
        coap_obj_ptr->context = NULL;
#ifdef ENABLE_COAP_SERVER
        coap_obj_ptr->resources = NULL;
#endif
        coap_obj_ptr->socket = NULL;
        coap_obj_ptr->semphr = NULL;

        mp_arg_val_t args[MP_ARRAY_SIZE(mod_coap_init_args)];
        mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_coap_init_args, args);

        mp_obj_t list = mp_obj_new_list(0, NULL);
        // Get the address as a string
        mp_obj_list_append(list, args[0].u_obj);
        // Get the port as a number
        mp_obj_list_append(list, mp_obj_new_int(args[1].u_int));
        // Get whether service discovery is supported
        bool service_discovery = args[2].u_bool;

        mod_coap_init_helper(list, service_discovery);

        coap_obj_ptr->semphr = xSemaphoreCreateBinary();
        xSemaphoreGive(coap_obj_ptr->semphr);

        initialized = true;
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Coap module already initialized!"));
    }

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_coap_init_obj, 1, mod_coap_init);

#ifdef ENABLE_COAP_SERVER
// Get the socket of the Coap
STATIC mp_obj_t mod_coap_socket(void) {

    // The Coap module should have been already initialized
    if(initialized == true) {
        return coap_obj_ptr->socket;
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Coap module has not been initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_coap_socket_obj, mod_coap_socket);


STATIC const mp_arg_t mod_coap_add_resource_args[] = {
        { MP_QSTR_uri,                      MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_media_type,               MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = -1}},
        { MP_QSTR_max_age,                  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = -1}},
        { MP_QSTR_value,                    MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        { MP_QSTR_etag,                     MP_ARG_KW_ONLY  | MP_ARG_BOOL,{.u_bool = false}},
};

// Add a new resource to the context if not exists
STATIC mp_obj_t mod_coap_add_resource(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    // The Coap module should have been already initialized
    if(initialized == true) {

        xSemaphoreTake(coap_obj_ptr->semphr, portMAX_DELAY);

        mp_arg_val_t args[MP_ARRAY_SIZE(mod_coap_add_resource_args)];
        mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_coap_add_resource_args, args);

        mod_coap_resource_obj_t* res = add_resource(mp_obj_str_get_str(args[0].u_obj), args[1].u_int, args[2].u_int, args[3].u_obj, args[4].u_bool);

        xSemaphoreGive(coap_obj_ptr->semphr);

        // Theoretically it can happen that the resource was created successfully but deleted right after releasing the semaphore above, but
        // the result is the same: the new resource does not exist when this function returns
        if(res == NULL) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Resource already exists or cannot be created!"));
        }

        return res;
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Coap module has not been initialized!"));
    }

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_coap_add_resource_obj, 1, mod_coap_add_resource);

// Remove a resource from the context if exists
STATIC mp_obj_t mod_coap_remove_resource(mp_obj_t uri) {

    // The Coap module should have been already initialized
    if(initialized == true) {
        xSemaphoreTake(coap_obj_ptr->semphr, portMAX_DELAY);
        remove_resource(mp_obj_str_get_str(uri));
        xSemaphoreGive(coap_obj_ptr->semphr);
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Coap module has not been initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_coap_remove_resource_obj, mod_coap_remove_resource);

// Get a resource from the context if exists
STATIC mp_obj_t mod_coap_get_resource(mp_obj_t uri_in) {

    mp_obj_t res = mp_const_none;
    // The Coap module should have been already initialized
    if(initialized == true) {
        xSemaphoreTake(coap_obj_ptr->semphr, portMAX_DELAY);
        const char* uri = mp_obj_str_get_str(uri_in);
        coap_key_t key;
        (void)coap_hash_path((const unsigned char*)uri, strlen(uri), key);
        res = find_resource_by_key(key);
        xSemaphoreGive(coap_obj_ptr->semphr);
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Coap module has not been initialized!"));
    }

    return res;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_coap_get_resource_obj, mod_coap_get_resource);


// Call coap_read
STATIC mp_obj_t mod_coap_read(void) {

    // The Coap module should have been already initialized
    if(initialized == true) {
        // Take the context's semaphore to avoid concurrent access, this will guard the handler functions too
        xSemaphoreTake(coap_obj_ptr->semphr, portMAX_DELAY);
        coap_read(coap_obj_ptr->context);
        xSemaphoreGive(coap_obj_ptr->semphr);
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Coap module has not been initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_coap_read_obj, mod_coap_read);
#endif

STATIC const mp_map_elem_t mod_coap_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_coap) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_coap_init_obj },
#ifdef ENABLE_COAP_SERVER
    { MP_OBJ_NEW_QSTR(MP_QSTR_socket),                          (mp_obj_t)&mod_coap_socket_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_add_resource),                    (mp_obj_t)&mod_coap_add_resource_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_remove_resource),                 (mp_obj_t)&mod_coap_remove_resource_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_resource),                    (mp_obj_t)&mod_coap_get_resource_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),                            (mp_obj_t)&mod_coap_read_obj },

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_REQUEST_GET),                     MP_OBJ_NEW_SMALL_INT(MODCOAP_REQUEST_GET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_REQUEST_PUT),                     MP_OBJ_NEW_SMALL_INT(MODCOAP_REQUEST_PUT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_REQUEST_POST),                    MP_OBJ_NEW_SMALL_INT(MODCOAP_REQUEST_POST) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_REQUEST_DELETE),                  MP_OBJ_NEW_SMALL_INT(MODCOAP_REQUEST_DELETE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_TEXT_PLAIN),            MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_TEXT_PLAIN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_APP_LINK_FORMAT),       MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_APPLICATION_LINK_FORMAT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_APP_XML),               MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_APPLICATION_XML) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_APP_OCTET_STREAM),      MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_APPLICATION_OCTET_STREAM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_APP_RDF_XML),           MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_APPLICATION_RDF_XML) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_APP_EXI),               MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_APPLICATION_EXI) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_APP_JSON),              MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_APPLICATION_JSON) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MEDIATYPE_APP_CBOR),              MP_OBJ_NEW_SMALL_INT(COAP_MEDIATYPE_APPLICATION_CBOR) },
#endif
};

STATIC MP_DEFINE_CONST_DICT(mod_coap_globals, mod_coap_globals_table);

const mp_obj_module_t mod_coap = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mod_coap_globals,
};


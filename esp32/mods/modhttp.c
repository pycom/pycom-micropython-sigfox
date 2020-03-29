/*
 * Copyright (c) 2019, Pycom Limited.
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

#include "mpthreadport.h"
#include "mpirq.h"

#include "esp_err.h"
#include "esp_https_server.h"
#include "esp_http_client.h"
#include "modhttp.h"
#include "pycom_general_util.h"


/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define MOD_HTTP_GET        (1)
#define MOD_HTTP_PUT        (2)
#define MOD_HTTP_POST       (4)
#define MOD_HTTP_DELETE     (8)

#define MOD_HTTP_MEDIA_TYPE_TEXT_XML        "text/xml"
#define MOD_HTTP_MEDIA_TYPE_TEXT_PLAIN      "text/plain"
#define MOD_HTTP_MEDIA_TYPE_APP_XML         "application/xml"


#define MOD_HTTP_MEDIA_TYPE_TEXT_HTML_ID    (0)
#define MOD_HTTP_MEDIA_TYPE_TEXT_XML_ID     (1)
#define MOD_HTTP_MEDIA_TYPE_TEXT_PLAIN_ID   (2)
#define MOD_HTTP_MEDIA_TYPE_APP_JSON_ID     (3)
#define MOD_HTTP_MEDIA_TYPE_APP_OCTET_ID    (4)
#define MOD_HTTP_MEDIA_TYPE_APP_XML_ID      (5)

#define MOD_DEFAULT_HTTP_USER_AGENT         "ESP32 HTTP Client/1.0"


/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct mod_http_resource_obj_s {
	mp_obj_base_t base;
	struct mod_http_resource_obj_s* next;
	const char* uri;
	uint8_t* value;
	uint16_t value_len;
	uint8_t mediatype;
}mod_http_resource_obj_t;

typedef struct mod_http_server_obj_s {
	httpd_handle_t server;
	mod_http_resource_obj_t* resources;
	int max_resources;
	int num_of_resources;
}mod_http_server_obj_t;

// From esp_httpd_priv.h
#define HTTPD_SCRATCH_BUF  MAX(HTTPD_MAX_REQ_HDR_LEN, HTTPD_MAX_URI_LEN)

struct httpd_req_aux {
    struct sock_db *sd;                             /*!< Pointer to socket database */
    char            scratch[HTTPD_SCRATCH_BUF + 1]; /*!< Temporary buffer for our operations (1 byte extra for null termination) */
    size_t          remaining_len;                  /*!< Amount of data remaining to be fetched */
    char           *status;                         /*!< HTTP response's status code */
    char           *content_type;                   /*!< HTTP response's content type */
    bool            first_chunk_sent;               /*!< Used to indicate if first chunk sent */
    unsigned        req_hdrs_count;                 /*!< Count of total headers in request packet */
    unsigned        resp_hdrs_count;                /*!< Count of additional headers in response packet */
    struct resp_hdr {
        const char *field;
        const char *value;
    } *resp_hdrs;                                   /*!< Additional headers in response packet */
    struct http_parser_url url_parse_res;           /*!< URL parsing result, used for retrieving URL elements */
};

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC mod_http_resource_obj_t* find_resource(const char* uri);
STATIC mod_http_resource_obj_t* add_resource(const char* uri, mp_obj_t value, mp_int_t mediatype);
STATIC void remove_resource(const char* uri);
STATIC void resource_update_value(mod_http_resource_obj_t* resource, mp_obj_t new_value);
STATIC esp_err_t mod_http_resource_callback_helper(mod_http_resource_obj_t* resource , httpd_method_t method, mp_obj_t callback, bool action);
STATIC int mod_http_server_get_mediatype_id(const char* mediatype);
STATIC bool mod_http_server_get_acceptance(const char* accept_field, uint8_t mediatype_id);

STATIC void mod_http_server_callback_handler(void *arg_in);
STATIC esp_err_t mod_http_server_callback(httpd_req_t *r);

STATIC void mod_http_client_callback_handler(void *arg_in);
//STATIC esp_err_t mod_http_client_callback(httpd_req_t *r);

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/

STATIC const char* mod_http_mediatype[] = {
		HTTPD_TYPE_TEXT,
		MOD_HTTP_MEDIA_TYPE_TEXT_XML,
		MOD_HTTP_MEDIA_TYPE_TEXT_PLAIN,
		HTTPD_TYPE_JSON,
		HTTPD_TYPE_OCTET,
		MOD_HTTP_MEDIA_TYPE_APP_XML
};

// There can only be 1 server instance
STATIC mod_http_server_obj_t* server_obj = NULL;
STATIC bool server_initialized = false;

STATIC esp_http_client_handle_t client_obj;
STATIC esp_http_client_config_t client_config = {};
STATIC mp_obj_t client_callback;

STATIC const mp_obj_type_t mod_http_resource_type;



/******************************************************************************
 DEFINE PRIVATE SERVER FUNCTIONS
 ******************************************************************************/
// Get the resource if exists
STATIC mod_http_resource_obj_t* find_resource(const char* uri) {

	if(server_obj->resources != NULL) {
		mod_http_resource_obj_t* current = server_obj->resources;
		for(; current != NULL; current = current->next) {
			// Compare the Uri
			if(strcmp(current->uri, uri) == 0) {
				return current;
			}
		}
	}
	return NULL;
}

// Create a new resource in the scope of the only context
STATIC mod_http_resource_obj_t* add_resource(const char* uri, mp_obj_t value, mp_int_t mediatype) {

	// Resource does not exist, create a new resource object
	mod_http_resource_obj_t* resource = m_new_obj(mod_http_resource_obj_t);
	resource->base.type = &mod_http_resource_type;

	// No next elem
	resource->next = NULL;

	// uri parameter pointer will be destroyed, pass a pointer to a permanent location
	resource->uri = m_malloc(strlen(uri));
	memcpy((char*)resource->uri, uri, strlen(uri));

	// If no default value is given set it to 0
	if(value == MP_OBJ_NULL) {
		value = mp_obj_new_int(0);
	}

	// Initialize default value
	resource_update_value(resource, value);

	// Add the resource to HTTP Server
	if(server_obj->resources == NULL) {
		// No resource exists, add as first element
		server_obj->resources = resource;
	}
	else {
		mod_http_resource_obj_t* current = server_obj->resources;
		// Find the last resource
		for(; current->next != NULL; current = current->next) {}
		// Append the new resource to the end of the list
		current->next = resource;
	}
	resource->mediatype = mediatype;

	return resource;
}

// Remove the resource in the scope of the only context by its key
STATIC void remove_resource(const char* uri) {

	if(server_obj->resources != NULL) {
		mod_http_resource_obj_t* current = server_obj->resources;
		mod_http_resource_obj_t* previous = server_obj->resources;
		for(; current != NULL; current = current->next) {

			// Compare the URI
			if(strcmp(current->uri, uri) == 0) {
				// Resource found, remove from the list
				// Check if it is the first element in the list
				if(server_obj->resources == current) {
					// If no more element in the list then invalidate the list
					if(current->next == NULL) {
						server_obj->resources = NULL;
					}
					// Other elements are in the list
					else {
						server_obj->resources = current->next;
					}
				}
				else {
					// It is not the first element
					previous->next = current->next;
				}

				// Free the URI
				m_free((char*)current->uri);
				// Free the element in MP scope
				m_free(current->value);
				// Free the resource itself
				m_free(current);

				return;
			}

			// Mark the current element as previous, needed when removing the actual current element from the list
			previous = current;
		}
	}
}

// Update the value of a resource
STATIC void resource_update_value(mod_http_resource_obj_t* resource, mp_obj_t new_value) {

	// Invalidate current data first
	resource->value_len = 0;
	m_free(resource->value);

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
		resource->value = m_malloc(resource->value_len);
		memcpy(resource->value, &value, sizeof(value));

	} else {

		mp_buffer_info_t value_bufinfo;
		mp_get_buffer_raise(new_value, &value_bufinfo, MP_BUFFER_READ);
		resource->value_len = value_bufinfo.len;

		// Allocate memory for the new data
		resource->value = m_malloc(resource->value_len);
		memcpy(resource->value, value_bufinfo.buf, resource->value_len);
	}
}


STATIC esp_err_t mod_http_resource_callback_helper(mod_http_resource_obj_t* resource , httpd_method_t method, mp_obj_t callback, bool action){

	esp_err_t ret = ESP_OK;

	if(action == true) {

		/* This needs to be static otherwise, most probably due to compiler optimizations, the
		 * value of it is not updated between to subsequent calls and httpd_register_uri_handler()
		 * fails with error ESP_ERR_HTTPD_HANDLER_EXISTS because it gets the URI of the previous resource
		 */
		STATIC httpd_uri_t uri;

		// Set the URI
		uri.uri = resource->uri;

		// Save the user's callback into user context field for future usage
		uri.user_ctx = callback;
		// The registered handler is our own handler which will handle different requests and call user's callback, if any
		uri.handler = mod_http_server_callback;

		// Unregister first URI, solves ESP_ERR_HTTPD_ALLOC_MEM issue
		ret = httpd_unregister_uri(server_obj->server, uri.uri);

		if((method & MOD_HTTP_GET) && (ret == ESP_OK)) {
			uri.method = HTTP_GET;
			ret = httpd_register_uri_handler(server_obj->server, &uri);
		}

		if((method & MOD_HTTP_PUT) && (ret == ESP_OK)) {
			uri.method = HTTP_PUT;
			ret = httpd_register_uri_handler(server_obj->server, &uri);
		}

		if((method & MOD_HTTP_POST) && (ret == ESP_OK)) {
			uri.method = HTTP_POST;
			ret = httpd_register_uri_handler(server_obj->server, &uri);
		}

		if((method & MOD_HTTP_DELETE) && (ret == ESP_OK)) {
			uri.method = HTTP_DELETE;
			ret = httpd_register_uri_handler(server_obj->server, &uri);
		}
	}
	else {

		if((method & MOD_HTTP_GET) && (ret == ESP_OK)) {
			ret = httpd_unregister_uri_handler(server_obj->server, resource->uri, HTTP_GET);
		}

		if((method & MOD_HTTP_PUT) && (ret == ESP_OK)) {
			ret = httpd_unregister_uri_handler(server_obj->server, resource->uri, HTTP_PUT);
		}

		if((method & MOD_HTTP_POST) && (ret == ESP_OK)) {
			ret = httpd_unregister_uri_handler(server_obj->server, resource->uri, HTTP_POST);
		}

		if((method & MOD_HTTP_DELETE) && (ret == ESP_OK)) {
			ret = httpd_unregister_uri_handler(server_obj->server, resource->uri, HTTP_DELETE);
		}
	}

	return ret;
}

// Compares Accept request-header field with resources mediatype
STATIC bool mod_http_server_get_acceptance(const char* accept_field, uint8_t mediatype_id) {

	// Start with no acceptance
	bool accept = false;

	// Initiate resource's mediatype long slice
	char* slice;
	slice = (char*)malloc(strlen(mod_http_mediatype[mediatype_id])*(sizeof(char)+1));

	// Handle edge case
	if(strlen(accept_field) >= strlen(mod_http_mediatype[mediatype_id])) {
		// Go through on accept field buffer with resources mediatype long window
		for(int i=0; i<=strlen(accept_field)-strlen(mod_http_mediatype[mediatype_id]); i++) {
			// Reset slice
			memset(slice,'\0',strlen(mod_http_mediatype[mediatype_id])+1);
			// Get slice from accept field
			strncpy(slice, accept_field+i, strlen(mod_http_mediatype[mediatype_id]));
			// Accept in case of match
			if(strcmp(mod_http_mediatype[mediatype_id], slice) == 0) {
				accept = true;
				break;
			}
		}
	}

	return accept;
}

STATIC int mod_http_server_get_mediatype_id(const char* mediatype) {

	int id = -1;

	for(int i = 0; i < (sizeof(mod_http_mediatype)/sizeof(mod_http_mediatype[0])); i++) {
		if(strcmp(mediatype, mod_http_mediatype[i]) == 0) {
			id = i;
			break;
		}
	}

	return id;
}

STATIC mp_obj_t mod_http_server_get_headers(const char* hdr_ptr, int hdr_cnt) {
	// Initiate an empty request_headers dictionary MP Object
	mp_obj_t request_headers = mp_obj_new_dict(0);

	// Initiate Start and End indices
	int key_start = 0;
	int key_end = 0;
	int val_start = 0;
	int val_end = 0;

	// First look for Key
	bool find_key = 1;

	char key[HTTPD_SCRATCH_BUF];
	char val[HTTPD_SCRATCH_BUF];

	for(int i=0; i<HTTPD_SCRATCH_BUF; i++) {
		if(find_key) {
			// Looking for key
			if(hdr_ptr[i] == ':') {
				// Set Key end index
				key_end = i - 1;

				// Get Key substring
				memset(key, '\0', HTTPD_SCRATCH_BUF);
				memcpy(key, hdr_ptr + key_start, key_end - key_start + 1);

				// Set Value start index
				val_start = i + 2;

				// Set iterator
				i = i + 2;

				// Look for Value
				find_key = 0;
			}
		} else {
			// Looking for value
			if(hdr_ptr[i] == '\0') {
				// Set Value end index
				val_end = i - 1;

				// Get Value substring
				memset(val, '\0', HTTPD_SCRATCH_BUF);
				memcpy(val, hdr_ptr + val_start, val_end - val_start + 1);

				// Set Key start index
				key_start = i + 2;

				// Set iterator
				i = i + 2;

				// Look for Key
				find_key = 1;

				// Less Header is remained
				hdr_cnt--;

				// Store Key-Value Header pair
				mp_obj_dict_store(request_headers, mp_obj_new_str(key, strlen(key)), mp_obj_new_str(val, strlen(val)));
			}
		}
		if(hdr_cnt <= 0) {
			break;
		}
	}
	return request_headers;
}

STATIC void mod_http_server_callback_handler(void *arg_in) {

	/* The received arg_in is a tuple with 4 elements
	 * 0 - user's MicroPython callback
	 * 1 - URI as string
	 * 2 - HTTP method as INT
	 * 3 - Headers as dictionary
	 * 4 - Body as a string (if any)
	 * 5 - New URI as a string (if any)
	 * 6 - Status Code as int
	 */

	mp_obj_t args[6];
	// URI
	args[0] = ((mp_obj_tuple_t*)arg_in)->items[1];
	// method
	args[1] = ((mp_obj_tuple_t*)arg_in)->items[2];
	// Headers
	args[2] = ((mp_obj_tuple_t*)arg_in)->items[3];
	// Body
	args[3] = ((mp_obj_tuple_t*)arg_in)->items[4];
	// New URI
	args[4] = ((mp_obj_tuple_t*)arg_in)->items[5];
	// Status Code
	args[5] = ((mp_obj_tuple_t*)arg_in)->items[6];

	// Call the user registered MicroPython function
	mp_call_function_n_kw(((mp_obj_tuple_t*)arg_in)->items[0], 6, 0, args);

}

STATIC esp_err_t mod_http_server_callback(httpd_req_t *r){

	char* content = NULL;
	bool error = false;
	mp_obj_t args[7];
	mod_http_resource_obj_t* resource = find_resource(r->uri);
	char* new_uri = "";
	int status_code = 404;
	int mediatype_id = 0;

	// Get headers
	struct httpd_req_aux *ra = r->aux;
    const char* hdr_ptr = ra->scratch;
    int hdr_cnt = ra->req_hdrs_count;
    mp_obj_t request_headers = mod_http_server_get_headers(hdr_ptr, hdr_cnt);

	// If the resource does not exist anymore then send back 404
	if(resource == NULL){
		httpd_resp_send_404(r);
		status_code = 404;
		// This can happen if locally the resource has been removed but for some reason it still exists in the HTTP Server library context...
		return ESP_FAIL;
	}

	// Get the content part of the message
	if(r->content_len > 0) {
		// Allocate memory for the content
		content = m_malloc(r->content_len);
		if(content != NULL) {
			// Get the content from the message
			int recv_length = httpd_req_recv(r, content, r->content_len);

			// If return value less than 0, error occurred
			if(recv_length < 0) {

				// Check if timeout error occurred and send back appropriate response
				if (recv_length == HTTPD_SOCK_ERR_TIMEOUT) {
					httpd_resp_send_408(r);
					status_code = 408;
				}

				//TODO: check if exception is needed
				error = true;
			}
			else if(recv_length != r->content_len) {

				//TODO: Handle this case properly
				printf("recv_length != r->content_len !!\n");

				//TODO: check if exception is needed
				error = true;
			}
		}
		else {
			//TODO: ESP_FAIL should be returned as per HTTP LIB, check if exception is needed
			return ESP_FAIL;
			//nlr_raise(mp_obj_new_exception_msg(&mp_type_MemoryError, "Not enough free memory to handle request to HTTP Server!"));
		}
	}

	// Get "Content-Type" field
	size_t length = httpd_req_get_hdr_value_len(r, "Content-Type");
	if(length > 0) {
		// length+1 is needed because with length the ESP_ERR_HTTPD_RESULT_TRUNC is dropped
		char* buf = m_malloc(length+1);
		esp_err_t ret = httpd_req_get_hdr_value_str(r, "Content-Type", buf, length+1);
		if(ret == ESP_OK) {
			mediatype_id = mod_http_server_get_mediatype_id(buf);
		}
		m_free(buf);
	}

	if(error == false) {

		// This is a GET request, send back the current value of the resource
		if(r->method == HTTP_GET) {
			// Check if "Accept" field is defined
			size_t length = httpd_req_get_hdr_value_len(r, "Accept");
			if(length > 0) {
				// length+1 is needed because with length the ESP_ERR_HTTPD_RESULT_TRUNC is dropped
				char* buf = m_malloc(length+1);
				esp_err_t ret = httpd_req_get_hdr_value_str(r, "Accept", buf, length+1);
				if(ret == ESP_OK) {
					if(!mod_http_server_get_acceptance(buf, resource->mediatype)) {
						//406 status code is not defined in esp-idf httpd_resp_send_err()
						char* status = "406 Not Acceptable";
						char* msg    = "This request is not acceptable.";
						httpd_resp_set_status(r, status);
						httpd_resp_set_type(r, HTTPD_TYPE_TEXT);
						httpd_resp_send(r, msg, strlen(msg));
						error = true;
						status_code = 406;
					}
				}
				m_free(buf);
			}

			if(error == false) {
				// Set the media type
				httpd_resp_set_type(r, mod_http_mediatype[resource->mediatype]);
				httpd_resp_send(r, (const char*)resource->value, (ssize_t)resource->value_len);
				status_code = 200;
			}
		}
		// This is a PUT request
		else if(r->method == HTTP_PUT) {

			if(mediatype_id != -1) {
				// Update the mediatype of the resource
				resource->mediatype = (uint8_t)mediatype_id;
			}
			else {
				httpd_resp_send_err(r, 415, "Unsupported Media Type");
				error = true;
				status_code = 415;
			}

			// Update the resource if new value is provided
			if(error == false && content != NULL) {
				// Update the resource
				resource_update_value(resource, mp_obj_new_str(content, r->content_len));
				//TODO: compose here a better message
				const char resp[] = "Resource is updated.";
				httpd_resp_send(r, resp, strlen(resp));
				status_code = 200;
			}
		}

		// This is a POST request
		else if(r->method == HTTP_POST) {

			// Generate 5 digit id
			long int id_int = 10000 + (long int)esp_timer_get_time() % 89999;

			// Convert id from int to string
			char* id_str = calloc(5, sizeof(char));
			itoa(id_int, id_str, 10);

			// [parent_uri][/][5_digit_id] + null termination format
			new_uri = calloc(strlen(r->uri) + 2 + 5, sizeof(char));

			// Concatanate parent_uri + '/' + id
			httpd_uri_t uri;
			strncpy(new_uri, r->uri, strlen(r->uri));
			new_uri[strlen(r->uri)] = '/';
			strncat(new_uri, id_str, 5);
			new_uri[strlen(r->uri) + 5] = '\0';
			uri.uri = new_uri;

			// Create the resource in the esp-idf http server's context
			esp_err_t ret = ESP_OK;
			ret = httpd_register_uri_handler(server_obj->server, &uri);

			if(ret == ESP_OK && server_obj->num_of_resources <= server_obj->max_resources) {
				// Add resource to MicroPython http server's context with default value
				add_resource(new_uri, mp_obj_new_str(content, r->content_len), 0);
				server_obj->num_of_resources++;

				if(mediatype_id != -1) {
					printf("mediatype id: %d\n", mediatype_id);
					find_resource(new_uri)->mediatype = (uint8_t)mediatype_id;
				}

				// 201 status code
				char* status = "201 Created";
				char* msg = calloc(11 + strlen(new_uri), sizeof(char));
				strcat(msg, "Location: ");
				strcat(msg, uri.uri);
				httpd_resp_set_status(r, status);
				httpd_resp_set_type(r, HTTPD_TYPE_TEXT);
				httpd_resp_set_hdr(r, "Location", new_uri);
				httpd_resp_send(r, msg, strlen(msg));
				status_code = 201;
			}
			else {
				new_uri = (char *) realloc(new_uri, 1);
				new_uri = calloc(1, sizeof(char));
				// 500 status code
				char* status = "500 Internal Server Error";
				char* msg    = "";
				httpd_resp_set_status(r, status);
				httpd_resp_set_type(r, HTTPD_TYPE_TEXT);
				httpd_resp_send(r, msg, strlen(msg));
				status_code = 500;
			}
		}

		else if(r->method == HTTP_DELETE) {
			// Remove resource
			remove_resource(r->uri);
			(void)httpd_unregister_uri(server_obj->server, r->uri);
			server_obj->num_of_resources--;

			// 204 status code is not defined in esp-idf
			char* status = "204 No Content";
			char* msg    = "Resource is deleted.";
			httpd_resp_set_status(r, status);
			httpd_resp_set_type(r, HTTPD_TYPE_TEXT);
			httpd_resp_send(r, msg, strlen(msg));
			status_code = 204;
		}

		// If there is a registered MP callback for this resource the parameters need to be prepared
		if(r->user_ctx != MP_OBJ_NULL) {
			// The MicroPython callback is stored in user_ctx
			args[0] = r->user_ctx;
			args[1] = mp_obj_new_str(r->uri, strlen(r->uri));
			args[2] = mp_obj_new_int(r->method);
			args[3] = request_headers;
			args[4] = mp_obj_new_str(content, r->content_len);
			args[5] = mp_obj_new_str(new_uri, strlen(new_uri));
			args[6] = mp_obj_new_int(status_code);

			// The user registered MicroPython callback will be called decoupled from the HTTP Server context in the IRQ Task
			mp_irq_queue_interrupt(mod_http_server_callback_handler, (void *)mp_obj_new_tuple(7, args));
		}
	}

	// Free up local content, no longer needed
	m_free(content);
	return ESP_OK;
}

/******************************************************************************
 DEFINE PRIVATE CLIENT FUNCTIONS
 ******************************************************************************/
// TODO handle response_headers better
STATIC mp_obj_t response_headers;
esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
	if(evt->event_id == HTTP_EVENT_ON_CONNECTED) {
		// Create empty headers dictionary
		response_headers = mp_obj_new_dict(0);
	}

	if(evt->event_id == HTTP_EVENT_ON_HEADER) {
		// Append dictionary with actual header Key: Value pair
		mp_obj_dict_store(response_headers, mp_obj_new_str(evt->header_key, strlen(evt->header_key)), mp_obj_new_str(evt->header_value, strlen(evt->header_value)));
	}

	if(evt->event_id == HTTP_EVENT_ON_DATA) {
		if (!esp_http_client_is_chunked_response(evt->client)) {
			mp_obj_t args[4];

			// Fill arguments
			args[0] = client_callback;
			args[1] = mp_obj_new_int(esp_http_client_get_status_code(client_obj));
			args[2] = response_headers;
			args[3] = mp_obj_new_str((char*)evt->data, evt->data_len);

			// The user registered MicroPython callback will be called decoupled from the HTTP Server context in the IRQ Task
			mp_irq_queue_interrupt(mod_http_client_callback_handler, (void *)mp_obj_new_tuple(5, args));
		}
	}

	return ESP_OK;
}

STATIC void mod_http_client_callback_handler(void *arg_in) {

	/* The received arg_in is a tuple with 3 elements
	 * 0 - user's MicroPython callback
	 * 1 - Status code as int
	 * 2 - Headers as dictionary
	 * 3 - Body as string
	 */

	mp_obj_t args[3];
	// Status Code
	args[0] = ((mp_obj_tuple_t*)arg_in)->items[1];
	// Headers
	args[1] = ((mp_obj_tuple_t*)arg_in)->items[2];
	// Body
	args[2] = ((mp_obj_tuple_t*)arg_in)->items[3];

	// Call the user registered MicroPython function
	mp_call_function_n_kw(((mp_obj_tuple_t*)arg_in)->items[0], 3, 0, args);

}

/******************************************************************************
 DEFINE HTTP RESOURCE CLASS FUNCTIONS
 ******************************************************************************/
// Gets or sets the value of a resource
STATIC mp_obj_t mod_http_resource_value(mp_uint_t n_args, const mp_obj_t *args) {

	mod_http_resource_obj_t* self = (mod_http_resource_obj_t*)args[0];
	mp_obj_t ret = mp_const_none;

	// Commented out -> in case of seting none value, cannot be set to any value anymore
	// If the value exists, e.g.: not deleted from another task before we got the semaphore
	//if(self->value != NULL) {
		if (n_args == 1) {
			// get
			ret = mp_obj_new_bytes(self->value, self->value_len);
		} else {
			// set
			resource_update_value(self, (mp_obj_t)args[1]);
		}
	//}
	return ret;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_http_resource_value_obj, 1, 2, mod_http_resource_value);

// Sets or removes the callback on a given method
STATIC mp_obj_t mod_http_resource_register_request_handler(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_resource_register_request_handler_args[] = {
			{ MP_QSTR_self,                    MP_ARG_OBJ  | MP_ARG_REQUIRED, },
			{ MP_QSTR_method,                  MP_ARG_INT  | MP_ARG_REQUIRED, },
			{ MP_QSTR_callback,                MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
			{ MP_QSTR_action,                  MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = true}},
	};

	mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_resource_register_request_handler_args)];
	mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_resource_register_request_handler_args, args);

	// Get the resource
	mod_http_resource_obj_t* self = (mod_http_resource_obj_t*)args[0].u_obj;
	// Get the method
	httpd_method_t method = args[1].u_int;
	// Get the callback
	mp_obj_t callback = args[2].u_obj;
	// Get the action
	bool action = args[3].u_bool;

	if(action == true && callback == MP_OBJ_NULL) {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "If the \"action\" is TRUE then \"callback\" must be defined"));
	}

	esp_err_t ret = mod_http_resource_callback_helper(self, method, callback, action);

	if(ret != ESP_OK) {
		nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Callback of the resource could not be updated, error code: %d!", ret));
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_resource_register_request_handler_obj, 2, mod_http_resource_register_request_handler);

STATIC const mp_map_elem_t http_resource_locals_table[] = {
		// instance methods
		{ MP_OBJ_NEW_QSTR(MP_QSTR_value),                       (mp_obj_t)&mod_http_resource_value_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_register_request_handler),    (mp_obj_t)&mod_http_resource_register_request_handler_obj },

};
STATIC MP_DEFINE_CONST_DICT(http_resource_locals, http_resource_locals_table);

STATIC const mp_obj_type_t mod_http_resource_type = {
		{ &mp_type_type },
		.name = MP_QSTR_HTTP_Resource,
		.locals_dict = (mp_obj_t)&http_resource_locals,
};


/******************************************************************************
 DEFINE HTTP SERVER CLASS FUNCTIONS
 ******************************************************************************/

// Initialize the http server module
STATIC mp_obj_t mod_http_server_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_server_init_args[] = {
			{ MP_QSTR_port,                     MP_ARG_INT  	| MP_ARG_KW_ONLY, {.u_int = 80}},
			{ MP_QSTR_keyfile,                  MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
			{ MP_QSTR_certfile,                 MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
			{ MP_QSTR_max_uri,                  MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 100}},
	};

	if(server_initialized == false) {

		MP_STATE_PORT(http_server_ptr) = m_malloc(sizeof(mod_http_server_obj_t));
		server_obj = MP_STATE_PORT(http_server_ptr);

		mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_server_init_args)];
		mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_server_init_args, args);

		httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
		config.httpd.max_open_sockets = 7;
		config.httpd.max_uri_handlers = 4*args[3].u_int;
		server_obj->max_resources = args[3].u_int;
		server_obj->num_of_resources = 0;

		// HTTPS Server
		if(args[0].u_int == 443) {
			config.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;

			// retrieve the file paths (with an 6 byte offset in order to strip it from the '/flash' prefix)
			const char *keyfile_path  = (args[1].u_obj == mp_const_none) ? NULL : mp_obj_str_get_str(args[1].u_obj);
			const char *certfile_path = (args[2].u_obj == mp_const_none) ? NULL : mp_obj_str_get_str(args[2].u_obj);

			// server side requires both certfile and keyfile
			if (!keyfile_path || !certfile_path) {
				nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "HTTPS Server cannot be initialized without Certification and Key Files"));
			}
			else {
				const char *signed_cert = NULL;
				const char *prvt_key = NULL;

				vstr_t vstr_ca = {};
				vstr_t vstr_key =  {};

				signed_cert = pycom_util_read_file(certfile_path, &vstr_ca);
				prvt_key = pycom_util_read_file(keyfile_path, &vstr_key);

				config.cacert_pem = (uint8_t *) signed_cert;
				config.cacert_len = strlen(signed_cert);

				config.prvtkey_pem = (uint8_t *) prvt_key;
				config.prvtkey_len = strlen(prvt_key);

				if(signed_cert == NULL) {
					nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "CA file not found"));
				}
				if(prvt_key == NULL) {
					nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "KEY file not found"));
				}
			}
		}
		// HTTP SERVER
		else {
			config.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
			config.port_insecure = args[0].u_int;
		}

		esp_err_t ret = httpd_ssl_start(&server_obj->server, &config);
		if(ret != ESP_OK) {
			m_free(server_obj);
			nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "HTTP Server could not be initialized, error code: %d", ret));
		}

		server_obj->resources = NULL;

		server_initialized = true;
	}
	else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "HTTP Server module is already initialized!"));
	}

	return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_server_init_obj, 0, mod_http_server_init);

STATIC mp_obj_t mod_http_server_deinit(void) {

	// Check if Server is already initiated
	if(server_initialized == false) {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Server is not initialized!"));
	}

	// Cleanup Server
	httpd_stop(server_obj->server);

	// Server is not initialized anymore
	server_initialized = false;

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_http_server_deinit_obj, mod_http_server_deinit);

// Adds a resource to the http server module
STATIC mp_obj_t mod_http_server_add_resource(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_server_add_resource_args[] = {
			{ MP_QSTR_uri,                     MP_ARG_OBJ  | MP_ARG_REQUIRED, },
			{ MP_QSTR_value,                   MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
			{ MP_QSTR_content_type,            MP_ARG_INT  | MP_ARG_KW_ONLY, {.u_int = MOD_HTTP_MEDIA_TYPE_TEXT_HTML_ID}},

	};

	if(server_initialized == true) {

		mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_server_add_resource_args)];
		mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_server_add_resource_args, args);

		mod_http_resource_obj_t* resource = MP_OBJ_NULL;
		esp_err_t ret = ESP_OK;
		httpd_uri_t uri;

		// Get the URI
		uri.uri = mp_obj_str_get_str(args[0].u_obj);

		if(find_resource(uri.uri) != NULL) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Resource already added!"));
		}

		if(server_obj->num_of_resources >= server_obj->max_resources) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Cannot be added more resources!"));
		}

		// Check first character of URI
		if(uri.uri[0] == '/') {

			// Create the resource in the esp-idf http server's context
			ret = httpd_register_uri_handler(server_obj->server, &uri);

			if(ret == ESP_OK) {
				// Add resource to MicroPython http server's context with default value
				resource = add_resource(uri.uri, args[1].u_obj, args[2].u_int);
				server_obj->num_of_resources++;
			}
			else {
				// Error occurred, remove the registered resource from MicroPython http server's context
				(void)httpd_unregister_uri(server_obj->server, uri.uri);
				nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Resource could not be added, error code: %d!", ret));
			}

			return resource;
		}
		else {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "URI must start with \"/\"."));
			// Just to fulfills the compiler's needs
			return mp_const_none;
		}

	}
	else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "HTTP Server module is not initialized!"));
		// Just to fulfills the compiler's needs
		return mp_const_none;
	}
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_server_add_resource_obj, 1, mod_http_server_add_resource);

// Get a resource from http server module
STATIC mp_obj_t mod_http_server_get_resource(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_server_get_resource_args[] = {
			{ MP_QSTR_uri,                     MP_ARG_OBJ  | MP_ARG_REQUIRED, },

	};

	if(server_initialized == true) {

		mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_server_get_resource_args)];
		mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_server_get_resource_args, args);

		httpd_uri_t uri;

		// Get the URI
		uri.uri = mp_obj_str_get_str(args[0].u_obj);

		if(find_resource(uri.uri) == NULL) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Resource is not exists!"));
		}
		return find_resource(uri.uri);
	}
	else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "HTTP Server module is not initialized!"));
		// Just to fulfills the compiler's needs
		return mp_const_none;
	}
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_server_get_resource_obj, 1, mod_http_server_get_resource);

// Get a resource from http server module
STATIC mp_obj_t mod_http_server_list_resource(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_server_list_resource_args[] = {
			{ MP_QSTR_uri,                     MP_ARG_OBJ},
	};

	if(server_initialized == true) {

		mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_server_list_resource_args)];
		mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_server_list_resource_args, args);

		httpd_uri_t uri;

		// Get the URI
		uri.uri = (args[0].u_obj != MP_OBJ_NULL) ? mp_obj_str_get_str(args[0].u_obj) : "/";

		// Maximum 100 resources can be listed
		mp_obj_t tuple[100];

		// Initiate number of resources with 0
		int num_of_res = 0;

		if(server_obj->resources != NULL) {
			mod_http_resource_obj_t* current = server_obj->resources;
			for(; current != NULL; current = current->next) {
				// Match found
				//printf("current: %s, uri: %s, len: %d\n", current)
				if(strncmp(current->uri, uri.uri, strlen(uri.uri)) == 0) {
					// Append uri string to list
					tuple[num_of_res] = mp_obj_new_str(current->uri, strlen(current->uri));
					num_of_res++;
				}
			}
		}
		return mp_obj_new_tuple(num_of_res, tuple);
	}
	else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "HTTP Server module is not initialized!"));
		// Just to fulfills the compiler's needs
		return mp_const_none;
	}
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_server_list_resource_obj, 0, mod_http_server_list_resource);

// Removes a resource from the http server module
STATIC mp_obj_t mod_http_server_remove_resource(mp_obj_t uri_in) {

	if(server_initialized == true) {

		const char* uri = mp_obj_str_get_str(uri_in);

		if(find_resource(uri) == NULL) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Resource does not exist!"));
		}

		esp_err_t ret = httpd_unregister_uri(server_obj->server, uri);
		if(ret != ESP_OK) {
			nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Resource could not be removed, error code: %d!", ret));
		}

		remove_resource(uri);
		server_obj->num_of_resources--;
	}
	else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "HTTP Server module is not initialized!"));
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_http_server_remove_resource_obj, mod_http_server_remove_resource);


STATIC const mp_map_elem_t mod_http_server_globals_table[] = {
		{ MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_HTTP_Server) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_http_server_init_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                          (mp_obj_t)&mod_http_server_deinit_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_add_resource),                    (mp_obj_t)&mod_http_server_add_resource_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_remove_resource),                 (mp_obj_t)&mod_http_server_remove_resource_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_get_resource),                    (mp_obj_t)&mod_http_server_get_resource_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_list_resource),                   (mp_obj_t)&mod_http_server_list_resource_obj },

		// class constants
		{ MP_OBJ_NEW_QSTR(MP_QSTR_GET),                      MP_OBJ_NEW_SMALL_INT(MOD_HTTP_GET) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_PUT),                      MP_OBJ_NEW_SMALL_INT(MOD_HTTP_PUT) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_POST),                     MP_OBJ_NEW_SMALL_INT(MOD_HTTP_POST) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_DELETE),                   MP_OBJ_NEW_SMALL_INT(MOD_HTTP_DELETE) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_TEXT),                     MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_TEXT_HTML_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_XML),                      MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_TEXT_XML_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_PLAIN),                    MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_TEXT_PLAIN_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_JSON),                     MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_APP_JSON_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_OCTET),                    MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_APP_OCTET_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_APP_XML),                  MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_APP_XML_ID) },

};

STATIC MP_DEFINE_CONST_DICT(mod_http_server_globals, mod_http_server_globals_table);

const mp_obj_module_t mod_http_server = {
		.base = { &mp_type_module },
		.globals = (mp_obj_dict_t*)&mod_http_server_globals,
};

/******************************************************************************
 DEFINE HTTP CLIENT CLASS FUNCTIONS
 ******************************************************************************/

STATIC mp_obj_t mod_http_client_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_client_init_args[] = {
			{ MP_QSTR_url,                     MP_ARG_OBJ  | MP_ARG_REQUIRED, },
			{ MP_QSTR_callback,                MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL} },
			{ MP_QSTR_auth,         		   MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL} },
	};

	mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_client_init_args)];
	mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_client_init_args, args);

	// Init client object
	const char* url = mp_obj_str_get_str(args[0].u_obj);

	// URL must start with "http://" or "https://" and have at least one more character except "/", otherwise it crash
	if(strlen(url) > 7 && strncmp("http://", url, 7) == 0 && url[7] != '/') {
		//HTTP over TCP
		client_config.url = url;
		client_config.transport_type = HTTP_TRANSPORT_OVER_TCP;
	}
	else if(strlen(url) > 8 && strncmp("https://", url, 8) == 0 && url[8] != '/') {
		//HTTP over SSL
		client_config.url = url;
		client_config.transport_type = HTTP_TRANSPORT_OVER_SSL;
	}
	else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "URL must start with \"http://\" or \"https://\""));
	}

	// Set python callback
	if(args[1].u_obj != MP_OBJ_NULL) {
		// Set callback and Event Handler if argument is used
		client_callback = args[1].u_obj;
		client_config.event_handler = _http_event_handle;
	} else {
		// Reset callback and Event Handler if argument is not used
		memset(&client_callback, 0, sizeof(client_callback));
		memset(&client_config.event_handler, 0, sizeof(client_config.event_handler));
	}

	// Set Authentication User and Password fields
	if (args[2].u_obj != MP_OBJ_NULL) {

		// Expect a tuple with 2 elements
		mp_obj_t *sec;
		mp_obj_get_array_fixed_n(args[2].u_obj, 2, &sec);

		// Get user and pass from tuple
		const char *user = mp_obj_str_get_str(sec[0]);
		const char *pass = mp_obj_str_get_str(sec[1]);

		if (strlen(user) < 1 || strlen(pass) < 1) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Username and password was not correctly set."));
		}

		client_config.auth_type = HTTP_AUTH_TYPE_BASIC;
		client_config.username = user;
		client_config.password = pass;
	}
	else {
		// Reset auth type, username and password
		client_config.auth_type = HTTP_AUTH_TYPE_NONE;
		memset(&client_config.username, 0, sizeof(client_config.username));
		memset(&client_config.password, 0, sizeof(client_config.password));
	}

	// Initiate Client Object
	client_obj = esp_http_client_init(&client_config);

	return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_client_init_obj, 0, mod_http_client_init);

STATIC mp_obj_t mod_http_client_deinit(void) {

	// Check if Client is already initiated
	if(client_config.url == NULL) {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Client is not initialized!"));
	}

	// Cleanup Client
	if(client_obj != NULL) {
		esp_http_client_cleanup(client_obj);
	}

	// Reinit Client Config
	memset(&client_config, 0, sizeof(client_config));
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_http_client_deinit_obj, mod_http_client_deinit);

// Sets or removes the callback on a given method
STATIC mp_obj_t mod_http_client_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_client_callback_args[] = {
			{ MP_QSTR_callback,                MP_ARG_OBJ  | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL}},
			{ MP_QSTR_action,                  MP_ARG_BOOL | MP_ARG_KW_ONLY,  {.u_bool = true}},
	};
	mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_client_callback_args)];
	mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_client_callback_args, args);

	// Set method
	client_callback = args[0].u_obj;

	if(args[1].u_bool) {
		client_config.event_handler = _http_event_handle;
	} else {
		// If action is false, remove client callback
		memset(&client_callback, 0, sizeof(client_callback));
		memset(&client_config.event_handler, 0, sizeof(client_config.event_handler));
	}

	// Init is needed to run -> TODO: Would be good to solve callback registration without init
	client_obj = esp_http_client_init(&client_config);
	return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_client_callback_obj, 0, mod_http_client_callback);

STATIC mp_obj_t mod_http_client_send_request(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_client_send_request_args[] = {
			{ MP_QSTR_method,                   MP_ARG_INT  | MP_ARG_KW_ONLY, {.u_int = MOD_HTTP_GET} },
			{ MP_QSTR_body,                     MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
			{ MP_QSTR_content_type,             MP_ARG_INT  | MP_ARG_KW_ONLY, {.u_int = MOD_HTTP_MEDIA_TYPE_TEXT_HTML_ID}},
			{ MP_QSTR_accept,                   MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
			{ MP_QSTR_user_agent,               MP_ARG_OBJ  | MP_ARG_KW_ONLY, {.u_obj = MP_OBJ_NULL}},
	};
	mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_client_send_request_args)];
	mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_client_send_request_args, args);

	if(client_config.url == NULL) {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Client was not initialized with URL!"));
	}

	// Set client's method
	if(args[0].u_int==MOD_HTTP_GET) {
		esp_http_client_set_method(client_obj, HTTP_METHOD_GET);
	}
	else if(args[0].u_int==MOD_HTTP_POST) {
		esp_http_client_set_method(client_obj, HTTP_METHOD_POST);
	}
	else if(args[0].u_int==MOD_HTTP_PUT) {
		esp_http_client_set_method(client_obj, HTTP_METHOD_PUT);
	}
	else if(args[0].u_int==MOD_HTTP_DELETE) {
		esp_http_client_set_method(client_obj, HTTP_METHOD_DELETE);
	}
	else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "This method is not allowed."));
	}

	// Set Request body
	if (args[1].u_obj != MP_OBJ_NULL) {
		esp_http_client_set_post_field(client_obj, mp_obj_str_get_str(args[1].u_obj), strlen(mp_obj_str_get_str(args[1].u_obj)));
	} else {
		esp_http_client_set_post_field(client_obj, "", 0);
	}

	// Set Content-Type Header
	esp_http_client_set_header(client_obj, "Content-Type", mod_http_mediatype[args[2].u_int]);

	// Set Accept Header
	if (args[3].u_obj != MP_OBJ_NULL) {
		esp_http_client_set_header(client_obj, "Accept", mod_http_mediatype[mp_obj_get_int(args[3].u_obj)]);
	} else {
		esp_http_client_delete_header(client_obj, "Accept");
	}

	// Set User-Agent Header
	if (args[4].u_obj != MP_OBJ_NULL) {
		esp_http_client_set_header(client_obj, "User-Agent", mp_obj_str_get_str(args[4].u_obj));
	} else {
		esp_http_client_set_header(client_obj, "User-Agent", MOD_DEFAULT_HTTP_USER_AGENT);
	}

	// Perform request
	esp_err_t err = esp_http_client_perform(client_obj);
	free(client_obj->request);

	if (err != ESP_OK) {
		// Cleanup Client - needed according to esp-idf documentation after every request, but its too slow
		// in case of HTTPS handshake
		esp_http_client_cleanup(client_obj);
		// Reinit Client
		client_obj = esp_http_client_init(&client_config);
		nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "Request could not sent."));
	}

	return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_client_send_request_obj, 0, mod_http_client_send_request);

// Gets or sets the Client URL
STATIC mp_obj_t mod_http_client_url(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_client_callback_args[] = {
			{ MP_QSTR_url,                MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
	};
	mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_client_callback_args)];
	mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_client_callback_args, args);

	if(args[0].u_obj != MP_OBJ_NULL) {
		// Init client object
		const char* url = mp_obj_str_get_str(args[0].u_obj);

		// URL must start with "http://" or "https://" and have at least one more character except "/", otherwise it crash
		if(strlen(url) > 7 && strncmp("http://", url, 7) == 0 && url[7] != '/') {
			//HTTP over TCP
			client_config.url = url;
			client_config.transport_type = HTTP_TRANSPORT_OVER_TCP;
		}
		else if(strlen(url) > 8 && strncmp("https://", url, 8) == 0 && url[8] != '/') {
			//HTTP over SSL
			client_config.url = url;
			client_config.transport_type = HTTP_TRANSPORT_OVER_SSL;
		}
		else {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "URL must start with \"http://\" or \"https://\""));
		}

		client_obj = esp_http_client_init(&client_config);
		return mp_const_none;
	}
	else {
		if(client_config.url == NULL) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "URL has not yet been set."));
		}
		char* url = mp_obj_new_str(client_config.url, strlen(client_config.url));
		return url;
	}
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_client_url_obj, 0, mod_http_client_url);

// Gets or sets the Client URL
STATIC mp_obj_t mod_http_client_auth(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

	const mp_arg_t mod_http_client_auth_args[] = {
			{ MP_QSTR_user,                MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
			{ MP_QSTR_pass,                MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
	};
	mp_arg_val_t args[MP_ARRAY_SIZE(mod_http_client_auth_args)];
	mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), mod_http_client_auth_args, args);

	// Any arguments are not defined
	if (args[0].u_obj == MP_OBJ_NULL && args[1].u_obj == MP_OBJ_NULL) {
		if(client_config.username == NULL && client_config.password == NULL) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Username and Password are not set."));
		}
		mp_obj_t tuple[2];
		tuple[0] = mp_obj_new_str(client_config.username, strlen(client_config.username));
		tuple[1] = mp_obj_new_str(client_config.password, strlen(client_config.username));
		return mp_obj_new_tuple(2, tuple);
	}
	else {
		if (args[0].u_obj == MP_OBJ_NULL || args[1].u_obj == MP_OBJ_NULL) {
			nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Username and Password must be set."));
		}
		// Get user and pass from tuple
		const char *user = mp_obj_str_get_str(args[0].u_obj);
		const char *pass = mp_obj_str_get_str(args[1].u_obj);

		if (strlen(user) == 0 && strlen(pass) == 0) {
			// Reset auth type, username and password
			client_config.auth_type = HTTP_AUTH_TYPE_NONE;
			memset(&client_config.username, 0, sizeof(client_config.username));
			memset(&client_config.password, 0, sizeof(client_config.password));
		} else if (strlen(user) < 1 || strlen(pass) < 1) {
			// Username and Password was not correctly set
			nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Username and Password must be set."));
		} else {
			// Set Auth
			client_config.auth_type = HTTP_AUTH_TYPE_BASIC;
			client_config.username = user;
			client_config.password = pass;
		}

		// Initiate Client Object
		client_obj = esp_http_client_init(&client_config);
	}

	return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_http_client_auth_obj, 0, mod_http_client_auth);

STATIC const mp_map_elem_t mod_http_client_globals_table[] = {
		{ MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_HTTP_Client) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_init),                            (mp_obj_t)&mod_http_client_init_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                          (mp_obj_t)&mod_http_client_deinit_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_callback),                        (mp_obj_t)&mod_http_client_callback_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_send_request),                    (mp_obj_t)&mod_http_client_send_request_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_url),                             (mp_obj_t)&mod_http_client_url_obj },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_auth),                            (mp_obj_t)&mod_http_client_auth_obj },

		// class constants
		{ MP_OBJ_NEW_QSTR(MP_QSTR_GET),                      		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_GET) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_PUT),                      		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_PUT) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_POST),                     		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_POST) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_DELETE),                   		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_DELETE) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_TEXT),                     		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_TEXT_HTML_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_XML),                      		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_TEXT_XML_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_PLAIN),                    		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_TEXT_PLAIN_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_JSON),                     		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_APP_JSON_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_OCTET),                    		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_APP_OCTET_ID) },
		{ MP_OBJ_NEW_QSTR(MP_QSTR_APP_XML),                  		MP_OBJ_NEW_SMALL_INT(MOD_HTTP_MEDIA_TYPE_APP_XML_ID) },
};

STATIC MP_DEFINE_CONST_DICT(mod_http_client_globals, mod_http_client_globals_table);

const mp_obj_module_t mod_http_client = {
		.base = { &mp_type_module },
		.globals = (mp_obj_dict_t*)&mod_http_client_globals,
};

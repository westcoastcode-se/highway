//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef HIW_SERVLET_H
#define HIW_SERVLET_H

#include "hiw_server.h"
#include "hiw_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

// maximum number of headers allowed by highway
#if !defined(HIW_MAX_HEADERS_COUNT)
#define HIW_MAX_HEADERS_COUNT (32)
#endif

// the max header size for a highway http request (8 kb)
#if !defined(HIW_MAX_HEADER_SIZE)
#define HIW_MAX_HEADER_SIZE (8 * 1024)
#endif

// should the server write out the server header
#if !defined(HIW_WRITE_SERVER_HEADER)
#define HIW_WRITE_SERVER_HEADER 1
#endif

// should the server version be written in the server header
#if !defined(HIW_WRITE_SERVER_VERSION)
#define HIW_WRITE_SERVER_VERSION 1
#endif

/**
 * A highway http header
 */
struct HIW_PUBLIC hiw_header
{
	// The header name
	hiw_string name;

	// The header value
	hiw_string value;
};
typedef struct hiw_header hiw_header;

/**
 * A highway http header
 */
struct HIW_PUBLIC hiw_headers
{
	// memory for all headers
	hiw_header headers[HIW_MAX_HEADERS_COUNT];

	// number of headers
	int count;
};

typedef struct hiw_headers hiw_headers;

/**
 * A highway http request
 */
struct HIW_PUBLIC hiw_request
{
	// The request method
	hiw_string method;

	// The request uri
	hiw_string uri;

	// Headers sent from the client
	hiw_headers headers;

	// content_length received from the client
	int content_length;
};

typedef struct hiw_request hiw_request;

/**
 * A highway http response
 */
struct HIW_PUBLIC hiw_response
{
	// Headers to be sent to the server
	hiw_headers headers;
};

typedef struct hiw_response hiw_response;
typedef struct hiw_filter_chain hiw_filter_chain;

// Function called when running the filter
HIW_PUBLIC typedef void (*hiw_filter_fn)(hiw_request*, hiw_response*, const hiw_filter_chain*);

/**
 * A highway filter
 */
struct HIW_PUBLIC hiw_filter
{
	// The function
	hiw_filter_fn func;

	// User-data associated with this filter
	void* data;
};

typedef struct hiw_filter hiw_filter;

/**
 * A highway filter chain
 */
struct HIW_PUBLIC hiw_filter_chain
{
	const hiw_filter* filters;
};

/**
 * Configuration
 */
struct HIW_PUBLIC hiw_servlet_config
{
	// number of threads
	int num_accept_threads;

	// Generic global user-data
	void* userdata;
};

typedef struct hiw_servlet_config hiw_servlet_config;

// the default port
#define HIW_SERVLET_DEFAULT_NUM_ACCEPT_THREADS (8)

// default configuration
#define hiw_servlet_config_default                                                                                     \
	(hiw_servlet_config) { .num_accept_threads = HIW_SERVLET_DEFAULT_NUM_ACCEPT_THREADS, .userdata = NULL }

typedef struct hiw_servlet_thread hiw_servlet_thread;

// A function definition for when a new servlet thread is spawned
typedef void (*hiw_servlet_start_fn)(hiw_servlet_thread*);

// A function definition for a servlet function
typedef void (*hiw_servlet_fn)(hiw_request*, hiw_response*);

typedef struct hiw_servlet hiw_servlet;

// A flag that the servlet owns the memory of the server
#define hiw_servlet_flags_server_owner (1 << 0)

enum HIW_PUBLIC hiw_servlet_error
{
	// No error
	HIW_SERVLET_ERROR_SUCCESS = 0,

	// One or more arguments are invalid
	HIW_SERVLET_ERROR_INVALID_ARGUMENT = 1,

	// An error occurred while starting the servlet threads
	HIW_SERVLET_ERROR_THREADS = 2
};

typedef enum hiw_servlet_error hiw_servlet_error;

/**
 * Create a new highway servlet. You can also use hiw_server_init if you already have
 * memory allocated for the server.
 *
 * @param server
 * @return
 */
HIW_PUBLIC extern hiw_servlet* hiw_servlet_new(hiw_server* server);

/**
 * Delete the memory allocated for a servlet
 *
 * @param s the servlet
 */
HIW_PUBLIC extern void hiw_servlet_delete(hiw_servlet* s);

/**
 * Set the filter chain for the servlet
 *
 * @param s the servlet
 * @param filters An array of all filters
 */
HIW_PUBLIC extern void hiw_servlet_set_filter_chain(hiw_servlet* s, const hiw_filter* filters);

/**
 * Set the servlet to be called for each request. This function is called at the end of each filter chain
 *
 * @param s the servlet
 * @param func the function
 */
HIW_PUBLIC extern void hiw_servlet_set_func(hiw_servlet* s, hiw_servlet_fn func);

/**
 * Set the starter function for a servlet. A default starter is used if this is not set.
 *
 * It is expected that you call hiw_servlet_start_filter_chain from within the started function
 *
 * @param s the servlet
 * @param func the function
 */
HIW_PUBLIC extern void hiw_servlet_set_starter_func(hiw_servlet* s, hiw_servlet_start_fn func);

/**
 * Start the servlet
 *
 * @param s the servlet
 * @param config
 * @return FALSE if the start sequence failed
 */
HIW_PUBLIC extern hiw_servlet_error hiw_servlet_start(hiw_servlet* s, const hiw_servlet_config* config);

/**
 * Get the user-data from the supplied filter in a filter chain
 *
 * @param filter the filter
 * @return user data associated with the supplied filter
 */
HIW_PUBLIC extern void* hiw_filter_get_data(const hiw_filter_chain* filter);

/**
 * Jump to the next filter in the chain
 *
 * @param s
 * @param filter
 */
HIW_PUBLIC extern void hiw_filter_chain_next(hiw_request* req, hiw_response* resp, const hiw_filter_chain* filter);

/**
 * Start the servlet filter chain
 *
 * @param st the highway servlet thread
 */
HIW_PUBLIC extern void hiw_servlet_start_filter_chain(hiw_servlet_thread* st);

/**
 * Get the thread associated with the supplied request
 *
 * @param req the request
 * @return the thread associated with the supplier request
 */
HIW_PUBLIC extern hiw_thread* hiw_request_get_thread(hiw_request* req);

/**
 * @brief Receive data from the supplier request
 * @param dest The destination buffer
 * @param n The number of bytes to read
 * @return The number of bytes read from the client
 */
HIW_PUBLIC extern int hiw_request_recv(hiw_request* req, char* dest, int n);

/**
 * @brief write a header to the supplier response
 * @param resp The response
 * @param header
 * @return true if writing the header was successful
 */
HIW_PUBLIC extern bool hiw_response_write_header(hiw_response* resp, hiw_header header);

/**
 * @brief write raw binary data to the client
 * @param resp The response
 * @param src The source buffer
 * @param n The number of bytes to send
 * @return true if writing the response body data was successful
 *
 * Please note that this will forcefully send all headers. If those are invalid, then the connection will forcefully
 * close
 */
HIW_PUBLIC extern bool hiw_response_write_body_raw(hiw_response* resp, const char* src, int n);

/**
 * @brief write the content-type header to the supplier response
 * @param resp The response
 * @param mime_type The mime type
 * @return true if writing the header was successful
 */
HIW_PUBLIC extern bool hiw_response_set_content_type(hiw_response* resp, hiw_string mime_type);

/**
 * @brief write a content-length header to the supplier response
 * @param resp The response
 * @param len The length
 * @return true if writing the header was successful
 */
HIW_PUBLIC extern bool hiw_response_set_content_length(hiw_response* resp, int len);

/**
 * @brief write the connection header to the supplier response with the value close (if value set to true)
 * @param resp The response
 * @param close If the connection should close or not
 * @return true if writing the header was successful
 */
HIW_PUBLIC extern bool hiw_response_set_connection_close(hiw_response* resp, bool close);

/**
 * @brief Set the status code
 * @param resp The response
 * @param status the status code
 * @return true if writing the header was successful
 */
HIW_PUBLIC extern bool hiw_response_set_status_code(hiw_response* resp, int status);

#ifdef __cplusplus
}
#endif

#endif // HIW_SERVLET_H

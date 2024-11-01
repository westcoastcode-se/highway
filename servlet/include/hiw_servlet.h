//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef hiw_SERVLET_H
#define hiw_SERVLET_H

#include "hiw_server.h"
#include "hiw_thread.h"

// maximum number of headers allowed by highway
#define HIW_MAX_HEADERS_COUNT (32)

// the max header size for a highway http request (8 kb)
#define HIW_MAX_HEADER_SIZE (8 * 1024)

/**
 * A wcc http header
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
 * A wcc http header
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
 * A wcc http request
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
 * A wcc http response
 */
struct HIW_PUBLIC hiw_response
{
	// Headers to be sent to the server
	hiw_headers headers;
};
typedef struct hiw_response hiw_response;

// Function called when running the filter
HIW_PUBLIC typedef void (* hiw_filter_fn)(hiw_request*, hiw_response*, const struct hiw_filter_chain*);

/**
 * A filter
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
 * A filter
 */
struct HIW_PUBLIC hiw_filter_chain
{
	const hiw_filter* filters;
};
typedef struct hiw_filter_chain hiw_filter_chain;

/**
 * Configuration
 */
struct HIW_PUBLIC hiw_servlet_config
{
	// number of threads
	int num_threads;
};
typedef struct hiw_servlet_config hiw_servlet_config;

// the default port
#define hiw_SERVLET_DEFAULT_NUM_THREADS (8)

// default configuration
#define hiw_servlet_config_default (hiw_servlet_config) { .num_threads = hiw_SERVLET_DEFAULT_NUM_THREADS }

// A function definition for when a new servlet thread is spawned
typedef void (* hiw_servlet_start_fn)(struct hiw_servlet_thread*);

// A function definition for a servlet function
typedef void (* hiw_servlet_fn)(hiw_request*, hiw_response*);

/**
 * The servlet is the entry-point of all http requests
 */
struct HIW_PUBLIC hiw_servlet
{
	// The filter chain used by all servlet threads
	hiw_filter_chain filter_chain;

	// Config
	hiw_servlet_config config;

	// The start function. Note that you are expected to call
	// hiw_servlet_thread_start(thread)
	hiw_servlet_start_fn start_func;

	// The threads
	struct hiw_servlet_thread* threads;

	// The underlying server socket
	hiw_server* server;

	// Flags associated with the servlet
	int flags;
};
typedef struct hiw_servlet hiw_servlet;

// A flag that the servlet owns the memory of the server
#define hiw_servlet_flags_server_owner (1 << 0)

/**
 * A servlet thread
 */
struct HIW_PUBLIC hiw_servlet_thread
{
	// The servlet
	hiw_servlet* servlet;

	// The thread this servlet
	hiw_thread* thread;

	// Filter chain running this servlet
	hiw_filter_chain filter_chain;

	// The next thread
	struct hiw_servlet_thread* next;
};
typedef struct hiw_servlet_thread hiw_servlet_thread;

enum HIW_PUBLIC hiw_servlet_error
{
	// No error
	hiw_SERVLET_ERROR_NO_ERROR = 0,

	// No servlet
	hiw_SERVLET_ERROR_NULL,

	// out of memory
	hiw_SERVLET_ERROR_OUT_OF_MEMORY,
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
 * Create a new highway servlet. You can also use hiw_server_init if you already have
 * memory allocated for the server.
 *
 * @param s
 * @param server
 * @return same value as s
 */
HIW_PUBLIC extern hiw_servlet* hiw_servlet_init(hiw_servlet* s, hiw_server* server);

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
 * Set the starter function for a servlet. A default starter is used if this is not set
 *
 * @param s the servlet
 * @param func the function
 */
HIW_PUBLIC extern void hiw_servlet_set_starter_func(hiw_servlet* s, hiw_servlet_start_fn func);

/**
 * Start the servlet
 *
 * @param s the servlet
 * @return FALSE if the start sequence failed
 */
HIW_PUBLIC extern hiw_servlet_error hiw_servlet_start(hiw_servlet* s);

/**
 * Release a servlets internal resources
 *
 * @param s the servlet
 */
HIW_PUBLIC extern void hiw_servlet_release(hiw_servlet* s);

/**
 * Get the user-data for the current filter in the filter chain
 *
 * @param filter
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
 * @param st
 */
HIW_PUBLIC extern void hiw_servlet_start_filter_chain(hiw_servlet_thread* st);

#endif //hiw_SERVLET_H

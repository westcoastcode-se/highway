//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_servlet.h"
#include "hiw_logger.h"
#include <assert.h>

struct hiw_internal_request {
	// public implementation
	hiw_request pub;

	// memory associated with the request's internal resources
	hiw_memory memory;

	// client associated with the request
	hiw_client* client;

	// thread associated with this request
	hiw_servlet_thread* thread;

	// The position of where read-ahead memory is located, if any is found
	char* read_ahead;

	// How much memory is available in the read-ahead memory buffer
	int read_ahead_length;

	// should the connection be opened for this request
	bool connection_close;

	// keep track of the expected number of bytes left to be read from the request socket
	int content_length;

	// How much of the content length is left to be read
	int content_length_remaining;
};
typedef struct hiw_internal_request hiw_internal_request;

// An error has occurred during the writing
#define hiw_internal_response_flag_error (1 << 0)

// flag that tells us that the headers are sent already
#define hiw_internal_response_flag_headers_sent (1 << 1)

// The content length header is set
#define hiw_internal_response_flag_content_length_set (1 << 2)

// The connection close header is set
#define hiw_internal_response_flag_connection_set (1 << 3)

// The status code is written
#define hiw_internal_response_flag_status_code_set (1 << 4)

struct hiw_internal_response {
	// public implementation
	hiw_response pub;

	// memory associated with the request's internal resources
	hiw_memory memory;

	// client associated with the request
	hiw_client* client;

	// thread associated with this request
	hiw_servlet_thread* thread;

	// flags used to keep track of the status of the response
	int flags;

	// The connection header is a little special. By default, the value will be the same as what the client sends in. The developer
	// can override this if they want. But, if the client is sending/streaming actually body data without setting this, then
	// the connection has to forcefully close for the client to know that they've gotten all the data
	bool connection_close;

	// the return status code
	int status_code;

	// The Content-Length header
	int content_length;

	// Keep track of the expected number of bytes left to be sent
	int content_bytes_left;
};
typedef struct hiw_internal_response hiw_internal_response;

bool hiw_internal_response_error(hiw_internal_response* r)
{
	r->flags |= hiw_internal_response_flag_error;
	return false;
}

bool hiw_internal_response_out_of_memory(hiw_internal_response* r)
{
	log_errorf("[t:%p][c:%p] out of memory", r->thread, r->client);
	r->flags |= hiw_internal_response_flag_error;
	return false;
}

/**
 * Initialize the request object
 *
 * @param req
 * @param buf
 * @param len
 */
void hiw_internal_request_init(hiw_internal_request* const req, hiw_servlet_thread* const thread, char* const buf, int len)
{
	hiw_memory_fixed_init(&req->memory, buf, len);
	req->thread = thread;
	req->client = NULL;
	req->connection_close = true;
}

/**
 * Initialize the response object
 *
 * @param resp
 * @param buf
 * @param len
 */
void hiw_internal_response_init(hiw_internal_response* const resp, hiw_servlet_thread* const thread, char* const buf, int len)
{
	hiw_memory_fixed_init(&resp->memory, buf, len);
	resp->thread = thread;
	resp->client = NULL;
	resp->connection_close = true;
}

/**
 * Reset a request to be used by a new client
 *
 * @param req
 * @param client
 */
void hiw_internal_request_reset(hiw_internal_request* req, hiw_client* client)
{
	req->client = client;
	req->pub.headers.count = 0;
	req->pub.uri.length = 0;
	req->pub.method.length = 0;
	req->pub.content_length = -1;
	req->read_ahead = req->memory.ptr;
	req->read_ahead_length = 0;
	req->connection_close = true;
	req->content_length = 0;
	req->content_length_remaining = 0;
	hiw_memory_reset(&req->memory);
}

/**
 * Reset the response for a new client
 *
 * @param resp
 * @param client
 */
void hiw_internal_response_reset(hiw_internal_response* resp, hiw_client* client)
{
	resp->pub.headers.count = 0;
	hiw_memory_reset(&resp->memory);
	resp->client = client;
	resp->flags = 0;
	resp->connection_close = true;
	resp->status_code = 0;
	resp->content_length = -1;
	resp->content_bytes_left = 0;
}

hiw_servlet* hiw_servlet_new(hiw_server* s)
{
	hiw_servlet* const st = (hiw_servlet*)malloc(sizeof(hiw_servlet));
	if (st == NULL)
	{
		log_error("out of memory");
		return NULL;
	}
	return hiw_servlet_init(st, s);
}

hiw_servlet* hiw_servlet_init(hiw_servlet* s, hiw_server* server)
{
	s->config = hiw_servlet_config_default;
	s->filter_chain.filters = NULL;
	s->start_func = NULL;
	s->server = server;
	s->flags = hiw_servlet_flags_server_owner;
	return s;
}

void hiw_servlet_delete(hiw_servlet* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	hiw_servlet_release(s);
	free(s);
}

void hiw_servlet_set_filter_chain(hiw_servlet* s, const hiw_filter* filters)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	s->filter_chain.filters = filters;
}

void hiw_servlet_set_func(hiw_servlet* s, hiw_servlet_fn func)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	s->servlet_func = func;
}

void hiw_servlet_set_starter_func(hiw_servlet* s, hiw_servlet_start_fn func)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	s->start_func = func;
}

void hiw_servlet_start_func_default(hiw_servlet_thread* st)
{
	hiw_servlet_start_filter_chain(st);
}

void hiw_servlet_func(hiw_thread* t)
{
	// The servlet thread
	hiw_servlet_thread* const st = (hiw_servlet_thread*)hiw_thread_get_userdata(t);

	// Start the actual servlet
	if (st->servlet->start_func != NULL)
		st->servlet->start_func(st);
	else
		hiw_servlet_start_func_default(st);
}

hiw_servlet_thread* hiw_servlet_thread_new()
{
	hiw_servlet_thread* const st = (hiw_servlet_thread*)malloc(sizeof(hiw_servlet_thread));
	if (st == NULL)
	{
		log_error("out of memory");
		return NULL;
	}
	st->next = NULL;
	st->thread = hiw_thread_new(hiw_servlet_func);
	// attach the servlet_thread as user data so that we have access to it in the actual thread
	hiw_thread_set_userdata(st->thread, st);
	return st;
}

void hiw_servlet_thread_delete(hiw_servlet_thread* st)
{
	if (st == NULL)
		return;

	if (st->thread != NULL)
		hiw_thread_delete(st->thread);
	free(st);
}

hiw_servlet_error hiw_servlet_start(hiw_servlet* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return hiw_SERVLET_ERROR_NULL;

	log_infof("hiw_servlet(%p) is spawning %d threads", s, s->config.num_threads);

	// Initialize all servlet threads
	hiw_servlet_thread* first = NULL;
	hiw_servlet_thread* last = NULL;
	for (int i = 0; i < s->config.num_threads; ++i)
	{
		hiw_servlet_thread* st = hiw_servlet_thread_new();
		if (st == NULL)
		{
			log_error("out of memory");
			while (first)
			{
				hiw_servlet_thread* const next = first->next;
				hiw_servlet_thread_delete(first);
				first = next;
			}
			return hiw_SERVLET_ERROR_OUT_OF_MEMORY;
		}

		st->servlet = s;
		st->filter_chain = s->filter_chain;

		if (first == NULL)
		{
			first = st;
			last = st;
		}
		else
		{
			last->next = st;
			last = st;
		}
	}
	s->threads = first;

	// Start all servlet threads
	while (first != NULL)
	{
		log_infof("hiw_servlet(%p) is starting thread", s);
		if (!hiw_thread_start(first->thread))
		{
			log_errorf("failed to start servlet thread %lld", first->thread->id);
			// TODO: Cleanup memory?!!
		}
		first = first->next;
	}
	log_infof("hiw_servlet(%p) spawned %d threads", s, s->config.num_threads);

	// Start the main thread servlet
	hiw_servlet_thread main_servlet_thread = {
			.servlet = s,
			.thread = hiw_thread_main(),
			.filter_chain = s->filter_chain,
			.next = NULL};
	hiw_thread_set_userdata(main_servlet_thread.thread, &main_servlet_thread);
	hiw_thread_set_func(main_servlet_thread.thread, hiw_servlet_func);
	hiw_thread_start(main_servlet_thread.thread);

	return hiw_SERVLET_ERROR_NO_ERROR;
}

void hiw_servlet_release(hiw_servlet* s)
{
	log_infof("hiw_servlet(%p) is releasing is resources", s);
	hiw_server_stop(s->server);

	hiw_servlet_thread* first = s->threads;
	while (first)
	{
		hiw_servlet_thread* const next = first->next;
		hiw_servlet_thread_delete(first);
		first = next;
	}

	if (hiw_bit_test(s->flags, hiw_servlet_flags_server_owner))
		hiw_server_delete(s->server);
	s->server = NULL;
}

void* hiw_filter_get_data(const hiw_filter_chain* filter)
{
	return filter->filters->data;
}

void hiw_filter_chain_next(hiw_request* req, hiw_response* resp, const hiw_filter_chain* filter)
{
	hiw_internal_request* const impl = (hiw_internal_request*)req;
	log_debugf("[t:%p][c:%p] next filter", impl->thread, impl->client);

	const hiw_filter* next = filter->filters + 1;
	if (next->func != NULL)
	{
		const hiw_filter_chain next_chain = {.filters = next};
		next->func(req, resp, &next_chain);
	}
	else
	{
		impl->thread->servlet->servlet_func(req, resp);
	}
}

bool hiw_internal_request_parse_status_line(hiw_internal_request* req, hiw_string line)
{
	// Check to see if the status line is valid
	if (line.length == 0)
	{
		log_infof("[t:%p][c:%p] faulty request: status line is missing in request", req->thread, req->client);
		return false;
	}

	hiw_string method_path_version[3];
	if (hiw_string_split(&line, ' ', method_path_version, 3) != 3)
	{
		log_infof("[t:%p][c:%p] invalid HTTP request status line", req->thread, req->client);
		return false;
	}

	const hiw_string http_version = hiw_string_rtrim(method_path_version[2]);
	if (!hiw_string_cmpc(http_version, "HTTP/1.1", 8))
	{
		log_infof("[t:%p][c:%p] received an unsupported HTTP version %.*s", req->thread, req->client,
				  http_version.length, http_version.begin);
		return false;
	}

	req->pub.method = hiw_string_rtrim(method_path_version[0]);
	req->pub.uri = hiw_string_rtrim(method_path_version[1]);
	return true;
}

bool hiw_internal_request_read_headers(hiw_internal_request* req)
{
	log_infof("[t:%p][c:%p] reading headers", req->thread, req->client);

	req->pub.headers.count = 0;

	// headers are read in the following way:
	//
	// 1. Read a chunk of data
	// 2. Try to parse as much as possible, until we get eof or an empty row
	// 3. If eof, then receive more data and parse chunks again
	// 4. If we've reached the maximum allowed of header bytes from client then forcefully disconnect the client
	// 5. If empty row then assume that we're reading the data now
	// 6. If we've partially read the string content then copy that content into the request buffer
	//    so that the one reading the buffer can read it in a sequential order

	hiw_memory* const memory = &req->memory;
	int bytes_read = 0;
	char* pos = hiw_memory_get(memory, HIW_MAX_HEADER_SIZE);
	hiw_string status_line;

	// Read a chunk of data, with the maximum of MAX_HEADER_SIZE bytes
	while (1)
	{
		const int count = hiw_client_recv(req->client, pos + bytes_read, HIW_MAX_HEADER_SIZE - bytes_read);
		if (count <= 0)
		{
			log_infof("[t:%p][c:%p] client closed connection", req->thread, req->client);
			return false;
		}

		bytes_read += count;

		// try to read the status line, which is in the beginning of the http request
		if (hiw_string_readline(&(hiw_string){.begin = memory->ptr, .length = count}, &status_line))
			break;

		// if we've read as much data the header is allowed to read from, and we still haven't gotten a
		// line, then assume that the status line is invalid
		if (bytes_read >= HIW_MAX_HEADER_SIZE)
		{
			log_infof("[t:%p][c:%p] invalid status line header", req->thread, req->client);
			return false;
		}
	}

	// try to parse the status line, which is the first line in the request buffer
	if (!hiw_internal_request_parse_status_line(req, status_line))
		return false;

	// special case, in which the status line is the entire allowed buffer. This can happen
	// if the URI is very long
	if (bytes_read >= HIW_MAX_HEADER_SIZE)
	{
		log_warnf("[t:%p][c:%p] a very long uri was received", req->thread, req->client);
		return true;
	}

	// move the memory cursor to after the status line (and the new line character)
	pos = pos + status_line.length + 1;

	// now, let's read the rest of the data until we've read the entire header (we've reached the header-body separator)
	bool header_connection_close = req->connection_close;
	int header_content_length = req->content_length;
	while (1)
	{
		int bytes_left = bytes_read - (int)(pos - memory->ptr);
		hiw_string line;
		while (hiw_string_readline(&(hiw_string){.begin = pos, .length = bytes_left}, &line))
		{
			// verify if we've received too many headers
			if (req->pub.headers.count > HIW_MAX_HEADERS_COUNT)
			{
				log_warnf("[t:%p][c:%p] sending more headers than %d", req->thread, req->client, HIW_MAX_HEADERS_COUNT);
				return false;
			}

			// parse and read the header
			hiw_string name_value[2];
			hiw_header* const header = &req->pub.headers.headers[req->pub.headers.count];
			const int num_parts = hiw_string_split(&line, ':', name_value, 2);
			if (num_parts != 2)
			{
				// This is, either, a faulty http header or the header-body separator (\r\n) and we've
				// read to much data from the socket. It has read ahead
				if (line.length <= 1)
				{
					// Skip any CRLF
					if (bytes_left > 0 && *pos == '\r')
					{
						pos++;
						bytes_left--;
					}

					if (bytes_left > 0 && *pos == '\n')
					{
						pos++;
						bytes_left--;
					}

					req->read_ahead = pos;
					req->read_ahead_length = bytes_read - (int)(pos - memory->ptr);
					goto done;
				}

				log_errorf("[t:%p][c:%p] sent faulty header for string %.*s", req->thread, req->client, line.length, line.begin);
				return false;
			}

			req->pub.headers.count++;
			header->name = hiw_string_trim(name_value[0]);
			header->value = hiw_string_trim(name_value[1]);

			// "line.length" does not include the new-line character, this is why we put a + 1 at the end so
			// that that's included
			pos += line.length + 1;

			// also, keep track of the amount of bytes left to read
			bytes_left = bytes_read - (int)(pos - memory->ptr);

			// is this a connection header?
			if (hiw_string_cmpc(header->name, "Connection", 10))
			{
				header_connection_close = hiw_string_cmpc(header->value, "close", 5);
				continue;
			}

			// should we receive content from the client?
			if (hiw_string_cmpc(header->name, "Content-Length", 14))
			{
				hiw_string_toi(header->value, &req->pub.content_length);
				header_content_length = req->pub.content_length;
				continue;
			}
		}

		// okay, so here's what might happen next:
		// 1. We didn't read the entire header yet, so let's read the next until we've reached the
		//    header-body separator or
		// 2. We tried to read more than what the maximum header size allows for

		const int capacity_left = HIW_MAX_HEADER_SIZE - bytes_read;
		if (capacity_left <= 0)
		{
			log_errorf("[t:%p][c:%p] request's header-size is larger than the maximum allowed size of %d bytes",
					   req->thread, req->client, HIW_MAX_HEADER_SIZE);
			return false;
		}

		// read more data from the socket and try to parse the headers again
		assert(pos < memory->ptr + capacity_left);
		const int count = hiw_client_recv(req->client, pos, capacity_left);
		if (count <= 0)
		{
			log_errorf("[t:%p][c:%p] failed to read the rest of the data from client",
					   req->thread, req->client);
			return false;
		}
		bytes_read += count;
	}
done:
	req->connection_close = header_connection_close;
	req->content_length = header_content_length;
	req->content_length_remaining = req->content_length;
	return true;
}

bool hiw_internal_response_write_raw(hiw_internal_response* const resp, const char* src, int n)
{
	char* const dest = hiw_memory_get(&resp->memory, n);
	if (dest == NULL)
		return hiw_internal_response_error(resp);
	hiw_std_mempy(src, n, dest, n);
	return true;
}

/**
 * Flush all headers. This is really only used if the servlet isn't sending a body
 *
 * @param resp The response
 * @return true if headers was successfully flushed
 */
bool hiw_response_flush_headers(hiw_internal_response* const resp)
{
	// Headers are already sent?
	if (hiw_bit_test(resp->flags, hiw_internal_response_flag_headers_sent))
	{
		return true;
	}

	if (resp->status_code == 0)
	{
		log_errorf("[t:%p][c:%p] no status code has been set for the response", resp->thread, resp->client);
		return false;
	}

	// If no content-length is set then set it to 0
	if (!hiw_bit_test(resp->flags, hiw_internal_response_flag_content_length_set))
	{
		if (!hiw_response_set_content_length(&resp->pub, 0))
		{
			return hiw_internal_response_error(resp);
		}
	}

	// Set the connection response header if not set
	if (!hiw_bit_test(resp->flags, hiw_internal_response_flag_connection_set))
	{
		if (!hiw_response_set_connection_close(&resp->pub, resp->connection_close))
		{
			return hiw_internal_response_error(resp);
		}
	}

	// write the header body separator
	if (!hiw_internal_response_write_raw(resp, "\r\n", 2))
	{
		log_errorf("[t:%p][c:%p] could not write header body separator", resp->thread, resp->client);
		return hiw_internal_response_error(resp);
	}

	// Send the entire header block
	const int total_header_size = hiw_memory_size(&resp->memory);
	if (hiw_client_sendall(resp->client, resp->memory.ptr, total_header_size) != total_header_size)
	{
		log_errorf("[t:%p][c:%p] could not write all data to the client", resp->thread, resp->client);
		return hiw_internal_response_error(resp);
	}
	resp->flags |= hiw_internal_response_flag_headers_sent;
	return true;
}

void hiw_servlet_start_filter_chain(hiw_servlet_thread* st)
{
	// Memory used for parsing header data, both for the request and the response
	char request_stack_memory[HIW_MAX_HEADER_SIZE];
	char response_stack_memory[HIW_MAX_HEADER_SIZE];

	hiw_internal_request request;
	hiw_internal_request_init(&request, st, request_stack_memory, sizeof(request_stack_memory));
	hiw_internal_response response;
	hiw_internal_response_init(&response, st, response_stack_memory, sizeof(response_stack_memory));

	// allow the thread to be running for as long as the server is running
	while (hiw_server_is_running(st->servlet->server))
	{
		// Blocking accept for new clients. Makes use of the OS's interrupts instead of polling and having to
		// take race-condition into consideration. Will return NULL if:
		// 1. accept timeout happened, which is okay and can be ignored
		// 2. the server socket is closed, which only happens when the server is stopped
		// TODO: Consider reusing clients instead of malloc and free

		hiw_client* const client = hiw_server_accept(st->servlet->server);
		if (client == NULL)
			continue;// continue will go back up, and if the server is no longer running then exit!
		log_infof("[t:%p][c:%p] %s connected", request.thread, request.client, hiw_client_get_address(client));

	read_retry:
		hiw_internal_request_reset(&request, client);
		hiw_internal_response_reset(&response, client);

		// read all headers
		if (!hiw_internal_request_read_headers(&request))
		{
			response.connection_close = true;
			goto read_abort;
		}
		log_infof("[t:%p][c:%p] %.*s %.*s", request.thread, request.client,
				  request.pub.method.length, request.pub.method.begin, request.pub.uri.length, request.pub.uri.begin);

		response.connection_close = request.connection_close;

		// Iterate over all filters and then, eventually, get to the actual servlet function!
		if (st->filter_chain.filters != NULL)
			st->filter_chain.filters->func(&request.pub, &response.pub, &st->filter_chain);
		else if (st->servlet->servlet_func != NULL)
			st->servlet->servlet_func(&request.pub, &response.pub);

		// Verify that we've read all content from the client. If not, then the client sent a Content-Length header
		// that's larger than what the servlet read
		if (request.content_length_remaining > 0)
		{
			log_errorf("[t:%p][c:%p] client sent %d bytes but only read %d bytes, connection will forcefully close",
					   request.thread, request.client, request.content_length, request.content_length_remaining);
			response.connection_close = true;
			goto read_abort;
		}

		// Flush headers if they aren't flushed already
		if (!hiw_response_flush_headers(&response))
		{
			response.connection_close = true;
			goto read_abort;
		}

		// If we haven't written all the memory to the client, then warn about it and then close the connection
		if (response.content_bytes_left > 0)
		{
			log_errorf("[t:%p][c:%p] bytes left to write to the client, connection will forcefully close", request.thread, request.client);
			response.connection_close = true;
			goto read_abort;
		}

		// reuse connection if possible
		if (!response.connection_close)
			goto read_retry;

	read_abort:
		log_infof("[t:%p][c:%p] disconnected", request.thread, request.client);
		hiw_client_delete(client);
	}
}

int hiw_request_recv(hiw_request* req, char* dest, int n)
{
	hiw_internal_request* const impl = (hiw_internal_request*)req;

	// content length is mandatory if the library should allow reading data from a request
	if (impl->content_length == 0)
		return -1;

	// no more content to be read
	if (impl->content_length_remaining <= 0)
		return -1;

	// clamp the requested bytes to the content-length
	n = n > impl->content_length_remaining ? impl->content_length_remaining : n;
	if (n <= 0)
		return -1;

	int result = 0;

	// Copy read-ahead data first
	if (impl->read_ahead_length > 0)
	{
		// copy memory from the read ahead buffer
		const int read_bytes = impl->read_ahead_length > n ? n : impl->read_ahead_length;
		hiw_std_mempy(impl->read_ahead, read_bytes, dest, n);
		impl->read_ahead_length -= read_bytes;
		impl->read_ahead += read_bytes;
		result = read_bytes;
		n -= read_bytes;
	}

	// is there more data that's requested to be read from the client
	if (n > 0)
	{
		const int ret = hiw_client_recv(impl->client, dest, n);
		if (ret == -1)
			return -1;
		result += ret;
	}

	impl->content_length_remaining -= result;
	return result;
}

bool hiw_response_write_status_code(hiw_internal_response* resp)
{
	int len = hiw_string_const_len("HTTP/1.1 ");
	char* buf = hiw_memory_get(&resp->memory, len);
	if (buf == NULL)
		return hiw_internal_response_out_of_memory(resp);
	hiw_std_mempy("HTTP/1.1 ", len, buf, len);

	switch (resp->status_code)
	{
	case 200:
		len = hiw_string_const_len("200 OK\r\n");
		buf = hiw_memory_get(&resp->memory, len);
		if (buf == NULL)
			return hiw_internal_response_out_of_memory(resp);
		hiw_std_mempy("200 OK\r\n", len, buf, len);
		break;

	case 404:
		len = hiw_string_const_len("404 Not Found\r\n");
		buf = hiw_memory_get(&resp->memory, len);
		if (buf == NULL)
			return hiw_internal_response_out_of_memory(resp);
		hiw_std_mempy("404 Not Found\r\n", len, buf, len);
		break;
	case 418:
	default:
		len = hiw_string_const_len("418 I'm a teapot\r\n");
		buf = hiw_memory_get(&resp->memory, len);
		if (buf == NULL)
			return hiw_internal_response_out_of_memory(resp);
		hiw_std_mempy("418 I'm a teapot\r\n", len, buf, len);
		break;
	}
	return true;
}

bool hiw_response_write_header(hiw_response* const resp, const hiw_header header)
{
	assert(resp != NULL);
	if (resp == NULL)
		return false;
	hiw_internal_response* const impl = (hiw_internal_response*)resp;

	if (hiw_bit_test(impl->flags, hiw_internal_response_flag_headers_sent))
	{
		log_errorf("[t:%p][c:%p] headers already sent to client", impl->thread, impl->client);
		return hiw_internal_response_error(impl);
	}

	if (resp->headers.count >= HIW_MAX_HEADERS_COUNT)
	{
		log_errorf("[t:%p][c:%p] trying to add more headers than %d in the response", impl->thread, impl->client, HIW_MAX_HEADERS_COUNT);
		return hiw_internal_response_error(impl);
	}

	// verify that the header is not added to the response already
	for (int i = 0; i < resp->headers.count; ++i)
	{
		if (hiw_string_cmp(resp->headers.headers[i].name, header.name))
		{
			log_errorf("[t:%p][c:%p] header %.*s is already added", impl->thread, impl->client,
					   resp->headers.headers[i].name.length, resp->headers.headers[i].name.begin);
			return hiw_internal_response_error(impl);
		}
	}

	// status code must be set
	if (impl->status_code == 0)
	{
		log_errorf("[t:%p][c:%p] status code is not set", impl->thread, impl->client);
		return hiw_internal_response_error(impl);
	}

	// write the status code
	if (!hiw_bit_test(impl->flags, hiw_internal_response_flag_status_code_set))
	{
		if (!hiw_response_write_status_code(impl))
		{
			log_errorf("[t:%p][c:%p] could not write status code", impl->thread, impl->client);
			return hiw_internal_response_error(impl);
		}

		impl->flags |= hiw_internal_response_flag_status_code_set;
	}

	// write the header name to the buffer
	char* dest = hiw_memory_get(&impl->memory, header.name.length);
	if (dest == NULL)
		return hiw_internal_response_out_of_memory(impl);
	hiw_std_mempy(header.name.begin, header.name.length, dest, header.name.length);

	// write ": "
	dest = hiw_memory_get(&impl->memory, 2);
	if (dest == NULL)
		return hiw_internal_response_out_of_memory(impl);
	*dest++ = ':';
	*dest++ = ' ';

	// write the value + CRLF
	dest = hiw_memory_get(&impl->memory, header.value.length + 2);
	if (dest == NULL)
		return hiw_internal_response_out_of_memory(impl);
	dest = hiw_std_mempy(header.value.begin, header.value.length, dest, header.value.length);
	*dest++ = '\r';
	*dest++ = '\n';

	// Set the header in the response header cache
	hiw_header* cached_header = &resp->headers.headers[resp->headers.count++];
	hiw_string_set(&cached_header->name, header.name.begin, header.name.length);
	hiw_string_set(&cached_header->value, header.value.begin, header.value.length);
	return true;
}

bool hiw_response_write_body_raw(hiw_response* resp, const char* src, int n)
{
	assert(resp != NULL);
	if (resp == NULL)
		return false;
	hiw_internal_response* const impl = (hiw_internal_response*)resp;

	// Flush all headers before sending the first data to the client
	if (!hiw_bit_test(impl->flags, hiw_internal_response_flag_headers_sent))
	{
		// Not setting the content-length is actually allowed, but handle it in a special way
		if (!hiw_bit_test(impl->flags, hiw_internal_response_flag_content_length_set))
		{
			// force the connection to close when the servlet is done writing data to the client
			if (!hiw_response_set_connection_close(&impl->pub, true))
				return hiw_internal_response_error(impl);
		}

		// flush all headers, if not already done so
		if (!hiw_response_flush_headers(impl))
			return hiw_internal_response_error(impl);
	}

	// Send the raw data to the client
	const int sent = hiw_client_sendall(impl->client, src, n);
	if (sent != n)
	{
		log_errorf("[t:%p][c:%p] failed to send all data to the client", impl->thread, impl->client);
		return hiw_internal_response_error(impl);
	}

	if (impl->content_length > 0)
	{
		if (impl->content_bytes_left > 0)
			impl->content_bytes_left -= n;

		if (impl->content_bytes_left < 0)
		{
			log_errorf("[t:%p][c:%p] you're trying to send more data to the client than content-length %d allows", impl->thread, impl->client, impl->content_length);
			return hiw_internal_response_error(impl);
		}
	}

	return true;
}

bool hiw_response_set_content_type(hiw_response* const resp, const hiw_string mime_type)
{
	const hiw_string content_type_name = hiw_string_const("content-type");
	return hiw_response_write_header(resp, (hiw_header){.name = content_type_name, .value = mime_type});
}

bool hiw_response_set_content_length(hiw_response* resp, int len)
{
	// Only set this once
	hiw_internal_response* const impl = (hiw_internal_response*)resp;
	if (hiw_bit_test(impl->flags, hiw_internal_response_flag_content_length_set))
		return true;

	const hiw_string content_length_name = hiw_string_const("content-length");
	char temp[hiw_string_const_len("2147483647")];

	const int written_bytes = (int)(hiw_std_uitoc(temp, sizeof(temp), len) - temp);
	if (!hiw_response_write_header(resp, (hiw_header){.name = content_length_name, .value.begin = temp, .value.length = written_bytes}))
		return hiw_internal_response_error(impl);

	impl->content_length = len;
	impl->content_bytes_left = len;
	impl->flags |= hiw_internal_response_flag_content_length_set;
	return true;
}

bool hiw_response_set_connection_close(hiw_response* resp, bool close)
{
	// Only set this once
	hiw_internal_response* const impl = (hiw_internal_response*)resp;
	if (hiw_bit_test(impl->flags, hiw_internal_response_flag_connection_set))
		return true;

	const hiw_string connection_name = hiw_string_const("connection");
	const hiw_string connection_close_keep_alive = hiw_string_const("keep-alive");
	const hiw_string connection_close_close = hiw_string_const("close");

	if (close)
	{
		if (!hiw_response_write_header(resp, (hiw_header){.name = connection_name, .value = connection_close_close}))
		{
			return hiw_internal_response_error(impl);
		}
	}
	else
	{
		if (!hiw_response_write_header(resp, (hiw_header){.name = connection_name, .value = connection_close_keep_alive}))
		{
			return hiw_internal_response_error(impl);
		}
	}

	impl->flags |= hiw_internal_response_flag_connection_set;
	impl->connection_close = close;
	return true;
}

bool hiw_response_set_status_code(hiw_response* resp, int status)
{
	assert(resp != NULL && "expected 'resp' to exist");
	if (resp == NULL)
		return false;

	hiw_internal_response* const impl = (hiw_internal_response*)resp;
	if (hiw_bit_test(impl->flags, hiw_internal_response_flag_headers_sent))
	{
		log_errorf("[t:%p][c:%p] cannot set status code when header is already sent to client", impl->thread, impl->client);
		return hiw_internal_response_error(impl);
	}

	if (hiw_bit_test(impl->flags, hiw_internal_response_flag_status_code_set))
	{
		log_errorf("[t:%p][c:%p] cannot set status code header is already written", impl->thread, impl->client);
		return hiw_internal_response_error(impl);
	}

	impl->status_code = status;
	return true;
}

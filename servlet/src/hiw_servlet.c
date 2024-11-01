//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_servlet.h"
#include "hiw_logger.h"
#include <assert.h>

struct hiw_internal_request
{
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
	int content_bytes_left;
};
typedef struct hiw_internal_request hiw_internal_request;

struct hiw_internal_response
{
	// public implementation
	hiw_response pub;

	// memory associated with the request's internal resources
	hiw_memory memory;

	// client associated with the request
	hiw_client* client;

	// thread associated with this request
	hiw_servlet_thread* thread;

	// should the connection be reused. The response might force the client connecton to be closed
	// if, for example, the application writes data without telling the client about it's Content-Length
	bool connection_close;

	// the return status code
	int status_code;

	// Known Content-Length
	int content_length;

	// Keep track of the expected number of bytes left to be sent
	int content_bytes_left;
};
typedef struct hiw_internal_response hiw_internal_response;

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
	req->connection_close = false;
	req->content_bytes_left = -1;
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
	resp->connection_close = false;
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
			.next = NULL
	};
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
	log_debug("next filter");

	const hiw_filter* next = filter->filters + 1;
	if (next->func != NULL)
	{
		const hiw_filter_chain next_chain = { .filters = next };
		next->func(req, resp, &next_chain);
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
	while (bytes_read < HIW_MAX_HEADER_SIZE)
	{
		const int count = hiw_client_recv(req->client, pos + bytes_read, HIW_MAX_HEADER_SIZE - bytes_read);
		if (count <= 0)
		{
			log_infof("[t:%p][c:%p] client closed connection", req->thread, req->client);
			return false;
		}

		bytes_read += count;

		// try to read the status line, which is in the beginning of the http request
		if (hiw_string_readline(&(hiw_string) { .begin = memory->ptr, .length = count }, &status_line))
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
	int header_content_length = req->content_bytes_left;
	while (1)
	{
		int bytes_left = bytes_read - (int)(pos - memory->ptr);
		hiw_string line;
		while (hiw_string_readline(&(hiw_string){.begin=pos,.length=bytes_left}, &line))
		{
			// verify if we've received too many headers
			if (req->pub.headers.count > HIW_MAX_HEADERS_COUNT)
			{
				log_warnf("[%p] %s sending more headers than %d", req->client,
						hiw_client_get_address(req->client), HIW_MAX_HEADERS_COUNT);
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

				log_errorf("[%p] %s sent faulty header for string %.*s", req->client,
						hiw_client_get_address(req->client),
						line.length, line.begin);
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
			log_errorf("[%p] %s request's header-size is larger than the maximum allowed size of %d bytes", req->client,
					hiw_client_get_address(req->client), HIW_MAX_HEADER_SIZE);
			return false;
		}

		// read more data from the socket and try to parse the headers again
		assert(pos < memory->ptr + capacity_left);
		const int count = hiw_client_recv(req->client, pos, capacity_left);
		if (count <= 0)
		{
			log_errorf("[%p] %s failed to read the rest of the data from client", req->client,
					hiw_client_get_address(req->client));
			return false;
		}
		bytes_read += count;
	}
done:
	req->connection_close = header_connection_close;
	req->content_bytes_left = header_content_length;
	return true;
}

void hiw_servlet_start_filter_chain(hiw_servlet_thread* st)
{
	// Memory used for parsing header data, both for the request and the response
	char stack_memory[HIW_MAX_HEADER_SIZE];

	hiw_internal_request request;
	hiw_internal_request_init(&request, st, stack_memory, sizeof(stack_memory));
	hiw_internal_response response;
	hiw_internal_response_init(&response, st, stack_memory, sizeof(stack_memory));

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
			continue; // continue will go back up, and if the server is no longer running then exit!
		log_warnf("[t:%p][c:%p] %s connected", request.thread, request.client, hiw_client_get_address(client));

	read_retry:
		hiw_internal_request_reset(&request, client);
		hiw_internal_response_reset(&response, client);

		// read all headers
		if (!hiw_internal_request_read_headers(&request))
		{
			hiw_client_delete(client);
			continue;
		}
		log_warnf("[t:%p][c:%p] %.*s %.*s", request.thread, request.client,
			request.pub.method.length, request.pub.method.begin, request.pub.uri.length, request.pub.uri.begin);

		response.connection_close = request.connection_close;

		// Iterate over all filters and then, eventually, get to the actual servlet function!
		if (st->filter_chain.filters != NULL)
			st->filter_chain.filters->func(&request.pub, &response.pub, &st->filter_chain);

		// TODO flush headers, if they aren't flushed already

		// If we haven't written all the memory to the client, then warn about it and then close the connection
		if (response.content_bytes_left > 0)
		{
			log_errorf("[%p] %d bytes left to write to the client, connection will forcefully close", client, request.content_bytes_left);
			response.connection_close = true;
		}

		// reuse connection if possible
		if (!response.connection_close)
			goto read_retry;

		log_debugf("closing client(%p)", client);
		hiw_client_delete(client);
	}

}

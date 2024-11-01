//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "highway.h"
#include "hiw_servlet.h"
#include "static_cache.h"

#include <stdio.h>
#include <signal.h>

// server
hiw_server* server;
// static cache
static_cache cache;

void stop_server_on_signal(int sig)
{
	if (server != NULL && hiw_server_is_running(server))
	{
		hiw_server_stop(server);
	}
}

void serve_cached_content(hiw_request* req, hiw_response* resp)
{
	for(int i = 0; i < cache.content_count; ++i)
	{
		const static_content* const c = &cache.content[i];
		if (hiw_string_cmp(c->uri, req->uri))
		{
			hiw_response_set_status_code(resp, 200);
			hiw_response_set_content_length(resp, c->length);
			hiw_response_set_content_type(resp, c->mime_type);
			hiw_response_write_body_raw(resp, c->memory, c->length);
			return;
		}
	}

	hiw_response_set_status_code(resp, 404);
	hiw_response_set_content_length(resp, 23);
	hiw_response_set_content_type(resp, hiw_mimetypes.text_plain);
	hiw_response_write_body_raw(resp, "could not find resource", 23);
}

int start(hiw_string data_dir, int num_threads, int read_timeout, int write_timeout)
{
	log_infof("starting static content server from '%.*s'", data_dir.length, data_dir.begin);

	if (!static_cache_init(&cache, data_dir))
	{
		log_error("failed to initialize cache");
		return 1;
	}

	if (!hiw_init(hiw_init_config_default)) return 0;

	// start the server
	hiw_server_config server_config = hiw_server_config_default;
	server_config.socket_config.read_timeout = read_timeout;
	server_config.socket_config.write_timeout = write_timeout;
	server = hiw_server_new(&server_config);
	if (server == NULL)
	{
		log_error("failed to initialize server");
		goto end_start;
	}

	// Start the server socket
	hiw_server_start(server);
	hiw_servlet servlet = {};
	hiw_servlet_init(&servlet, server);
	servlet.config.num_threads = num_threads;
	hiw_servlet_set_func(&servlet, serve_cached_content);
	hiw_servlet_start(&servlet);

	// Release servlet resources
	hiw_servlet_release(&servlet);

	hiw_server_delete(server);
	server = NULL;

end_start:
	hiw_release();
	return 0;
}

int main(int argc, char** argv)
{
	signal(SIGINT, stop_server_on_signal);

	hiw_string data_dir = hiw_string_const("data");
	int num_threads = 50;
	int read_timeout = 5000;
	int write_timeout = 5000;

	if (argc < 2)
	{
		fprintf(stdout, "Usage: static [data-dir] [max-threads] [read-timeout] [write-timeout]");
		fprintf(stdout, "\n");
		return 1;
	}

	if (argc > 1)
	{
		data_dir.begin = argv[1];
		data_dir.length = strlen(argv[1]);
	}

	if (argc > 2)
	{
		num_threads = atoi(argv[2]);
	}

	if (argc > 3)
	{
		read_timeout = atoi(argv[3]);
	}

	if (argc > 4)
	{
		write_timeout = atoi(argv[4]);
	}

	return start(data_dir, num_threads, read_timeout, write_timeout);
}
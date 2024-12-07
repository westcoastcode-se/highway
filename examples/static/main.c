//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include <hiw_boot.h>
#include "static_cache.h"

#include <string.h>
#include <stdio.h>
#include <signal.h>

// static cache
static_cache cache;

void on_request(hiw_request* const req, hiw_response* const resp)
{
	for (int i = 0; i < cache.content_count; ++i)
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

int hiw_boot_init(hiw_boot_config* config)
{
	hiw_string data_dir = hiw_string_const("data");
	if (config->argc > 1)
	{
		if (hiw_string_cmpc(hiw_string_const("--help"), config->argv[1], (int)strlen(config->argv[1])))
		{
			fprintf(stdout, "Usage: static [data-dir] [max-threads] [read-timeout] [write-timeout]\n");
			fprintf(stdout, "\n");
			fprintf(stdout, "\tdata-dir - is the path to the data directory\n");
			fprintf(stdout, "\tmax-threads - is the maximum number of threads\n");
			fprintf(stdout, "\tread-timeout - is the read timeout in milliseconds\n");
			fprintf(stdout, "\twrite-timeout - is the write timeout in milliseconds\n");
			fprintf(stdout, "\n");
			return 0;
		}

		data_dir.begin = config->argv[1];
		data_dir.length = (int)strlen(config->argv[1]);
	}
	if (config->argc > 2)
		config->servlet_config.num_accept_threads = (int)strtol(config->argv[2], NULL, 10);
	if (config->argc > 3)
		config->server_config.socket_config.read_timeout = (int)strtol(config->argv[3], NULL, 10);
	if (config->argc > 4)
		config->server_config.socket_config.write_timeout = (int)strtol(config->argv[4], NULL, 10);
	config->servlet_func = on_request;

	if (!static_cache_init(&cache, data_dir))
	{
		log_error("failed to initialize cache");
		return 2;
	}

	const int ret = hiw_boot_start(config);
	static_cache_release(&cache);
	return ret;
}

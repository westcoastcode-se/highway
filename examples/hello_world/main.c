//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "highway.h"
#include "hiw_servlet.h"
#include <stdio.h>
#include <signal.h>

int KEY;
char VALUE = 'x';
hiw_server* server;

void handleSignal(int sig)
{
	if (server != NULL && hiw_server_is_running(server))
	{
		hiw_server_stop(server);
	}
}

void hello_world_servlet(hiw_request* req, hiw_response* resp)
{
	hiw_response_set_status_code(resp, 200);
	hiw_response_set_content_length(resp, 12);
	hiw_response_write_body_raw(resp, "Hello World!", 12);
}

void hello_world_filter(hiw_request* req, hiw_response* resp, const hiw_filter_chain* chain)
{
	void* data = hiw_filter_get_data(chain);

	log_info("hello world");

	hiw_filter_chain_next(req, resp, chain);
}

int main()
{
	if (!hiw_init(hiw_init_config_default))
		return 0;
	signal(SIGINT, handleSignal);

	hiw_server_config server_config = hiw_server_config_default;
	server = hiw_server_new(&server_config);
	if (server)
	{
		// Start the server socket
		hiw_server_start(server);

		// Initialize the filter chain
		const hiw_filter filters[] = {
				{ .func = hello_world_filter, NULL },
				{ 0, 0 }
		};

		// Initialize the servlet
		hiw_servlet servlet = {};
		hiw_servlet_init(&servlet, server);
		hiw_servlet_set_filter_chain(&servlet, filters);
		hiw_servlet_set_func(&servlet, hello_world_servlet);

		// Start the servlet
		hiw_servlet_start(&servlet);

		// release servlet resources
		hiw_servlet_release(&servlet);

		hiw_server_delete(server);
		server = NULL;
	}

	hiw_release();
	return 0;
}

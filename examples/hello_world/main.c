//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw.h"
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

void on_request(hiw_thread* t)
{
	hiw_server_start(server);

	hiw_thread_context context = { .key = &KEY, .value = &VALUE };
	hiw_thread_context_push(t, &context);

	log_infof("hello world %c", *(char*)hiw_thread_context_find(t, &KEY));

	hiw_thread_context_pop(t);
}

void handleRequest(hiw_request* req, hiw_response* resp)
{
	hiw_response_set_status_code(resp, 200);
	hiw_response_set_content_length(resp, 12);
	hiw_response_write_body_raw(resp, "Hello World!", 12);
}

void test(hiw_request* req, hiw_response* resp, const hiw_filter_chain* chain)
{
	void* data = hiw_filter_get_data(chain);

	log_info("hello world");

	hiw_filter_chain_next(req, resp, chain);
}

int main1()
{
	if (!hiw_init(hiw_init_config_default))
		return 0;
	signal(SIGINT, handleSignal);

	hiw_server_config server_config = hiw_server_config_default;
	server = hiw_server_new(&server_config);
	if (server)
	{
		hiw_thread* thread = hiw_thread_main();
		hiw_thread_set_func(thread, on_request);
		hiw_thread_start(thread);

		// Initialize the filter chain
		const hiw_filter filters[] = {
				{ .func = test, NULL },
				{ 0, 0 }
		};

		// Initialize the servlet
		hiw_servlet servlet;
		servlet.config = (hiw_servlet_config){ .num_threads = 20 };
		hiw_servlet_set_filter_chain(&servlet, filters);

		hiw_server_stop(server);
		hiw_server_delete(server);
		server = NULL;
	}

	hiw_release();
	return 0;
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
				{ .func = test, NULL },
				{ 0, 0 }
		};

		// Initialize the servlet
		hiw_servlet servlet = {};
		hiw_servlet_init(&servlet, server);
		hiw_servlet_set_filter_chain(&servlet, filters);
		hiw_servlet_set_func(&servlet, handleRequest);

		// Start the servlet
		hiw_servlet_start(&servlet);

		// release servlet resources
		hiw_servlet_release(&servlet);

		server = NULL;
	}

	hiw_release();
	return 0;
}

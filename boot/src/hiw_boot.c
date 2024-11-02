//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_boot.h"
#include <stdio.h>
#include <signal.h>

// server
hiw_server* server;

/**
* @brief Function called when a new request
* @param req the incoming request
* @param resp the outgoing response
*/
void hiw_boot_on_request(hiw_request* req, hiw_response* resp)
{
	hiw_boot_servlet_func(req, resp);
}

void hiw_boot_signal(int sig)
{
	if (server != NULL && hiw_server_is_running(server))
	{
		hiw_server_stop(server);
	}
}

int hiw_boot_start()
{
	// Create the server
	hiw_server_config server_config = hiw_server_config_default;
	server = hiw_server_new(&server_config);
	if (server == NULL)
	{
		log_error("failed to initialize server");
		return 2;
	}

	// Start the server socket
	hiw_server_start(server);
	hiw_servlet servlet = {};
	hiw_servlet_init(&servlet, server);
	servlet.config.num_threads = 8;
	hiw_servlet_set_func(&servlet, hiw_boot_on_request);
	hiw_servlet_start(&servlet);

	// Release servlet resources
	hiw_servlet_release(&servlet);
	return 0;
}

int main(int argc, char** argv)
{
	signal(SIGINT, hiw_boot_signal);
	if (!hiw_init(hiw_init_config_default)) return 1;
	int ret = hiw_boot_start();
	hiw_release();
	return ret;
}
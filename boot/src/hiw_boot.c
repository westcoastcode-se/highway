//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_boot.h"
#include <stdio.h>
#include <signal.h>

struct hiw_boot_state
{
	hiw_server* server;
	hiw_boot_config config;
};

struct hiw_boot_state hiw_app_state;

struct hiw_boot_userdata
{
	void* userdata;
};

/**
* @brief Function called when a new request is received to this server
* @param req the incoming request
* @param resp the outgoing response
*/
void hiw_boot_on_request(hiw_request* req, hiw_response* resp)
{
	hiw_app_state.config.servlet_func(req, resp);
}

/**
 * @brief Function called when the servlet thread is starting up
 * @param st The new thread
 */
void hiw_boot_on_servlet_start(hiw_servlet_thread* st)
{
	hiw_app_state.config.servlet_start_func(st);
}

/**
 * @brief Function called when a signal is received from the OS
 * @param sig
 */
void hiw_boot_signal(int sig)
{
	if (hiw_app_state.server != NULL && hiw_server_is_running(hiw_app_state.server))
	{
		hiw_server_stop(hiw_app_state.server);
	}
}

void hiw_boot_servlet_start_default(hiw_servlet_thread* st)
{
	hiw_servlet_start_filter_chain(st);
}

void hiw_boot_start(const hiw_boot_config* const config)
{
	// Create the server. Ownership of the server is given to the servlet after it's started up, so
	// we don't have to clean it up
	hiw_app_state.server = hiw_server_new(&config->server_config);
	if (hiw_app_state.server == NULL)
	{
		log_error("failed to initialize highway server");
		return;
	}

	// Set the user-data
	struct hiw_boot_userdata userdata = (struct hiw_boot_userdata){.userdata = config->userdata};
	hiw_server_set_userdata(hiw_app_state.server, &userdata);

	// Start the server socket
	if (hiw_server_start(hiw_app_state.server) != HIW_SERVER_ERROR_SUCCESS)
	{
		hiw_server_delete(hiw_app_state.server);
		hiw_app_state.server = NULL;
		log_error("failed to start highway server");
		return;
	}

	// Start the servlet
	hiw_servlet servlet = {.config = config->servlet_config};
	hiw_servlet_init(&servlet, hiw_app_state.server);
	hiw_servlet_set_starter_func(&servlet, hiw_boot_on_servlet_start);
	hiw_servlet_set_func(&servlet, hiw_boot_on_request);
	hiw_servlet_start(&servlet);

	// Release servlet resources
	hiw_servlet_release(&servlet);
}

void* hiw_boot_get_userdata()
{
	struct hiw_boot_userdata* userdata = hiw_server_get_userdata(hiw_app_state.server);
	if (userdata == NULL) return NULL;
	return userdata->userdata;
}

/**
 * Pre-start phase
 */
void hiw_boot_pre_start(int argc, char** argv)
{
	// load the configuration
	hiw_app_state = (struct hiw_boot_state){
			.server = NULL,
			.config = {
					.server_config = hiw_server_config_default,
					.servlet_config = hiw_servlet_config_default,
					.filters = NULL,
					.servlet_start_func = hiw_boot_servlet_start_default,
					.servlet_func = NULL,
					.userdata = NULL,
					.argc = argc,
					.argv = argv
			}
	};
	hiw_boot_init(&hiw_app_state.config);
}

int main(int argc, char** argv)
{
	signal(SIGINT, hiw_boot_signal);
	if (!hiw_init(hiw_init_config_default)) return 1;
	hiw_boot_pre_start(argc, argv);
	hiw_release();
	return 0;
}
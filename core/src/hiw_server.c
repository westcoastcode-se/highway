//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_server.h"
#include "hiw_logger.h"
#include <assert.h>
#include <hiw_thread.h>

/**
 * Server instance
 */
struct hiw_server
{
	// Configuration
	hiw_server_config config;

	// Server socket
	SOCKET socket;

	// Server is running?
	atomic_bool running;

	// user data associated with the server
	void* userdata;
};

/**
 * Client instance
 */
struct hiw_client
{
	// will be non-zero if an error has occurred
	int error;

	// client socket
	SOCKET socket;

	// ip version used by the client
	hiw_socket_ip_version ip_version;

	// the address to the client
	char address[INET6_ADDRSTRLEN];
};

bool hiw_server_is_error(hiw_server_error err) { return err != HIW_SERVER_ERROR_SUCCESS; }

hiw_server* hiw_server_new(const hiw_server_config* config)
{
	hiw_server* const impl = hiw_malloc(sizeof(hiw_server));
	impl->config = *config;
	impl->userdata = NULL;
	impl->socket = INVALID_SOCKET;
	impl->running = false;
	return impl;
}

hiw_server_error hiw_server_start(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return HIW_SERVER_ERROR_MEMORY;

	// Start listen for incoming requests using the configuration
	hiw_socket_error err;
	s->socket = hiw_socket_listen(&s->config.socket_config, &err);
	if (err != HIW_SOCKET_ERROR_SUCCESS)
	{
		log_error("failed to start highway server");
		return HIW_SERVER_ERROR_SOCKET;
	}
	s->running = true;
	return HIW_SERVER_ERROR_SUCCESS;
}

void* hiw_server_get_userdata(const hiw_server* const s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return NULL;
	return s->userdata;
}

hiw_server_error hiw_server_set_userdata(hiw_server* const s, void* userdata)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return HIW_SERVER_ERROR_MEMORY;
	if (s->socket != INVALID_SOCKET)
		return HIW_SERVER_ERROR_RUNNING;
	s->userdata = userdata;
	return HIW_SERVER_ERROR_SUCCESS;
}

void hiw_server_stop(hiw_server* const s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	if (s->running)
	{
		s->running = false;
		if (s->socket != INVALID_SOCKET)
		{
			log_debugf("hiw_server(%p) closing socket", s);
			hiw_socket_close(s->socket);
			s->socket = INVALID_SOCKET;
		}
	}
	log_debugf("hiw_server(%p) highway server stopped", s);
}

void hiw_server_delete(hiw_server* const s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	assert(s->running == false &&
		   "it is recommended that you stop the server before deleting it's internal resources");
	if (s->socket != INVALID_SOCKET)
	{
		log_debugf("hiw_server(%p) closing socket", s);
		hiw_socket_close(s->socket);
		s->socket = INVALID_SOCKET;
	}
	free(s);
}

bool hiw_server_is_running(hiw_server* const s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return false;
	return s->running;
}

hiw_client* hiw_server_accept(hiw_server* const s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return NULL;
	if (s->running == false)
	{
		log_error("failed to accept client: server is shutting down");
		return NULL;
	}

	log_debugf("server(%p) accepting a new client", s);
	hiw_socket_error err = HIW_SOCKET_ERROR_SUCCESS;
	const SOCKET client_socket = hiw_socket_accept(s->socket, &s->config.socket_config, &err);
	if (client_socket == INVALID_SOCKET)
	{
		log_debugf("hiw_server(%p) failed to accept socket", s);
		return NULL;
	}

	hiw_client* const client = hiw_malloc(sizeof(hiw_client));
	client->socket = client_socket;
	client->ip_version = s->config.socket_config.ip_version;
	client->error = 0;

	// get the actual IP address from the socket
	if (s->config.socket_config.ip_version == HIW_SOCKET_IPV4)
	{
		struct sockaddr_in addr = {};
		socklen_t len = sizeof(addr);
		getpeername(client_socket, (struct sockaddr*)&addr, &len);
		inet_ntop(AF_INET, &addr.sin_addr, client->address, sizeof(client->address));
	}
	else
	{
		struct sockaddr_in6 addr = {};
		socklen_t len = sizeof(addr);
		getpeername(client_socket, (struct sockaddr*)&addr, &len);
		inet_ntop(AF_INET6, &addr.sin6_addr, client->address, sizeof(client->address));
	}

	return client;
}

const char* hiw_client_get_address(hiw_client* c)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return "";
	return c->address;
}

void hiw_client_disconnect(hiw_client* c)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return;
	if (c->socket != INVALID_SOCKET)
	{
		hiw_socket_close(c->socket);
		c->socket = INVALID_SOCKET;
	}
}

void hiw_client_delete(hiw_client* c)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return;
	if (c->socket != INVALID_SOCKET)
	{
		hiw_socket_close(c->socket);
		c->socket = INVALID_SOCKET;
	}
	free(c);
}

int hiw_client_recv(hiw_client* c, char* const dest, const int len)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return -1;
	return hiw_socket_recv(c->socket, dest, len);
}

int hiw_client_send(hiw_client* c, const char* const src, const int len)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return -1;
	return hiw_socket_send(c->socket, src, len);
}

int hiw_client_sendall(hiw_client* c, const char* src, int len)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return -1;
	int bytes_left = len;
	while (bytes_left > 0)
	{
		const int ret = hiw_socket_send(c->socket, src, bytes_left);
		if (ret <= 0)
			return -1;
		bytes_left -= ret;
	}
	return len;
}

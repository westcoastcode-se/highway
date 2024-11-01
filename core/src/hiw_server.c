//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_server.h"
#include "hiw_logger.h"
#include <assert.h>

struct hiw_internal_server
{
	// The public implementation
	hiw_server pub;

	// Server socket
	SOCKET socket;

	// Server is running?
	atomic_bool running;
};
typedef struct hiw_internal_server hiw_internal_server;

struct hiw_internal_client
{
	// The public implementation
	hiw_client pub;

	// client socket
	SOCKET socket;

	// ip version used by the client
	hiw_socket_ip_version ip_version;

	// the address to the client
	char address[INET6_ADDRSTRLEN];
};
typedef struct hiw_internal_client hiw_internal_client;

hiw_server* hiw_server_new(const hiw_server_config* config)
{
	hiw_internal_server* const impl = (hiw_internal_server*)malloc(sizeof(hiw_internal_server));
	if (impl == NULL)
	{
		log_error("out of memory");
		return NULL;
	}

	impl->pub.config = *config;
	impl->socket = INVALID_SOCKET;
	impl->running = false;
	return &impl->pub;
}

hiw_server_error hiw_server_start(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return hiw_SERVER_ERROR_MEMORY;
	hiw_internal_server* const impl = (hiw_internal_server*)s;

	// Start listen for incoming requests using the configuration
	hiw_socket_error err;
	impl->socket = hiw_socket_listen(&s->config.socket_config, &err);
	if (err != hiw_SOCKET_ERROR_NO_ERROR)
	{
		log_error("failed to start highway server");
		return hiw_SERVER_ERROR_SOCKET;
	}
	impl->running = true;
	return hiw_SERVER_ERROR_NO_ERROR;
}

void hiw_server_stop(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	impl->running = false;
	if (impl->socket != INVALID_SOCKET)
	{
		hiw_socket_close(impl->socket);
		impl->socket = INVALID_SOCKET;
	}
}

void hiw_server_delete(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	assert(impl->running == FALSE &&
		   "it is recommended that you stop the server before deleting it's internal resources");
	if (impl->socket != INVALID_SOCKET)
	{
		hiw_socket_close(impl->socket);
		impl->socket = INVALID_SOCKET;
	}
	free(impl);
}

bool hiw_server_is_running(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return false;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	return impl->running;
}

hiw_client* hiw_server_accept(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return false;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	if (impl->running == false)
	{
		log_error("failed to accept client: server is shutting down");
		return NULL;
	}

	log_debugf("server(%p) accepting a new client", s);
	hiw_server_error err = hiw_SERVER_ERROR_NO_ERROR;
	const SOCKET client_socket = hiw_socket_accept(impl->socket, &impl->pub.config.socket_config, &err);
	if (client_socket == INVALID_SOCKET)
	{
		log_infof("Failed to accept client socket: error(%d)", hiw_socket_last_error(client_socket));
		return NULL;
	}

	hiw_internal_client* const client = (hiw_internal_client*)malloc(sizeof(hiw_internal_client));
	if (client == NULL)
	{
		log_error("out of memory");
		hiw_socket_close(client_socket);
		return NULL;
	}

	client->socket = client_socket;
	client->ip_version = impl->pub.config.socket_config.ip_version;
	client->pub.error = 0;

	// get the actual IP address from the socket
	if (impl->pub.config.socket_config.ip_version == HIW_SOCKET_IPV4)
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

	return &client->pub;
}

const char* hiw_client_get_address(hiw_client* c)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return "";
	hiw_internal_client* const impl = (hiw_internal_client*)c;
	return impl->address;
}

void hiw_client_disconnect(hiw_client* c)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return;
	hiw_internal_client* const impl = (hiw_internal_client*)c;
	if (impl->socket != INVALID_SOCKET)
	{
		hiw_socket_close(impl->socket);
		impl->socket = INVALID_SOCKET;
	}
}

void hiw_client_delete(hiw_client* c)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return;
	hiw_internal_client* const impl = (hiw_internal_client*)c;
	if (impl->socket != INVALID_SOCKET)
	{
		hiw_socket_close(impl->socket);
		impl->socket = INVALID_SOCKET;
	}
	free(impl);
}

int hiw_client_recv(hiw_client* c, char* dest, int len)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return -1;
	hiw_internal_client* const impl = (hiw_internal_client*)c;
	return hiw_socket_recv(impl->socket, dest, len);
}

int hiw_client_send(hiw_client* c, const char* const src, const int len)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return -1;
	hiw_internal_client* const impl = (hiw_internal_client*)c;
	return hiw_socket_send(impl->socket, src, len);
}

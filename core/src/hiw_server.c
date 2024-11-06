//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_server.h"
#include "hiw_logger.h"
#include <assert.h>
#include <hiw_thread.h>

struct hiw_internal_server
{
	// The public implementation
	hiw_server pub;

	// Server socket
	SOCKET socket;

	// Server is running?
	atomic_bool running;

	// user data associated with the server
	void* userdata;
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

bool hiw_server_is_error(hiw_server_error err) { return err != HIW_SERVER_ERROR_NO_ERROR; }

hiw_server* hiw_server_new(const hiw_server_config* config)
{
	hiw_internal_server* const impl = (hiw_internal_server*)malloc(sizeof(hiw_internal_server));
	if (impl == NULL)
	{
		log_error("out of memory");
		return NULL;
	}

	impl->pub.config = *config;
	impl->userdata = NULL;
	impl->socket = INVALID_SOCKET;
	impl->running = false;
	return &impl->pub;
}

hiw_server_error hiw_server_start(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return HIW_SERVER_ERROR_MEMORY;
	hiw_internal_server* const impl = (hiw_internal_server*)s;

	// Start listen for incoming requests using the configuration
	hiw_socket_error err;
	impl->socket = hiw_socket_listen(&s->config.socket_config, &err);
	if (err != hiw_SOCKET_ERROR_NO_ERROR)
	{
		log_error("failed to start highway server");
		return HIW_SERVER_ERROR_SOCKET;
	}
	impl->running = true;
	return HIW_SERVER_ERROR_NO_ERROR;
}

void* hiw_server_get_userdata(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return NULL;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	return impl->userdata;
}

hiw_server_error hiw_server_set_userdata(hiw_server* s, void* userdata)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return HIW_SERVER_ERROR_MEMORY;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	if (impl->socket != INVALID_SOCKET)
		return HIW_SERVER_ERROR_RUNNING;
	impl->userdata = userdata;
	return HIW_SERVER_ERROR_NO_ERROR;
}

void hiw_server_stop(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	if (impl->running)
	{
		impl->running = false;
		if (impl->socket != INVALID_SOCKET)
		{
			log_debugf("hiw_server(%p) closing socket", s);
			hiw_socket_close(impl->socket);
			impl->socket = INVALID_SOCKET;
		}
	}
	log_debugf("hiw_server(%p) highway server stopped", s);
}

void hiw_server_delete(hiw_server* s)
{
	assert(s != NULL && "expected 's' to exist");
	if (s == NULL)
		return;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	assert(impl->running == false &&
		   "it is recommended that you stop the server before deleting it's internal resources");
	if (impl->socket != INVALID_SOCKET)
	{
		log_debugf("hiw_server(%p) closing socket", s);
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
		return NULL;
	hiw_internal_server* const impl = (hiw_internal_server*)s;
	if (impl->running == false)
	{
		log_error("failed to accept client: server is shutting down");
		return NULL;
	}

	log_debugf("server(%p) accepting a new client", s);
	hiw_socket_error err = hiw_SOCKET_ERROR_NO_ERROR;
	const SOCKET client_socket = hiw_socket_accept(impl->socket, &impl->pub.config.socket_config, &err);
	if (client_socket == INVALID_SOCKET)
	{
		log_debugf("hiw_server(%p) failed to accept socket", s);
		return NULL;
	}

	hiw_internal_client* const client = malloc(sizeof(hiw_internal_client));
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

int hiw_client_recv(hiw_client* c, char* const dest, const int len)
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

int hiw_client_sendall(hiw_client* c, const char* src, int len)
{
	assert(c != NULL && "expected 'c' to exist");
	if (c == NULL)
		return -1;
	hiw_internal_client* const impl = (hiw_internal_client*)c;

	int bytes_left = len;
	while (bytes_left > 0)
	{
		const int ret = hiw_socket_send(impl->socket, src, bytes_left);
		if (ret <= 0)
			return -1;
		bytes_left -= ret;
	}
	return len;
}

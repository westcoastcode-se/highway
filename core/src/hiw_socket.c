//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_socket.h"
#include "hiw_logger.h"
#include <assert.h>

int hiw_socket_recv_all(SOCKET s, char* dest, const int len)
{
	assert(dest != NULL && "expected 'dest' to exist");
	if (dest == NULL)
		return -1;
	if (len == 0)
		return 0;

	int bytes_left = len;
	while (bytes_left > 0)
	{
		const int count = recv(s, dest, bytes_left, 0);
		if (count < 0)
			return len - bytes_left;
		bytes_left -= count;
		dest += count;
	}
	return len;
}

int hiw_socket_recv(SOCKET s, char* dest, int len)
{
	assert(dest != NULL && "expected 'dest' to exist");
	if (dest == NULL)
		return -1;
	if (len == 0)
		return 0;
	return recv(s, dest, len, 0);
}

int hiw_socket_send(SOCKET s, const char* dest, int len)
{
	assert(dest != NULL && "expected 'dest' to exist");
	if (dest == NULL)
		return -1;
	if (len == 0)
		return 0;
	return send(s, dest, len, 0);
}

int hiw_socket_last_error(SOCKET s)
{
	return WSAGetLastError();
}

bool hiw_internal_server_bind_ipv4(SOCKET sock, unsigned short port)
{
	// bind socket to address and port
	SOCKADDR_IN addr = { 0 };
	// TODO: allow for binding a specific IP only
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

	const int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
	if (result < 0)
	{
		log_errorf("could not bind socket: error(%d)", hiw_socket_last_error(sock));
		return false;
	}
	return true;
}

bool hiw_internal_server_bind_ipv6(SOCKET sock, unsigned short port)
{
	// bind socket to address and port
	struct sockaddr_in6 addr = { 0 };
	// TODO: allow for binding a specific IP only
	addr.sin6_addr = in6addr_any;
	addr.sin6_port = htons(port);
	addr.sin6_family = AF_INET6;

	const int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
	if (result < 0)
	{
		log_errorf("could not bind socket: error(%d)", hiw_socket_last_error(sock));
		return false;
	}
	return true;
}

SOCKET hiw_socket_listen(const hiw_socket_config* config, hiw_socket_error* err)
{
	// create socket
	const SOCKET sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		log_errorf("could not create server socket: error(%d)", hiw_socket_last_error(sock));
		*err = hiw_SOCKET_ERROR_CREATE;
		return INVALID_SOCKET;
	}

	// configure socket
	int opt = 1;
	int result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	if (result < 0)
	{
		hiw_socket_close(sock);
		log_errorf("could not configure socket: error(%d)", hiw_socket_last_error(sock));
		*err = hiw_SOCKET_ERROR_CONFIG;
		return INVALID_SOCKET;
	}

	// allow for IPv4 address mapping over IPv6
	if (config->ip_version == HIW_SOCKET_IPV4_AND_6)
	{
		opt = 0;
		result = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&opt, sizeof(opt));
		if (result < 0)
		{
			hiw_socket_close(sock);
			log_errorf("could not configure socket: error(%d)", hiw_socket_last_error(sock));
			*err = hiw_SOCKET_ERROR_CONFIG;
			return INVALID_SOCKET;
		}
	}

#ifdef SO_REUSEPORT
	opt = 1;
	result = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (const char*)&opt, sizeof(opt));
	if (result < 0)
	{
		hiw_socket_close(sock);
		log_errorf("could not configure socket: error(%d)", hiw_socket_last_error(sock));
		*err = hiw_SOCKET_ERROR_CONFIG;
		return INVALID_SOCKET;
	}
#endif

	switch (config->ip_version)
	{
	case HIW_SOCKET_IPV4:
		if (!hiw_internal_server_bind_ipv4(sock, config->port))
		{
			hiw_socket_close(sock);
			*err = hiw_SOCKET_ERROR_BIND;
			return INVALID_SOCKET;
		}
		break;
	case HIW_SOCKET_IPV4_AND_6:
	case HIW_SOCKET_IPV6:
		if (!hiw_internal_server_bind_ipv6(sock, config->port))
		{
			hiw_socket_close(sock);
			*err = hiw_SOCKET_ERROR_BIND;
			return INVALID_SOCKET;
		}
		break;
	}

	if (listen(sock, 500))
	{
		hiw_socket_close(sock);
		log_errorf("could not listen for incoming requests: error(%d)", hiw_socket_last_error(sock));
		*err = hiw_SOCKET_ERROR_LISTEN;
		return INVALID_SOCKET;
	}

	*err = hiw_SOCKET_ERROR_NO_ERROR;
	return sock;
}

SOCKET hiw_socket_accept(SOCKET server_socket, const hiw_socket_config* config, hiw_socket_error* err)
{
	SOCKET sock = INVALID_SOCKET;
	struct sockaddr_in addr = {};
	struct sockaddr_in6 addr_v6 = {};

	if (config->ip_version == HIW_SOCKET_IPV4)
	{
		int addr_len = sizeof(addr);
		sock = accept(server_socket, (struct sockaddr*)&addr, &addr_len);
		if (sock == INVALID_SOCKET)
		{
			*err = hiw_SOCKET_ERROR_ACCEPT;
			log_infof("Failed to accept client socket: error(%d)", hiw_socket_last_error(sock));
			return INVALID_SOCKET;
		}
	}
	else
	{
		int addr_len = sizeof(addr_v6);
		sock = accept(server_socket, (struct sockaddr*)&addr_v6, &addr_len);
		if (sock == INVALID_SOCKET)
		{
			*err = hiw_SOCKET_ERROR_ACCEPT;
			log_infof("Failed to accept client socket: error(%d)", hiw_socket_last_error(sock));
			return INVALID_SOCKET;
		}
	}

#if defined(HIW_WINDOWS)

	DWORD value = config->read_timeout;
	int result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&value, sizeof(value));
	if (result < 0)
	{
		*err = hiw_SOCKET_ERROR_CONFIG;
		log_errorf("could not configure client socket: error(%d)", hiw_socket_last_error(sock));
		hiw_socket_close(sock);
		return INVALID_SOCKET;
	}

	value = config->write_timeout;
	result = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&value, sizeof(value));
	if (result < 0)
	{
		*err = hiw_SOCKET_ERROR_CONFIG;
		log_errorf("could not configure client socket: error(%d)", hiw_socket_last_error(sock));
		hiw_socket_close(sock);
		return INVALID_SOCKET;
	}

#else

#error Implement for Linux and OSX!!

#endif

	return sock;
}

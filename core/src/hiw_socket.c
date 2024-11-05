//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_socket.h"
#include "hiw_logger.h"
#include <assert.h>

hiw_socket_error hiw_socket_set_timeout(SOCKET sock, unsigned int read_timeout, unsigned int write_timeout)
{
	log_debugf("setting read_timeout=%u ms and write_timeout=%u ms", read_timeout, write_timeout);

#if defined(HIW_WINDOWS)
	DWORD value = read_timeout;
	int result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&value, sizeof(value));
	if (result < 0)
	{
		log_errorf("could not configure client socket: error(%d)", result);
		return hiw_SOCKET_ERROR_CONFIG;
	}

	value = write_timeout;
	result = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&value, sizeof(value));
	if (result < 0)
	{
		log_errorf("could not configure client socket: error(%d)", result);
		return hiw_SOCKET_ERROR_CONFIG;
	}
#else
	struct timeval tv;

	tv.tv_sec = read_timeout / 1000;
	tv.tv_usec = (read_timeout%1000) * 1000;
	int result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (result < 0)
	{
		log_errorf("could not configure client socket: error(%d)", result);
		return hiw_SOCKET_ERROR_CONFIG;
	}

	tv.tv_sec = write_timeout / 1000;
	tv.tv_usec = (write_timeout%1000) * 1000;
	result = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (result < 0)
	{
		log_errorf("could not configure client socket: error(%d)", result);
		return hiw_SOCKET_ERROR_CONFIG;
	}
#endif

	log_debug("timeout configured");
	return hiw_SOCKET_ERROR_NO_ERROR;
}

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

bool hiw_internal_server_bind_ipv4(SOCKET sock, unsigned short port)
{
	// bind socket to address and port
	struct sockaddr_in addr = { 0 };
	// TODO: allow for binding a specific IP only
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;

	const int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
	if (result < 0)
	{
		log_errorf("could not bind socket: error(%d)", result);
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
		log_errorf("could not bind socket: error(%d)", result);
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
		log_error("could not create server socket");
		*err = hiw_SOCKET_ERROR_CREATE;
		return INVALID_SOCKET;
	}

	int opt;
	int result;

	// configure socket
#ifdef SO_REUSEADDR
	opt = 1;
	result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	if (result < 0)
	{
		hiw_socket_close(sock);
		log_errorf("could not configure socket: error(%d)", result);
		*err = hiw_SOCKET_ERROR_CONFIG;
		return INVALID_SOCKET;
	}
#endif

	// allow for IPv4 address mapping over IPv6
	if (config->ip_version == HIW_SOCKET_IPV4_AND_6)
	{
		opt = 0;
		result = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&opt, sizeof(opt));
		if (result < 0)
		{
			hiw_socket_close(sock);
			log_errorf("could not configure socket: error(%d)", result);
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
		log_errorf("could not configure socket: error(%d)", result);
		*err = hiw_SOCKET_ERROR_CONFIG;
		return INVALID_SOCKET;
	}
#endif
	*err = hiw_socket_set_timeout(sock, config->read_timeout, config->write_timeout);
	if (*err != hiw_SOCKET_ERROR_NO_ERROR)
	{
		hiw_socket_close(sock);
		return INVALID_SOCKET;
	}

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

	result = listen(sock, 500);
	if (result)
	{
		hiw_socket_close(sock);
		log_errorf("could not listen for incoming requests: error(%d)", result);
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
		socklen_t addr_len = sizeof(addr);
		sock = accept(server_socket, (struct sockaddr*)&addr, &addr_len);
		if (sock == INVALID_SOCKET)
		{
			*err = hiw_SOCKET_ERROR_ACCEPT;
			log_info("Failed to accept client socket");
			return INVALID_SOCKET;
		}
	}
	else
	{
		socklen_t addr_len = sizeof(addr_v6);
		sock = accept(server_socket, (struct sockaddr*)&addr_v6, &addr_len);
		if (sock == INVALID_SOCKET)
		{
			*err = hiw_SOCKET_ERROR_ACCEPT;
			log_info("Failed to accept client socket");
			return INVALID_SOCKET;
		}
	}

	*err = hiw_socket_set_timeout(sock, config->read_timeout, config->write_timeout);
	if (*err != hiw_SOCKET_ERROR_NO_ERROR)
	{
		hiw_socket_close(sock);
		return INVALID_SOCKET;
	}
	return sock;
}

//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef hiw_SOCKET_H
#define hiw_SOCKET_H

#include "hiw_std.h"

#if defined(HIW_WINDOWS)

#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#define hiw_socket_close(s) closesocket(s)
#else

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#define hiw_socket_close(s) close(s)
#define SOCKET int
#define INVALID_SOCKET -1
#endif

#ifdef __cplusplus
extern "C" {
#endif

// the default port
#define HIW_SOCKET_DEFAULT_PORT (8080)

// the default read timeout if nothing else is specified. 0 is infinite
#define HIW_SOCKET_DEFAULT_READ_TIMEOUT (0)

// the default write timeout if nothing else is specified. 0 is infinite
#define HIW_SOCKET_DEFAULT_WRITE_TIMEOUT (0)

// How much are allowed to send in one TCP packet
#define HIW_SOCKET_SEND_CHUNK_SIZE (4096)

typedef enum hiw_socket_ip_version
{
	// Allow only IPv4 connections
	HIW_SOCKET_IPV4,

	// Allow only IPv6 connections
	HIW_SOCKET_IPV6,

	// Allow both IPv4 and 6 connections. This is the default value
	HIW_SOCKET_IPV4_AND_6,
} hiw_socket_ip_version;

// configuration for socket
struct HIW_PUBLIC hiw_socket_config
{
	// The port
	unsigned short port;

	// timeout used when reading data over a socket
	unsigned int read_timeout;

	// timeout used when writing data over a socket
	unsigned int write_timeout;

	// what ip version is allowed
	hiw_socket_ip_version ip_version;
};

typedef struct hiw_socket_config hiw_socket_config;

// default configuration
#define hiw_socket_config_default                                                                                      \
	(hiw_socket_config)                                                                                                \
	{                                                                                                                  \
		.port = HIW_SOCKET_DEFAULT_PORT, .read_timeout = HIW_SOCKET_DEFAULT_READ_TIMEOUT,                              \
		.write_timeout = HIW_SOCKET_DEFAULT_WRITE_TIMEOUT, .ip_version = HIW_SOCKET_IPV4_AND_6                         \
	}

/**
 * Receive all bytes from the supplied socket. Limit the number of bytes to the supplied length
 *
 * @param s The socket
 * @param dest The destination buffer where to put the data
 * @param len The total length to receive
 * @return The number of bytes received; -1 if the socket failed to read any bytes
 */
HIW_PUBLIC int hiw_socket_recv_all(SOCKET s, char* dest, int len);

/**
 * Receive bytes from the supplied socket
 *
 * @param s The socket
 * @param dest The destination buffer where to put the data
 * @param len The total length to receive
 * @return The number of bytes received; -1 if the socket failed to read any bytes
 */
HIW_PUBLIC int hiw_socket_recv(SOCKET s, char* dest, int len);

/**
 * Send the number of bytes to the supplied socket
 *
 * @param s The socket
 * @param dest The source buffer where to read the data to be sent
 * @param len The total length to send
 * @return The number of bytes sent; -1 if the socket failed to send any bytes
 */
HIW_PUBLIC int hiw_socket_send(SOCKET s, const char* dest, int len);

enum HIW_PUBLIC hiw_socket_error
{
	// No error happened
	hiw_SOCKET_ERROR_NO_ERROR = 0,

	// Could not create the socket. Maybe the system is out of memory?
	hiw_SOCKET_ERROR_CREATE,

	// Could not configure socket with properties, such as timeout
	HIW_SOCKET_ERROR_CONFIG,

	// Could not bind socket to address and port. Maybe some other process
	// is using it?
	hiw_SOCKET_ERROR_BIND,

	// Could not listen for incoming connections
	hiw_SOCKET_ERROR_LISTEN,

	// Could not accept incoming socket request
	hiw_SOCKET_ERROR_ACCEPT
};

typedef enum hiw_socket_error hiw_socket_error;

/**
 * Create a socket and listen for incoming connections
 *
 * @param config
 * @param err
 * @return
 */
HIW_PUBLIC SOCKET hiw_socket_listen(const hiw_socket_config* config, hiw_socket_error* err);

/**
 * Create a socket and listen for incoming connections
 *
 * @param server_socket
 * @param config
 * @param err
 * @return
 */
HIW_PUBLIC SOCKET hiw_socket_accept(SOCKET server_socket, const hiw_socket_config* config, hiw_socket_error* err);

#ifdef __cplusplus
}
#endif

#endif // hiw_SOCKET_H

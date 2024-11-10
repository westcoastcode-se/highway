//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef hiw_SERVER_H
#define hiw_SERVER_H

#include "hiw_socket.h"
#include "hiw_std.h"

#ifdef __cplusplus
extern "C" {
#endif

struct HIW_PUBLIC hiw_server_config
{
	// underlying server socket config
	hiw_socket_config socket_config;
};

typedef struct hiw_server_config hiw_server_config;

// Default configuration for a highway server
#define hiw_server_config_default                                                                                      \
	(hiw_server_config) { .socket_config = hiw_socket_config_default }

/**
 * Server instance
 */
struct HIW_PUBLIC hiw_server
{
	// Configuration
	hiw_server_config config;
};

typedef struct hiw_server hiw_server;

enum HIW_PUBLIC hiw_server_error
{
	// Everything is fine!
	HIW_SERVER_ERROR_SUCCESS = 0,

	// Memory error, most likely because missing server memory
	HIW_SERVER_ERROR_MEMORY,

	// Error raised if the server is running when it's expected not to
	HIW_SERVER_ERROR_RUNNING,

	// underlying socket error
	HIW_SERVER_ERROR_SOCKET,
};

typedef enum hiw_server_error hiw_server_error;

/**
 * Client instance
 */
struct HIW_PUBLIC hiw_client
{
	// will be non-zero if an error has occurred
	int error;
};

typedef struct hiw_client hiw_client;

/**
 * @return true if the supplied error is considered an actual error
 */
HIW_PUBLIC extern bool hiw_server_is_error(hiw_server_error err);

/**
 * @return A stopped highway server
 */
HIW_PUBLIC extern hiw_server* hiw_server_new(const hiw_server_config* config);

/**
 * @return a potential error
 */
HIW_PUBLIC hiw_server_error hiw_server_start(hiw_server* s);

/**
 * @return user-data associated with the server
 */
HIW_PUBLIC void* hiw_server_get_userdata(hiw_server* s);

/**
 * @param s the server
 * @param userdata the user data
 * @return a value indicating if userdata was set to the server or not
 * @remark please note that you are not allowed to set the user data after the server has started
 */
HIW_PUBLIC hiw_server_error hiw_server_set_userdata(hiw_server* s, void* userdata);

/**
 * @param s The server we want to stop
 */
HIW_PUBLIC void hiw_server_stop(hiw_server* s);

/**
 * @param s the server to be deleted
 */
HIW_PUBLIC extern void hiw_server_delete(hiw_server* s);

/**
 * @param s the server
 * @return true if the server is running
 */
HIW_PUBLIC extern bool hiw_server_is_running(hiw_server* s);

/**
 * Accept a new client
 *
 * @param s the server
 * @return A client if a new connection is established; NULL otherwise
 */
HIW_PUBLIC extern hiw_client* hiw_server_accept(hiw_server* s);

/**
 * Get the address of the client
 *
 * @param c the client
 * @return the address
 */
HIW_PUBLIC extern const char* hiw_client_get_address(hiw_client* c);

/**
 * Disconnect client
 *
 * @param c the client
 */
HIW_PUBLIC extern void hiw_client_disconnect(hiw_client* c);

/**
 * Delete the client's internal memory
 *
 * @param c the client
 */
HIW_PUBLIC extern void hiw_client_delete(hiw_client* c);

/**
 * Receive data from the supplier client
 */
HIW_PUBLIC extern int hiw_client_recv(hiw_client* c, char* dest, int len);

/**
 * Send data to this client
 */
HIW_PUBLIC extern int hiw_client_send(hiw_client* c, const char* src, int len);

/**
 * Send all data to this client
 */
HIW_PUBLIC extern int hiw_client_sendall(hiw_client* c, const char* src, int len);

#ifdef __cplusplus
}
#endif

#endif // hiw_SERVER_H

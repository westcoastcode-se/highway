//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef HIGHWAY_H
#define HIGHWAY_H

#include "hiw_std.h"
#include "hiw_thread.h"
#include "hiw_socket.h"
#include "hiw_server.h"
#include "hiw_logger.h"
#include "hiw_mimetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialization properties for highway
 */
struct HIW_PUBLIC hiw_init_config
{
	// Should we initialize the underlying OS socket library? This is, normally, only
	// used on Windows. If you are calling WSAStartup yourself, then this can be set to false
	bool initialize_sockets;

	// Should we initialize memory for the thread library? This is, normally, set to true
	// unless you know what you are doing
	bool initialize_threads;
};
typedef struct hiw_init_config hiw_init_config;

// default configuration
#define hiw_init_config_default                                                                                        \
	(hiw_init_config) { .initialize_sockets = true, .initialize_threads = true }

/**
 * @brief Initialize the wcc http framework
 * @return true if initialization is successful
 */
HIW_PUBLIC extern bool hiw_init(hiw_init_config config);

/**
 * @brief release wcc http framework
 */
HIW_PUBLIC extern void hiw_release();

#ifdef __cplusplus
}
#endif

#endif // HIGHWAY_H

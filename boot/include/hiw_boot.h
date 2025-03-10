//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef HIW_BOOT_H
#define HIW_BOOT_H

#include <highway.h>
#include <hiw_servlet.h>

/**
* Highway Boot takes over the responsibility for entry point for the application
*/
extern int main(int argc, char** argv);

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Configuration
*/
struct HIW_PUBLIC hiw_boot_config
{
    // The server config, is default if not set
    hiw_server_config server_config;

    // The servlet config, is default if not set
    hiw_servlet_config servlet_config;

    // Filters to be used when creating the filter chain
    hiw_filter* filters;

    // Function called when a servlet thread is started. This is most often used
    // when setting thread-local variables that should be available in the entire filter chain
    hiw_servlet_start_fn servlet_start_func;

    // Function called when the server receives a new request
    hiw_servlet_fn servlet_func;

    // User data that's associated with the server
    void* userdata;

    // Number of arguments received to this application on start
    int argc;

    // The argument text received to this application on start
    char** argv;
};

typedef struct hiw_boot_config hiw_boot_config;

/**
 * @brief Method that's responsible for initializing the Highway Boot Framework
 * @param config where to put the configuration
 * @return the return code. 0 if everything was okay.
 */
HIW_PUBLIC extern int hiw_boot_init(hiw_boot_config* config);

/**
 * @brief Start the highway boot application
 * @param config The configuration used
 */
HIW_PUBLIC extern int hiw_boot_start(const hiw_boot_config* config);

/**
 * @return The user data associated with the Highway Boot.
 * @remark Please note that the same user-data is available in all threads. Thread-safety is therefor up to the
 *         developer to keep track of
 */
HIW_PUBLIC extern void* hiw_boot_get_userdata();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// C++23 unique features

#endif

#endif //HIW_BOOT_H

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

// The function to be called
extern hiw_servlet_fn hiw_boot_servlet_func;

/**
* @brief helper define that the developer should use to specify a function
*/
#define DEFINE_SERVLET_FUNC(target) hiw_servlet_fn hiw_boot_servlet_func = target

#ifdef __cplusplus
}
#endif

#endif //HIW_BOOT_H

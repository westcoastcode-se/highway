//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef hiw_LOGGER_H
#define hiw_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef NDEBUG
#	define log_debugf(format, ...) fprintf(stdout, "DEBUG: " format "\n", __VA_ARGS__)
#	define log_debug(format) fprintf(stdout, "DEBUG: " format "\n")
#else
#	define log_debugf(format, ...)
#	define log_debug(format)
#endif

// Log a message with arguments
#define log_infof(format, ...) fprintf(stdout, "INFO:  " format "\n", __VA_ARGS__)

// Log a message
#define log_info(format) fprintf(stdout, "INFO:  " format "\n")

// Log a warning message with arguments. Warnings are faults in the code
// that originates from the client or if the problem isn't important enough
// to be a warning
#define log_warnf(format, ...) fprintf(stdout, "WARN:  " format "\n", __VA_ARGS__)

// Log a warning message. Warnings are faults in the code
// that originates from the client or if the problem isn't important enough
// to be a warning
#define log_warn(format) fprintf(stdout, "WARN:  " format "\n")

// Log an error message with arguments. Error messages are faults in the code
// that originates from the server or if the error is important enough to be visible
#define log_errorf(format, ...) fprintf(stderr, "ERROR: " format "\n", __VA_ARGS__)

// Log an error message. Error messages are faults in the code
// that originates from the server or if the error is important enough to be visible
#define log_error(format) fprintf(stderr, "ERROR: " format "\n")

#ifdef __cplusplus
}
#endif

#endif //hiw_LOGGER_H

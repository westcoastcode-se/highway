//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef HIW_FILE_CONTENT_H
#define HIW_FILE_CONTENT_H

#include "hiw_servlet.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Represents a unique file. Please note that the content in this object is not kept between iterations
 */
struct HIW_PUBLIC hiw_file
{
	// The full path to the file. The path is guaranteed to end with a \0 character
	hiw_string path;

	// The filename, excluding the path. The filename guaranteed to end with a \0 character
	hiw_string filename;

	// The suffix. The suffix guaranteed to end with a \0 character
	hiw_string suffix;
};

typedef struct hiw_file hiw_file;

enum hiw_file_traverse_error
{
	// no error happened
	HIW_FILE_TRAVERSE_ERROR_SUCCESS = 0,

	// the root path was not found
	HIW_FILE_TRAVERSE_ERROR_NOT_FOUND,

	// the length of the path becomes to large
	HIW_FILE_TRAVERSE_ERROR_PATH_LEN,

	// iteration was aborted by the caller
	HIW_FILE_TRAVERSE_ERROR_ABORTED
};

typedef enum hiw_file_traverse_error hiw_file_traverse_error;

/**
 * A callback function called when a new file is found during iteration
 * @return false if the iteration should be aborted
 */
typedef bool (*hiw_file_callback_fn)(const hiw_file* file, void* userdata);

/**
 * @brief Create a new iterator used when traversing a file-system file tree
 * @return 0 if the traversal worked fine or an error code if an error occurred during iteration
 */
extern HIW_PUBLIC hiw_file_traverse_error hiw_file_traverse(hiw_string root_path, hiw_file_callback_fn func,
															void* userdata);

#ifdef __cplusplus
}
#endif

#endif // HIW_FILE_CONTENT_H

//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_file_content.h"
#include "hiw_logger.h"
#include <assert.h>

#if defined(HIW_WINDOWS)
#	include <windows.h>
#	define HIW_MAX_PATH MAX_PATH

bool hiw_file_ignored(const char* filename, int len)
{
	if (len > 2) return false;
	if (len > 1) return *filename == '.' && *(filename + 1) == '.';
	if (len == 1) return *filename == '.';
	return false;
}

hiw_file_traverse_error hiw_file_traverse1(hiw_string root_path, hiw_file_callback_fn func, void* userdata,
                        char* path, char* path0)
{
	WIN32_FIND_DATA ffd;
	HANDLE handle;

	log_debugf("scanning '%.*s'", root_path.length, root_path.begin);
	char* const path1 = hiw_std_copy("\\", 1, path0, HIW_MAX_PATH - (int)(path0 - path));
	char* path2 = hiw_std_copy("*.*\0", 4, path1, HIW_MAX_PATH - (int)(path1 - path));

	// if 4 bytes was not written, then we know for sure that the path buffer is too small
	if ((path2 - path1) != 4)
	{
		log_errorf("invalid search query '%.*s'. Try increase HIW_MAX_PATH", HIW_MAX_PATH - (int)(path2 - path), path);
		return HIW_FILE_TRAVERSE_ERROR_PATH_LEN;
	}

	if ((handle = FindFirstFile(path, &ffd)) == INVALID_HANDLE_VALUE)
	{
		log_errorf("could not open path '%.*s", (int)(path2 - path), path);
		return HIW_FILE_TRAVERSE_ERROR_NOT_FOUND;
	}

	// convert windows \ with unix /
	*(path1 - 1) = '/';

	do
	{
		const int len = (int)strlen(ffd.cFileName);

		path2 = hiw_std_copy(ffd.cFileName, len, path1, HIW_MAX_PATH - (int)(path1 - path));
		hiw_std_copy("\0", 1, path2, HIW_MAX_PATH - (int)(path2 - path));

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (hiw_file_ignored(ffd.cFileName, len)) continue;
			const hiw_string file_path = {.begin = root_path.begin, .length = (int)(path2 - root_path.begin)};
			const hiw_file_traverse_error err = hiw_file_traverse1(file_path, func, userdata, path, path2);
			if (err != HIW_FILE_TRAVERSE_ERROR_SUCCESS) return err;
		}
		else
		{
			const hiw_string file_path = {.begin = path, .length = (int)(path2 - path)};
			const hiw_string filename = {.begin = path2 - len, .length = len};
			const hiw_string suffix = hiw_string_suffix(filename, '.');
			const hiw_file file = {
					.path = file_path,
					.filename = filename,
					.suffix = suffix,
			};
			if (!func(&file, userdata))
				return HIW_FILE_TRAVERSE_ERROR_ABORTED;
		}
	} while (FindNextFile(handle, &ffd));

	return HIW_FILE_TRAVERSE_ERROR_SUCCESS;
}

hiw_file_traverse_error hiw_file_traverse(hiw_string root_path, hiw_file_callback_fn func, void* userdata)
{
	WIN32_FIND_DATA ffd;
	HANDLE handle;

	log_debugf("scanning '%.*s'", root_path.length, root_path.begin);
	char path[HIW_MAX_PATH];
	char* const path0 = hiw_std_copy(root_path.begin, root_path.length, path, HIW_MAX_PATH);
	char* const path1 = hiw_std_copy("\\", 1, path0, HIW_MAX_PATH - (int)(path0 - path));
	char* path2 = hiw_std_copy("*.*\0", 4, path1, HIW_MAX_PATH - (int)(path1 - path));

	// if 4 bytes was not written, then we know for sure that the path buffer is too small
	if ((path2 - path1) != 4)
	{
		log_errorf("invalid search query '%.*s'. Try increase HIW_MAX_PATH", HIW_MAX_PATH - (int)(path2 - path), path);
		return HIW_FILE_TRAVERSE_ERROR_PATH_LEN;
	}

	if ((handle = FindFirstFile(path, &ffd)) == INVALID_HANDLE_VALUE)
	{
		log_errorf("could not find files in %s", path);
		return HIW_FILE_TRAVERSE_ERROR_NOT_FOUND;
	}

	// convert windows \ with unix /
	*(path1 - 1) = '/';

	do
	{
		const int len = (int)strlen(ffd.cFileName);

		path2 = hiw_std_copy(ffd.cFileName, len, path1, HIW_MAX_PATH - (int)(path1 - path));
		hiw_std_copy("\0", 1, path2, HIW_MAX_PATH - (int)(path2 - path));

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (hiw_file_ignored(ffd.cFileName, len)) continue;
			const hiw_string file_path = {.begin = path, .length = (int)(path2 - path)};
			const hiw_file_traverse_error err = hiw_file_traverse1(file_path, func, userdata, path, path2);
			if (err != HIW_FILE_TRAVERSE_ERROR_SUCCESS) return err;
		}
		else
		{
			const hiw_string file_path = {.begin = path, .length = (int)(path2 - path)};
			const hiw_string filename = {.begin = path2 - len, .length = len};
			const hiw_string suffix = hiw_string_suffix(filename, '.');
			const hiw_file file = {
					.path = file_path,
					.filename = filename,
					.suffix = suffix,
			};
			if (!func(&file, userdata))
				return HIW_FILE_TRAVERSE_ERROR_ABORTED;
		}
	} while (FindNextFile(handle, &ffd));

	return HIW_FILE_TRAVERSE_ERROR_SUCCESS;
}

#endif
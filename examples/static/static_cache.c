//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "static_cache.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)

void windows_path_to_linux(char* str, int len)
{
	// convert the filename into a URI-friendly path
	for (int i = 0; i < len; ++i)
	{
		if (*str == '\\')
			*str = '/';
		str++;
	}
}

#endif

// default cache capacity
static const cache_content_capacity = 64;

hiw_string static_cache_get_mime_type(hiw_string str)
{
	const hiw_string suffix = hiw_string_suffix(str, '.');

	if (hiw_string_cmpc(suffix, ".html", 5))
		return hiw_mimetypes.text_html;

	if (hiw_string_cmpc(suffix, ".png", 4))
		return hiw_mimetypes.image_png;

	if (hiw_string_cmpc(suffix, ".css", 4))
		return hiw_mimetypes.text_css;

	if (hiw_string_cmpc(suffix, ".js", 4))
		return hiw_mimetypes.text_javascript;

	// ....

	// use text plain if not sure?
	return hiw_mimetypes.text_plain;
}

bool static_cache_add(static_cache* cache, const char* filename)
{
	log_infof("Caching '%s'", filename);

	// Copy the filename into the memory buffer
	const int filename_length = strlen(filename) - cache->base_dir.length;
	char* const filename_copy = hiw_memory_get(&cache->memory, filename_length);
	hiw_std_mempy(filename + cache->base_dir.length, filename_length, filename_copy, filename_length);

#if defined(_WIN32)
	windows_path_to_linux(filename_copy, filename_length);
#endif

	FILE* f = fopen(filename, "rb");
	if (f == NULL)
	{
		log_errorf("Could not open file '%s'", filename);
		return false;
	}

	fseek(f, 0, SEEK_END);
	const int size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = hiw_memory_get(&cache->memory, size);
	if (buf == NULL)
	{
		log_error("Out of memory");
		return false;
	}

	if (fread(buf, size, 1, f) != 1)
	{
		log_errorf("Failed to read '%s' into memory", filename);
		return false;
	}
	fclose(f);

	if (cache->content_count % cache_content_capacity == 0)
	{
		static_content* const newmem = realloc(cache->content,
				sizeof(static_content) * (cache->content_count + cache_content_capacity));
		if (newmem == NULL)
		{
			log_error("out of memory");
			return false;
		}
		cache->content = newmem;
	}

	// put the result in the cache
	cache->content[cache->content_count++] = (static_content){
			.uri = (hiw_string){ .begin = filename_copy, .length = filename_length },
			.mime_type = static_cache_get_mime_type(
					(hiw_string){ .begin = filename_copy, .length = filename_length }),
			.memory = buf,
			.length = size,
	};

	log_infof("Cached '%s' as '%.*s'", filename, filename_length, filename_copy);
	return true;
}

#if defined(_WIN32) || defined(WIN32)

#include <windows.h>

bool load_files(static_cache* cache, const char* sub_dir)
{
	WIN32_FIND_DATA fd;
	HANDLE handle = NULL;

	char path[2048];
	sprintf(path, "%s\\*.*", sub_dir);
	log_infof("Scanning %s", path);

	if ((handle = FindFirstFile(path, &fd)) == INVALID_HANDLE_VALUE)
	{
		log_errorf("Could not find files in %s", path);
		return false;
	}

	do
	{
		// ignore . and ..
		if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
		{
			sprintf(path, "%s\\%s", sub_dir, fd.cFileName);

			// folder or file?
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				load_files(cache, path);
			}
			else
			{
				static_cache_add(cache, path);
			}
		}

	} while (FindNextFile(handle, &fd));

	FindClose(handle);
	return true;
}

bool load_files_base(static_cache* cache, hiw_string base_dir)
{
	WIN32_FIND_DATA fd;
	HANDLE handle = NULL;

	char path[2048];
	sprintf(path, "%.*s\\*.*", base_dir.length, base_dir.begin);
	log_infof("Scanning %s", path);

	if ((handle = FindFirstFile(path, &fd)) == INVALID_HANDLE_VALUE)
	{
		log_errorf("Could not find files in %s", path);
		return false;
	}

	do
	{
		// ignore . and ..
		if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
		{
			sprintf(path, "%.*s\\%s", base_dir.length, base_dir.begin, fd.cFileName);

			// folder or file?
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				load_files(cache, path);
			}
			else
			{
				static_cache_add(cache, path);
			}
		}

	} while (FindNextFile(handle, &fd));

	FindClose(handle);
	return true;
}

#else
#endif

bool static_cache_init(static_cache* cache, hiw_string base_dir)
{
	if (!hiw_memory_dynamic_init(&cache->memory, (64 * 1024)))
		return false;
	cache->base_dir = base_dir;
	cache->content = malloc(sizeof(static_content) * cache_content_capacity);
	cache->content_count = 0;
	if (!load_files_base(cache, base_dir))
	{
		free(cache->content);
		hiw_memory_release(&cache->memory);
		return false;
	}
	return true;
}

void static_cache_release(static_cache* cache)
{
	if (cache->content)
		free(cache->content);
	hiw_memory_release(&cache->memory);
}

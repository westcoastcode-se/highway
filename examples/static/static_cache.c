//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "static_cache.h"
#include "hiw_file_content.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)

#endif

// default cache capacity
static const int cache_content_capacity = 64;

bool static_cache_add(static_cache* cache, const hiw_file* file)
{
	log_infof("Caching path: '%.*s', filename: '%.*s', suffix: '%.*s'",
		file->path.length, file->path.begin,
		file->filename.length, file->filename.begin,
		file->suffix.length, file->suffix.begin);

	// Copy the filename into the memory buffer
	const int filename_length = (int)file->path.length - cache->base_dir.length;
	char* const filename_copy = hiw_memory_get(&cache->memory, filename_length);
	hiw_std_mempy(file->path.begin + cache->base_dir.length, filename_length, filename_copy, filename_length);

	FILE* f = fopen(file->path.begin, "rb");
	if (f == NULL)
	{
		log_errorf("could not open file '%s'", file->filename.begin);
		return false;
	}

	fseek(f, 0, SEEK_END);
	const int size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = hiw_memory_get(&cache->memory, size);
	if (buf == NULL)
	{
		log_error("out of memory");
		return false;
	}

	if (fread(buf, size, 1, f) != 1)
	{
		log_errorf("failed to read '%s' into memory", file->filename.begin);
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
			.uri = (hiw_string){.begin = filename_copy, .length = filename_length},
			.mime_type = hiw_mimetype_from_filename(
					(hiw_string){.begin = filename_copy, .length = filename_length}),
			.memory = buf,
			.length = size,
	};

	log_infof("cached '%s' as '%.*s'", file->path.begin, filename_length, filename_copy);
	return true;
}

bool file_found(const hiw_file* f, void* userdata)
{
	static_cache* cache = (static_cache*)userdata;
	static_cache_add(cache, f);
	return true;
}

bool static_cache_init(static_cache* cache, hiw_string base_dir)
{
	if (!hiw_memory_dynamic_init(&cache->memory, (64 * 1024)))
	{
		log_error("could not initialize static cache");
		return false;
	}
	cache->base_dir = base_dir;
	cache->content = malloc(sizeof(static_content) * cache_content_capacity);
	cache->content_count = 0;

	// Traverse all files
	const hiw_file_traverse_error err = hiw_file_traverse(base_dir, file_found, cache);
	if (err != HIW_FILE_TRAVERSE_ERROR_SUCCESS)
	{
		log_errorf("failed to traverse static content directory: %d", (int)err);
		free(cache->content);
		hiw_memory_release(&cache->memory);
		return false;
	}
	return true;
}

void static_cache_release(static_cache* cache)
{
	if (cache->content) free(cache->content);
	hiw_memory_release(&cache->memory);
}
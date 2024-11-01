//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef _STATIC_CACHE_H_
#define _STATIC_CACHE_H_

#include <highway.h>

/**
 * @brief One static content
 */
typedef struct static_content
{
    // the URI this content is associated with
    hiw_string uri;

    // the mime type of the file
    hiw_string mime_type;

    // memory where the content is located
    const char* memory;

    // the length of the memory
    int length;
} static_content;

/**
 * @brief A cache for all static content
 */
typedef struct static_cache
{
    // the base data dir
    hiw_string base_dir;

    // memory for cached content, including their filenames
    hiw_memory memory;

    // the actuasl content
    static_content* content;

    // how much content there is
    int content_count;
} static_cache;

/**
 * @brief Initialize the static cache
 * @param cache 
 * @param base_dir 
 * @return 
 */
extern bool static_cache_init(static_cache* cache, hiw_string base_dir);

extern void static_cache_release(static_cache* cache);

#endif

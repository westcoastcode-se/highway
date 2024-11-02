//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef HIW_MIMETYPES_H
#define HIW_MIMETYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hiw_std.h"

struct HIW_PUBLIC hiw_mimetypes_s
{
    const hiw_string text_css;
    const hiw_string text_html;
    const hiw_string text_javascript;
    const hiw_string text_plain;

    const hiw_string image_jpeg;
    const hiw_string image_png;

    const hiw_string application_json;

    const hiw_string application_octet_stream;
};
typedef struct hiw_mimetypes_s hiw_mimetypes_s;

/**
 * mime types
 */
HIW_PUBLIC extern const hiw_mimetypes_s hiw_mimetypes;

/**
 * @brief figure out the mimetype based on the filename
 * @param filename the filename
 * @return the mimetype, such as "text/html"
 */
HIW_PUBLIC extern hiw_string hiw_mimetype_from_filename(hiw_string filename);

/**
 * @brief figure out the mimetype based on the suffix - for example: ".html"
 * @param suffix the suffix
 * @return the mimetype, such as "text/html"
 */
HIW_PUBLIC extern hiw_string hiw_mimetype_from_suffix(hiw_string suffix);

#ifdef __cplusplus
}
#endif

#endif//HIW_MIMETYPES_H

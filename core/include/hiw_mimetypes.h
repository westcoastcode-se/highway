//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef HIW_MIMETYPES_H
#define HIW_MIMETYPES_H

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
};
typedef struct hiw_mimetypes_s hiw_mimetypes_s;

/**
 * mime types
 */
HIW_PUBLIC extern const hiw_mimetypes_s hiw_mimetypes;

#endif//HIW_MIMETYPES_H

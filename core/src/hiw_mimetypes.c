//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_mimetypes.h"

const hiw_mimetypes_s hiw_mimetypes =
{
		{.begin = "text/css", .length = hiw_string_const_len("text/css")},
		{.begin = "text/html", .length = hiw_string_const_len("text/html")},
		{.begin = "text/javascript", .length = hiw_string_const_len("text/javascript")},
		{.begin = "text/plain", .length = hiw_string_const_len("text/plain")},
		{.begin = "image/jpeg", .length = hiw_string_const_len("image/jpeg")},
		{.begin = "image/png", .length = hiw_string_const_len("image/png")},
		{.begin = "application/json", .length = hiw_string_const_len("application/json")},
};

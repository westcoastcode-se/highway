//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_mimetypes.h"

const hiw_mimetypes_s hiw_mimetypes = {
	{.begin = "text/css", .length = hiw_string_const_len("text/css")},
	{.begin = "text/html", .length = hiw_string_const_len("text/html")},
	{.begin = "text/javascript", .length = hiw_string_const_len("text/javascript")},
	{.begin = "text/plain", .length = hiw_string_const_len("text/plain")},
	{.begin = "image/jpeg", .length = hiw_string_const_len("image/jpeg")},
	{.begin = "image/png", .length = hiw_string_const_len("image/png")},
	{.begin = "application/json", .length = hiw_string_const_len("application/json")},
	{.begin = "application/octet-stream", .length = hiw_string_const_len("application/octet-stream")},
};

hiw_string hiw_mimetype_from_filename(hiw_string filename)
{
	const hiw_string suffix = hiw_string_suffix(filename, '.');
	return hiw_mimetype_from_suffix(suffix);
}

hiw_string hiw_mimetype_from_suffix(hiw_string suffix)
{
	if (hiw_string_cmp(suffix, hiw_string_const(".css")))
		return hiw_mimetypes.text_css;
	if (hiw_string_cmp(suffix, hiw_string_const(".html")))
		return hiw_mimetypes.text_html;
	if (hiw_string_cmp(suffix, hiw_string_const(".js")))
		return hiw_mimetypes.text_javascript;
	if (hiw_string_cmp(suffix, hiw_string_const(".txt")))
		return hiw_mimetypes.text_plain;

	if (hiw_string_cmp(suffix, hiw_string_const(".jpg")))
		return hiw_mimetypes.image_jpeg;
	if (hiw_string_cmp(suffix, hiw_string_const(".jpeg")))
		return hiw_mimetypes.image_jpeg;
	if (hiw_string_cmp(suffix, hiw_string_const(".png")))
		return hiw_mimetypes.image_png;

	if (hiw_string_cmp(suffix, hiw_string_const(".json")))
		return hiw_mimetypes.application_json;

	return hiw_mimetypes.application_octet_stream;
}

//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include <hiw_boot.h>

void on_request(hiw_request* const req, hiw_response* const resp)
{
	if (hiw_string_cmp(hiw_request_get_uri(req), hiw_string_const("/")))
	{
		const hiw_string json = hiw_string_const("{\"name\":\"John Doe\"}");

		hiw_response_set_status_code(resp, 200);
		hiw_response_set_content_length(resp, json.length);
		hiw_response_set_content_type(resp, hiw_mimetypes.application_json);
		hiw_response_write_body_raw(resp, json.begin, json.length);
		return;
	}

	hiw_response_set_status_code(resp, 404);
}

int hiw_boot_init(hiw_boot_config* config)
{
	// Configure the Highway Boot Framework
	config->servlet_func = on_request;

	// Start
	return hiw_boot_start(config);
}

//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include <hiw_boot.h>
#include <string>

class JsonDrive
{
public:
	JsonDrive()
	{
	}

private:
	std::string mPath;
};

void on_request(hiw_request* const req, hiw_response* const resp)
{
	if (hiw_string_cmp(req->method, hiw_string_const("GET")))
	{

	}
	else if (hiw_string_cmp(req->method, hiw_string_const("PUT")))
	{

	}
	else if (hiw_string_cmp(req->method, hiw_string_const("DELETE")))
	{

	}

	hiw_response_set_status_code(resp, 404);
}

void hiw_boot_init(hiw_boot_config* config)
{
	// Configure the Highway Boot Framework
	config->servlet_func = on_request;

	// Start
	hiw_boot_start(config);
}

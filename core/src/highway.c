//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "highway.h"
#include "hiw_logger.h"

hiw_init_config* hiw_internal_config()
{
	static hiw_init_config config;
	return &config;
}

bool hiw_init(hiw_init_config config)
{
	// keep track of the configuration used when initializing highway
	*hiw_internal_config() = config;

#if defined(_WIN32) || defined(WIN32)
	if (config.initialize_sockets)
	{
		WSADATA WSAData;
		const int ret = WSAStartup(MAKEWORD(2, 2), &WSAData);
		if (ret != 0)
		{
			WSACleanup();
			log_errorf("WSAStartup failed: %d", ret);
			return false;
		}
	}
#else
#endif

	// todo initialize thread memory

	return true;
}

void hiw_release()
{
	// todo release thread memory

#if defined(_WIN32) || defined(WIN32)
	if (hiw_internal_config()->initialize_sockets)
	{
		WSACleanup();
	}
#else
#endif
}

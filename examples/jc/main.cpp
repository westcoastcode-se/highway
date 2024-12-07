//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include <cstdio>
#include <exception>
#include <hiw_boot.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

using string_view = std::string_view;
using string = std::string;
using stringstream = std::stringstream;

using namespace std::literals;

struct string_hash
{
	using hash_type = std::hash<std::string_view>;
	using is_transparent = void;

	std::size_t operator()(const char* str) const { return hash_type{}(str); }
	std::size_t operator()(const string_view str) const { return hash_type{}(str); }
	std::size_t operator()(string const& str) const { return hash_type{}(str); }
};

typedef std::unordered_map<string, string, string_hash, std::equal_to<>> string_map;

class json_storage
{
public:
	json_storage(string_view data_dir) : mPath(data_dir) {}

	string_view get(const string_view key, const string_view default_value) { return get_view(key, default_value); }

	string add(const string_view key, string new_data) { return add_view(key, std::move(new_data)); }

	string remove(const string_view key) { return remove_view(key); }

private:
	string remove_view(const string_view key)
	{
		const auto path = secure_path(key);
		string old_value;

		std::lock_guard l(mMutex);
		if (const auto it = mCache.find(key); it != mCache.end())
		{
			old_value = std::move(it->second);
			mCache.erase(it);
		}

		// Remove the old json content on disk if found. First, remove the object from the trash if it exists
		if (!old_value.empty())
		{
			const string removedFile = path + ".trash";
			::remove(removedFile.c_str());
			::rename(path.c_str(), removedFile.c_str());
		}

		// Return the old value
		return old_value;
	}

	string add_view(const string_view key, string new_data)
	{
		const auto path = secure_path(key);
		string old_value;

		std::lock_guard l(mMutex);
		if (const auto it = mCache.find(key); it != mCache.end())
		{
			old_value = std::move(it->second);
			mCache.erase(it);
		}

		// Try to write the new content
		FILE* fp = fopen(path.c_str(), "wb");
		if (fp == nullptr)
			throw std::runtime_error("could not open resource on disk");
		fwrite(new_data.c_str(), new_data.length(), 1, fp);
		fclose(fp);

		// Save the new content
		mCache.emplace(key, std::move(old_value));
		return old_value;
	}

	string_view get_view(const string_view key, const string_view default_value)
	{
		const auto path = secure_path(key);

		std::lock_guard l(mMutex);
		auto it = mCache.find(key);
		if (it != mCache.end())
			return string_view(it->second);

		FILE* fp = fopen(path.c_str(), "rb");
		if (fp == nullptr)
			return default_value;

		// Figure out the file size
		fseek(fp, 0, SEEK_END);
		const auto length = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		// Larger than 4 mb?
		if (length > 4096 * 1024)
			throw std::runtime_error("file is too large");

		// Read the data from the HDD
		string data;
		data.resize(length);
		fread(const_cast<char*>(data.c_str()), length, 1, fp);
		fclose(fp);

		mCache.emplace(key, std::move(data));
		it = mCache.find(key);
		return string_view(it->second);
	}

	[[nodiscard]] static string normalize_path(const string_view relative_path)
	{
		// Assume that we've verified the path to the resource (i.e. length > 0 and first character is "/")
		stringstream result;
		result << "/";
		const auto size = relative_path.length();
		for (int i = 1; i < size; ++i)
		{
			const auto c = relative_path[i];
			if (c >= 'a' && c <= 'z')
				result << c;
			else if (c >= 'A' && c <= 'Z')
				result << c;
			else if (c >= '0' && c <= '9')
				result << c;
			else if (c == '_')
				result << c;
			else
				result << "+";
		}
		result << ".json";
		return result.str();
	}

	[[nodiscard]] string secure_path(const string_view relative_path) const
	{
		if (relative_path.empty() || relative_path[0] != '/')
			throw std::runtime_error("invalid path");
		return mPath + normalize_path(relative_path);
	}

	string mPath;
	std::mutex mMutex;
	string_map mCache;
};

void on_request(hiw_request* const req, hiw_response* const resp)
{
	const auto storage = static_cast<json_storage*>(hiw_boot_get_userdata());
	const auto uri = string_view(req->uri.begin, req->uri.length);
	try
	{
		if (hiw_string_cmp(req->method, hiw_string_const("GET")))
		{
			const auto result = storage->get(uri, string_view());
			if (result.empty())
			{
				log_warnf("could not find %.*s", req->uri.length, req->uri.begin);
				hiw_response_set_status_code(resp, 404);
				return;
			}

			hiw_response_set_status_code(resp, 200);
			hiw_response_set_content_length(resp, result.length());
			hiw_response_set_content_type(resp, hiw_mimetypes.application_json);
			hiw_response_write_body_raw(resp, result.data(), result.length());
			return;
		}

		if (hiw_string_cmp(req->method, hiw_string_const("PUT")))
		{
			if (req->content_length == 0)
			{
				log_warnf("request content-length is zero for %.*s", req->uri.length, req->uri.begin);
				hiw_response_set_status_code(resp, 400);
				return;
			}

			// Read the incoming data
			string new_data;
			new_data.resize(req->content_length);
			hiw_request_recv(req, const_cast<char*>(new_data.c_str()), req->content_length);

			// add the data
			const auto old_data = storage->add(uri, std::move(new_data));

			// return the old data
			hiw_response_set_status_code(resp, 200);
			hiw_response_set_content_length(resp, old_data.length());
			hiw_response_set_content_type(resp, hiw_mimetypes.application_json);
			hiw_response_write_body_raw(resp, old_data.data(), old_data.length());
			return;
		}

		if (hiw_string_cmp(req->method, hiw_string_const("DELETE")))
		{
			const auto removed_data = storage->remove(uri);
			if (removed_data.empty())
			{
				log_warnf("could not find %.*s", req->uri.length, req->uri.begin);
				hiw_response_set_status_code(resp, 404);
				return;
			}

			// return the removed data
			hiw_response_set_status_code(resp, 200);
			hiw_response_set_content_length(resp, removed_data.length());
			hiw_response_set_content_type(resp, hiw_mimetypes.application_json);
			hiw_response_write_body_raw(resp, removed_data.data(), removed_data.length());
			return;
		}
	}
	catch (std::exception& e)
	{
		log_errorf("unhandled exception: %s", e.what());
		hiw_response_set_status_code(resp, 400);
		return;
	}

	hiw_response_set_status_code(resp, 404);
}

int hiw_boot_init(hiw_boot_config* config)
{
	string_view data_dir("data");
	if (config->argc > 1)
	{
		if (hiw_string_cmpc(hiw_string_const("--help"), config->argv[1], (int)strlen(config->argv[1])))
		{
			fprintf(stdout, "Usage: jc [data-dir]\n");
			fprintf(stdout, "\n");
			fprintf(stdout,
					"\tdata-dir - is the path to the data directory where the json data is saved. Default: 'data'\n");
			fprintf(stdout, "\n");
			return 0;
		}

		data_dir = string_view(config->argv[1], (int)strlen(config->argv[1]));
	}

	json_storage storage(data_dir);

	// Configure the Highway Boot Framework
	config->servlet_func = on_request;
	config->userdata = &storage;

	// Start
	return hiw_boot_start(config);
}

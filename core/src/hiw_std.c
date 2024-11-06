//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_std.h"
#include <assert.h>
#include <ctype.h>

bool hiw_string_isspace(char c)
{
	switch (c)
	{
	case '\r':
	case '\n':
	case ' ':
	case '\t':
		return true;
	default:
		return false;
	}
}

bool hiw_string_cmp(const hiw_string s1, const hiw_string s2)
{
	if (s1.length != s2.length)
		return false;

	const char* c1 = s1.begin;
	const char* const end = c1 + s1.length;
	const char* c2 = s2.begin;

	for (; c1 != end; c1++, c2++)
		if (*c1 != *c2)
			return false;

	return true;
}

bool hiw_string_cmpc(const hiw_string s1, const char* c, const int n)
{
	if (s1.length != n)
		return false;

	const char* c1 = s1.begin;
	const char* const end = c1 + n;

	for (; c1 != end; c1++, c++)
		if (*c1 != *c)
			return false;

	return true;
}

bool hiw_string_readline(const hiw_string* str, hiw_string* dest)
{
	assert(dest != NULL);
	if (dest == NULL || str->length == 0)
		return false;

	const char* c = dest->begin = str->begin;
	const char* const end = str->begin + str->length;
	for (; c != end && *c != '\n'; ++c)
		;

	if (c != end)
	{
		dest->length = (int)(c - dest->begin);
		return true;
	}

	dest->length = 0;
	return false;
}

hiw_string hiw_string_rtrim(hiw_string str)
{
	// string is empty, so no trim is necessary
	if (str.length == 0)
		return (hiw_string){.length = 0};

	const char* start = str.begin;
	const char* end = str.begin + str.length;

	// right trim
	while (end != start && hiw_string_isspace(*--end))
		;

	// the last character is a whitespace
	if (hiw_string_isspace(*end))
		return (hiw_string){.length = 0};

	// return a string based on it
	return (hiw_string){.begin = start, .length = (int)(end - start) + 1};
}

hiw_string hiw_string_trim(const hiw_string str)
{
	if (str.length == 0)
		return (hiw_string){.length = 0};

	const char* start = str.begin;
	const char* end = str.begin + str.length;

	// left trim
	while (start != end && hiw_string_isspace(*start))
		start++;

	// right trim
	while (end != start && hiw_string_isspace(*--end))
		;

	// the last character is a whitespace
	if (hiw_string_isspace(*end))
		return (hiw_string){.length = 0};

	// return a string based on it
	return (hiw_string){.begin = start, .length = (int)(end - start) + 1};
}

hiw_string hiw_string_suffix(hiw_string str, char delim)
{
	if (str.length <= 1)
		return (hiw_string){.length = 0};

	const char* start = str.begin;
	const char* end = str.begin + str.length;

	// seek backwards
	while (end != start && *--end != delim)
		;

	// the last character might be a delimiter
	if (start == end && *start == delim)
		return str;

	start = end;
	end = str.begin + str.length;
	return (hiw_string){.begin = start, .length = (int)(end - start)};
}

const char* hiw_std_ctoui(const char* const str, const int n, unsigned int* i)
{
	const char* s = str;
	const char* const end = s + n;

	unsigned int num = 0;
	for (; s != end; ++s)
	{
		if (!isdigit(*s))
			return s;
		num = (num * 10) + (*s - '0');
	}

	*i = num;
	return s;
}

const char* hiw_std_ctoi(const char* const str, const int n, int* i)
{
	const char* s = str;
	const char* const end = s + n;

	int multiplier = 1;
	if (s != end && *s == '-')
	{
		multiplier = -1;
		++s;
	}

	int num = 0;
	for (; s != end; ++s)
	{
		if (!isdigit(*s))
			return s;
		num = (num * 10) + (*s - '0');
	}

	*i = num * multiplier;
	return s;
}

extern char* hiw_std_uitoc(char* const dest, const int n, unsigned int val)
{
	static const char base[] = {"0123456789"};
	if (n == 0 || dest == NULL)
		return NULL;

	if (val == 0)
	{
		*dest = base[0];
		return dest + 1;
	}

	// itoa in reversed order
	char* ptr = dest;
	const char* const end = ptr + n;
	for (; ptr != end && val; ++ptr)
	{
		*ptr = base[val % 10];
		val /= 10;
	}

	// mirror the result
	const int written = (int)(ptr - dest);
	const int mid = written / 2;
	for (int i = 0; i < mid; ++i)
	{
		const char tmp = dest[i];
		dest[i] = dest[written - i - 1];
		dest[written - i - 1] = tmp;
	}
	return ptr;
}

hiw_string hiw_string_toui(hiw_string str, unsigned int* i)
{
	const char* ptr = hiw_std_ctoui(str.begin, str.length, i);
	const int length = (int)((str.begin + str.length) - ptr);
	return (hiw_string){.begin = ptr, .length = length};
}

hiw_string hiw_string_toi(hiw_string str, int* i)
{
	const char* ptr = hiw_std_ctoi(str.begin, str.length, i);
	const int length = (int)((str.begin + str.length) - ptr);
	return (hiw_string){.begin = ptr, .length = length};
}

int hiw_string_split(hiw_string* str, char delim, hiw_string* dest, int n)
{
	assert(dest != NULL);
	if (str->length == 0 || n <= 0 || dest == NULL)
		return 0;

	const char* start = str->begin;
	const char* split_end = start;
	const char* const end = str->begin + str->length;
	int num = 0;

	for (; num < (n - 1); ++num)
	{
		// seek until we find the delimiter
		for (; split_end != end && *split_end != delim; ++split_end)
			;

		// did we reach the end?
		if (split_end == end)
			break;

		hiw_string_set(&dest[num], start, (int)(split_end - start));
		split_end++; // skip the delim
		start = split_end;
	}

	hiw_string_set(&dest[num], start, (int)(end - start));
	num++;
	return num;
}

void hiw_string_set(hiw_string* str, const char* s, int len)
{
	str->begin = s;
	str->length = len;
}

void hiw_memory_fixed_init(hiw_memory* m, char* buf, int capacity)
{
	assert(m);

	m->pos = m->ptr = buf;
	m->end = m->pos + capacity;
	m->resize_bytes = -1;
}

bool hiw_memory_dynamic_init(hiw_memory* m, int capacity)
{
	assert(m && "expected 'm' to be set");
	if (m == NULL)
		return false;

	assert(capacity > 0 && "expected 'capacity' to be a valid value");
	if (capacity <= 0)
		return false;

	m->pos = m->ptr = malloc(capacity);
	if (m->ptr == NULL)
		return false;
	m->end = m->pos + capacity;
	m->resize_bytes = capacity;
	return true;
}

void hiw_memory_release(hiw_memory* mem)
{
	// no need to free anything
	if (mem->resize_bytes == -1)
		return;

	// release the internal memory if needed
	if (mem->ptr)
	{
		free(mem->ptr);
		mem->end = mem->pos = mem->ptr = NULL;
		mem->resize_bytes = 0;
	}
}

bool hiw_memory_resize_enabled(hiw_memory* m) { return m->resize_bytes <= 0; }

int hiw_memory_size(hiw_memory* const m) { return (int)(m->pos - m->ptr); }

int hiw_memory_capacity(hiw_memory* const m) { return (int)(m->end - m->ptr); }

bool hiw_memory_resize(hiw_memory* const m, const int required_bytes)
{
	if (!hiw_memory_resize_enabled(m))
	{
		assert(false && "hiw_memory is out of memory");
		return false;
	}

	// figure out how many bytes we should resize to
	int new_capacity = hiw_memory_capacity(m) + m->resize_bytes;
	while (new_capacity < required_bytes)
		new_capacity += m->resize_bytes;

	// try to allocate new memory for our buffer
	char* new_memory = realloc(m->ptr, new_capacity);
	assert(new_memory && "out of memory");
	if (new_memory == NULL)
		return false;

	const int length = hiw_memory_size(m);
	m->ptr = new_memory;
	m->pos = new_memory + length;
	m->end = new_memory + new_capacity;
	return true;
}

void hiw_memory_reset(hiw_memory* m) { m->pos = m->ptr; }

char* hiw_memory_get(hiw_memory* m, int n)
{
	if (m->pos + n > m->end)
	{
		if (hiw_memory_resize(m, hiw_memory_size(m) + n))
		{
			return NULL;
		}
	}

	char* ret = m->pos;
	m->pos += n;
	return ret;
}

char* hiw_std_mempy(const char* src, const int n, char* dest, int capacity)
{
	// nothing to copy
	if (src == NULL || n == 0)
		return dest;
	if (dest == NULL || capacity == 0)
		return NULL;

	// clamp to the maximum number of bytes
	capacity = n > capacity ? capacity : n;
	for (const char* const end = src + capacity; src != end; src++, dest++)
		*dest = *src;

	return dest;
}

char* hiw_std_copy(const char* src, int n, char* dest, int capacity) { return hiw_std_mempy(src, n, dest, capacity); }

char* hiw_std_copy0(const char* src, char* dest, int capacity)
{
	for (int i = 0; i < capacity && *src; i++)
		*dest++ = *src++;
	return dest;
}

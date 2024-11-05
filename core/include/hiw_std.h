//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef hiw_STD_H
#define hiw_STD_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(HIW_HTTP_STATIC_LIB)
#	define HIW_EXPORT
#	define HIW_IMPORT
#else
#	if defined(_MSC_VER)
#		define HIW_EXPORT __declspec(dllexport)
#		define HIW_IMPORT __declspec(dllimport)
#	elif defined(__GNUC__)
#		define HIW_EXPORT __attribute__((visibility("default")))
#		define HIW_IMPORT
#	else
#		error Unknown dynamic link import/export semantics.
#	endif
#endif

#if WCC_HTTP_SLIB_COMPILING
#	if defined(__cplusplus)
#		define HIW_PUBLIC HIW_EXPORT
#	else
#		define HIW_PUBLIC HIW_EXPORT
#	endif
#else
#	if defined(__cplusplus)
#		define HIW_PUBLIC HIW_IMPORT
#	else
#		define HIW_PUBLIC HIW_IMPORT
#	endif
#endif

#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#	define HIW_WINDOWS 1
#elif defined(__APPLE__)
#	define HIW_APPLE 1
#else
#	define HIW_LINUX 1
#endif

#define HIGHWAY_VERSION "0.0.1"

/**
 * Check if the supplied bits are set in value
 */
#define hiw_bit_test(value, bits) (((value) & (bits)) == bits)

/**
 * a read-only view of memory
 */
struct HIW_PUBLIC hiw_memory_view
{
    // a pointer to where the memory begins
    const void* begin;

    // the length of the memory
    int length;
};

typedef struct hiw_memory_view hiw_memory_view;

/**
 * a read-only string view from already existing memory
 */
struct HIW_PUBLIC hiw_string
{
    union
    {
        hiw_memory_view _memory;

        struct
        {
            // a pointer to where the string begins
            const char* begin;

            // the length of the string
            int length;
        };
    };
};

typedef struct hiw_string hiw_string;

// figure out a compile-time constant's length
#define hiw_string_const_len(str) (sizeof(str) - 1)

// get a compile time string in hiw_string format
#ifdef __cplusplus
# define hiw_string_const(str) {str, hiw_string_const_len(str)}
#else
# define hiw_string_const(str) (hiw_string){.begin=str, .length=hiw_string_const_len(str)}
#endif

/**
 * compare two hiw_strings
 *
 * @param s1 the first string
 * @param s2 the second string
 * @return true if both strings are identical
 */
HIW_PUBLIC extern bool hiw_string_cmp(hiw_string s1, hiw_string s2);

/**
 * @brief Copy the source string into the destination buffer
 * @param s1 The string
 * @param c A pointer to the char array
 * @param n The numer of bytes in the char array
 * @return true if the two strings are identical
 */
HIW_PUBLIC extern bool hiw_string_cmpc(hiw_string s1, const char* c, int n);

/**
 * read a line from a string view and put the result in the destination buffer
 *
 * @param str
 * @param dest
 * @return
 */
HIW_PUBLIC extern bool hiw_string_readline(const hiw_string* str, hiw_string* dest);

/**
 * @brief Take the supplied string and trim it from right
 * @param str The string to be trimmed
 * @return A trimmed version of the string
 */
HIW_PUBLIC extern hiw_string hiw_string_rtrim(hiw_string str);

/**
 * @brief Take the supplied string and trim it from both left and right
 * @param str The string to be trimmed
 * @return A trimmed version of the string
 */
HIW_PUBLIC extern hiw_string hiw_string_trim(hiw_string str);

/**
 * @brief Try to get a suffix, seeking backwards
 */
HIW_PUBLIC extern hiw_string hiw_string_suffix(hiw_string str, char delim);

/**
* convert the supplied string and put the result into the supplied unsigned integer
* returns a pointer to the first character when it no longer contains an integer
*
* @param str the string
* @param n the number of characters in the string
* @param i the result
* @return
*/
HIW_PUBLIC extern const char* hiw_std_ctoui(const char* const str, int n, unsigned int* i);

/**
* convert the supplied string and put the result into the supplied integer
* returns a pointer to the first character when it no longer contains an integer
*
* @param str the string
* @param n the number of characters in the string
* @param i the result
* @return
*/
HIW_PUBLIC extern const char* hiw_std_ctoi(const char* const str, int n, int* i);

/**
* convert the supplied unsigned integer and put the result into the destination buffer
*
* @param dest the destination buffer
* @param n the number of characters in the destination buffer
* @param i the value
* @return a pointer to where the library stopped writing into the memory buffer
*/
HIW_PUBLIC extern char* hiw_std_uitoc(char* const dest, int n, unsigned int i);

/**
 * @brief convert the supplied string and put the result into the supplied unsigned integer
 * @param str
 * @param i
 * @return
 */
HIW_PUBLIC extern hiw_string hiw_string_toui(hiw_string str, unsigned int* i);

/**
 * @brief convert the supplied string and put the result into the supplied integer
 * @param str
 * @param i
 * @return
 */
HIW_PUBLIC extern hiw_string hiw_string_toi(hiw_string str, int* i);

/**
 *
 * @param str
 * @param delim
 * @param dest
 * @param n
 * @return
 */
HIW_PUBLIC extern int hiw_string_split(hiw_string* str, char delim, hiw_string* dest, int n);

/**
 *
 * @param str
 * @param s
 * @param len
 */
HIW_PUBLIC extern void hiw_string_set(hiw_string* str, const char* s, int len);

/**
 * @brief A generic binary memory pool. If the memory is initialized with data from the
 *        stack then automatic resizing of the underlying memory will not be done
 */
struct HIW_PUBLIC hiw_memory
{
    // the starting memory location of the memory
    char* ptr;

    // the current position in the memory
    char* pos;

    // the end
    const char* end;

    // how many bytes should we allow to resize this buffer. If this is zero then we
    // assume that the memory pool is not a dynamic memory pool and doens't do
    // malloc and free
    int resize_bytes;
};

typedef struct hiw_memory hiw_memory;

/**
 * @brief Initialize a memory object connecting it a fixed memory block - normally memory on the stack.
 *        This type of memory will not be resized by hiw_memory because we don't know for sure if the
 *        memory is a fixed heap-based memory or if from the stack
 * @param m A memory instance
 * @param buf A pointer to the memory we are allowed to use
 * @param capacity How many bytes are we allowed to write to the supplied memory buffer
 */
HIW_PUBLIC extern void hiw_memory_fixed_init(hiw_memory* m, char* buf, int capacity);

/**
 * @brief Initialize memory on the heap with the supplied capacity. The memory will, automatically, resize itself when needed
 * @param m
 * @param capacity
 * @return
 */
HIW_PUBLIC extern bool hiw_memory_dynamic_init(hiw_memory* m, int capacity);

/**
 * @brief Release the hiw_memory instance's internal memory.
 *        If the used memory is from the stack, then nothing will happen when calling this.
 * @param m the hiw_memory instance
 */
HIW_PUBLIC extern void hiw_memory_release(hiw_memory* m);

/**
 * @brief Check if the supplied memory buffer is allowed to resize it's internal memory
 * @param m The memory buffer
 */
HIW_PUBLIC extern bool hiw_memory_resize_enabled(hiw_memory* m);

/**
 * @brief Get how many bytes is written to the supplied hiw_memory instance
 * @param m The hiw_memory instance
 * @return The number of bytes
 */
HIW_PUBLIC extern int hiw_memory_size(hiw_memory* m);

/**
 * @brief Get the memory capacity, in bytes, of the hiw_memory instance
 * @param mem The hiw_memory instance
 * @return The capacity in bytes
 */
HIW_PUBLIC extern int hiw_memory_capacity(hiw_memory* m);

/**
 * @brief Reset the underlying memory for reuse
 * @param mem The hiw_memory instance
 */
HIW_PUBLIC extern void hiw_memory_reset(hiw_memory* m);

/**
 * @brief Try to get n amount of memory
 * @param mem The hiw_memory instance
 * @param n The number of bytes we want to get
 * @return A pointer to the memory location we are allowed to write to. NULL we are out of memory
 */
HIW_PUBLIC extern char* hiw_memory_get(hiw_memory* m, int n);

/**
 * Copy raw bytes from a view or memory
 * @param src the source memory
 * @param n Length of the source memory
 * @param dest the destination buffer
 * @param capacity destination buffer capacity
 * @return where the copy stopped in the destination buffer
 */
HIW_PUBLIC extern char* hiw_std_mempy(const char* src, int n, char* dest, int capacity);

 /**
  * @brief Copy the source string into the destination buffer
  * @param src A string with a known length
  * @param n The length of the source string
  * @param dest the destination buffer
  * @param capacity The destination capacity
  * @return A pointer to where the copy stopped
  */
 HIW_PUBLIC extern char* hiw_std_copy(const char* src, int n, char* dest, int capacity);

 /**
  * @brief Compare a hiw_string with a char array
  * @param src A null-terminated string
  * @param dest the destination buffer
  * @param capacity The destination capacity
  * @return A pointer to where the copy stopped
  */
 HIW_PUBLIC extern char* hiw_std_copy0(const char* src, char* dest, int capacity);

#ifdef __cplusplus
}
#endif

#endif //hiw_STD_H

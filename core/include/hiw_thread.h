//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#ifndef HIW_THREAD_H
#define HIW_THREAD_H

#include "hiw_std.h"

#ifdef __cplusplus
extern "C" {
#endif

#define hiw_thread_flags_main (1 << 0)

// Default timeout for when the thread is waiting for a thread to stop running
#if !defined(HIW_THREAD_WAIT_DEFAULT_TIMEOUT)
#define HIW_THREAD_WAIT_DEFAULT_TIMEOUT (30000)
#endif

/**
 * Context that can be used to recursively supply generic data to a thread
 */
struct HIW_PUBLIC hiw_thread_context
{
	// a unique key of this context
	void* key;

	// the value associated with this context
	void* value;

	// The thread this context is part of
	struct hiw_thread* thread;

	// parent context
	struct hiw_thread_context* parent;
};
typedef struct hiw_thread_context hiw_thread_context;

// a function definition for a highway thread
HIW_PUBLIC typedef void (*hiw_thread_fn)(struct hiw_thread*);

/**
 * Represents a thread managed by highway
 */
struct HIW_PUBLIC hiw_thread
{
	// thread flags
	int flags;

	// User-data
	void* data;

	// function to be called when this thread is spawned
	hiw_thread_fn func;

	// the leaf context containing the underlying value
	hiw_thread_context* context;
};
typedef struct hiw_thread hiw_thread;

/**
 * Create memory for a new thread
 *
 * @param fn function to be called
 * @return an instance that represents the thread
 */
HIW_PUBLIC hiw_thread* hiw_thread_new(hiw_thread_fn fn);

/**
 * Wait for the thread to stop. This function should be called when the server has stopped
 * and when the application waits for all servlet threads to stop
 *
 * @param t The thread
 * @param wait_ms How long we're waiting until we forcefully close the thread
 */
HIW_PUBLIC void hiw_thread_wait(hiw_thread* t, int wait_ms);

/**
 * @return The instance that represents the main thread
 */
HIW_PUBLIC hiw_thread* hiw_thread_main();

/**
 * Set the function to be called when starting the supplied thread. You are not allowed to
 * set the function if the thread is already running
 *
 * @param t the thread to set the function to
 * @param fn function to be called
 */
HIW_PUBLIC void hiw_thread_set_func(hiw_thread* t, hiw_thread_fn fn);

/**
 * Set user data
 *
 * @param t the thread to set the data to
 * @param data the user data
 */
HIW_PUBLIC void hiw_thread_set_userdata(hiw_thread* t, void* data);

/**
 * Get user data
 *
 * @param t the thread to get the data from
 */
HIW_PUBLIC void* hiw_thread_get_userdata(hiw_thread* t);

/**
 * Push a new context for a thread
 *
 * @param thread
 * @param value
 * @return the previous context
 */
HIW_PUBLIC hiw_thread_context* hiw_thread_context_push(hiw_thread* thread, hiw_thread_context* value);

/**
 * Pop a new context for a thread
 *
 * @param thread
 * @return the value removed from the thread context stack
 */
HIW_PUBLIC hiw_thread_context* hiw_thread_context_pop(hiw_thread* thread);

/**
 * Pop a new context for a thread
 *
 * @param thread
 * @param key The context key
 * @return The value - if one is found; NULL otherwise
 */
HIW_PUBLIC void* hiw_thread_context_find(hiw_thread* thread, const void* key);

/**
 * Start the supplied thread
 *
 * @param t the thread to start
 */
HIW_PUBLIC bool hiw_thread_start(hiw_thread* t);

/**
 * Destroy the supplied thread
 * @param t The thread we want to destroy
 */
HIW_PUBLIC void hiw_thread_delete(hiw_thread* t);

typedef struct hiw_thread_pool hiw_thread_pool;

/**
 * A thread pool executing generic jobs
 */
struct HIW_PUBLIC hiw_thread_pool
{
	// The number of threads running at this moment
	int count;

	// The maximum number of threads allowed to run
	int max_count;
};

/**
 * create a new thread pool instance
 *
 * @param initial_count the initial thread count
 * @param max_count the maximum number of allowed threads
 * @return a thread pool
 */
extern HIW_PUBLIC hiw_thread_pool* hiw_thread_pool_new(int initial_count, int max_count);

/**
 * @brief delete the supplied thread pool and destroy all it's internal memory
 */
extern HIW_PUBLIC void hiw_thread_pool_delete(hiw_thread_pool* pool);

/**
 * @brief start the supplied thread pool
 * @param pool the thread pool
 */
extern HIW_PUBLIC void hiw_thread_pool_start(hiw_thread_pool* pool);

/**
 * @brief push work to be executed
 */
extern HIW_PUBLIC void hiw_thread_pool_push(hiw_thread_pool* pool, hiw_thread_fn func, void* data);

/**
 * @brief Get the thread pool responsible for running the supplied thread
 */
extern HIW_PUBLIC hiw_thread_pool* hiw_thread_pool_get(hiw_thread* t);

#ifdef __cplusplus
}
#endif

#endif // HIW_THREAD_H

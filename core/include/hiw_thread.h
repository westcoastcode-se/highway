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

// Default timeout for when the thread is waiting for a thread to stop running
#if !defined(HIW_THREAD_WAIT_DEFAULT_TIMEOUT)
#define HIW_THREAD_WAIT_DEFAULT_TIMEOUT (30000)
#endif

/**
 * @brief context that can be used to recursively supply generic data to a thread
 *
 * it is expected that you push and pop data in the appropriate order.
 */
struct HIW_PUBLIC hiw_thread_context
{
	// a unique key of this context
	const void* key;

	// the value associated with this context
	void* value;

	// The thread this context is part of
	struct hiw_thread* thread;

	// parent context
	struct hiw_thread_context* parent;
};

typedef struct hiw_thread_context hiw_thread_context;
typedef struct hiw_thread hiw_thread;

// a function definition for a highway thread
typedef void (*hiw_thread_fn)(hiw_thread*);

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
 * @return A thread instance that represents the main thread
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
 * @brief set user data associated with the supplied thread
 * @param t the thread to set the data to
 * @param data the user data
 */
HIW_PUBLIC void hiw_thread_set_userdata(hiw_thread* t, void* data);

/**
 * @brief Get user data associated with the supplied thread
 * @param t The thread to get the data from
 *
 * Please note that a thread is only allowed to have one user-data associated with it. It's normally
 * used as a way to send data into the thread's entrypoint. If you want to put values onto the thread as
 * thread-local like variables, then consider using hiw_thread_context_push.
 */
HIW_PUBLIC void* hiw_thread_get_userdata(const hiw_thread* t);

/**
 * @brief Push a new context value onto a thread
 * @param t the thread
 * @param value the value
 * @return the previous context
 *
 * You are allowed to push multiple values onto the thread, and they are all searchable using the
 * function hiw_thread_context_find. Please note that you are expected to pop the values
 * when you are done with them
 */
HIW_PUBLIC hiw_thread_context* hiw_thread_context_push(hiw_thread* t, hiw_thread_context* value);

/**
 * @brief Pop a value from the supplied thread
 * @param t the thread
 * @return the value removed from the thread context
 */
HIW_PUBLIC hiw_thread_context* hiw_thread_context_pop(hiw_thread* t);

/**
 * @brief Clear all context values from the thread
 * @param t the thread
 */
HIW_PUBLIC void hiw_thread_context_clear(hiw_thread* t);

/**
 * @brief Search for a context value form the supplied thread
 * @param t the thread
 * @param key A key that represents the context value
 * @return The value, if one is found; NULL otherwise
 */
HIW_PUBLIC void* hiw_thread_context_find(const hiw_thread* t, const void* key);

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

typedef struct hiw_thread_pool_config hiw_thread_pool_config;
typedef struct hiw_thread_pool hiw_thread_pool;

/**
 * @brief thread pool configuration
 */
struct HIW_PUBLIC hiw_thread_pool_config
{
	// The number of threads running at this moment
	int count;

	// The maximum number of threads allowed to run
	int max_count;

	// allow the thread pool to shrink as load goes down
	bool allow_shrink;

	// function called after a new thread is created
	hiw_thread_fn init;

	// function called before a thread is being destroyed
	hiw_thread_fn release;
};

/**
 * @brief Create a new thread pool instance
 * @param config Configuration used by the thread pool during initialization
 * @return A thread pool instance
 */
extern HIW_PUBLIC hiw_thread_pool* hiw_thread_pool_new(const hiw_thread_pool_config* config);

/**
 * @brief Stop the thread pool and then cleanup up all memory associated with it
 * @param pool The thread pool
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

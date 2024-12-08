//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_thread.h"
#include "hiw_logger.h"
#include "hiw_std.h"

#if defined(HIW_WINDOWS)
#include <process.h>
#include <windows.h>
#elif defined(HIW_LINUX)
#include <pthread.h>
#else
#error OSX not implemented
#endif

#ifdef __unix__
#include <pthread.h>
#include <limits.h>
#define HIW_THREAD_HANDLE pthread_t
#elif defined(HIW_WINDOWS)
#include <process.h>
#include <windows.h>
#define HIW_THREAD_HANDLE HANDLE
#endif

#if defined(HIW_WINDOWS)
#define critical_section_init(mutex) InitializeCriticalSection(mutex)
#define critical_cond_init(mutex) InitializeConditionVariable(mutex)
#define critical_section_destroy(mutex)
#define critical_cond_destroy(cond)
#define critical_section_enter(mutex) EnterCriticalSection(mutex)
#define critical_section_exit(mutex) LeaveCriticalSection(mutex)

bool critical_section_wait(const PCONDITION_VARIABLE cond, const PCRITICAL_SECTION mutex, const int millis)
{
	return SleepConditionVariableCS(cond, mutex, millis);
}

typedef unsigned long long hiw_tick_t;

hiw_tick_t hiw_get_tick_count()
{
	return GetTickCount64();
}

#define critical_section_notify(cond) WakeConditionVariable(cond)
#define critical_section_broadcast(cond) WakeAllConditionVariable(cond)
#else
#define critical_section_init(mutex) pthread_mutex_init(mutex, NULL)
#define critical_cond_init(cond) pthread_cond_init(cond, NULL)
#define critical_section_destroy(mutex) pthread_mutex_destroy(mutex)
#define critical_cond_destroy(cond) pthread_cond_destroy(cond)
#define critical_section_enter(mutex) pthread_mutex_lock(mutex)
#define critical_section_exit(mutex) pthread_mutex_unlock(mutex)
#define INFINITE INT_MAX

bool critical_section_wait(pthread_cond_t* cond, pthread_mutex_t* mutex, const int millis)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += millis / 1000;
	ts.tv_nsec += (millis % 1000) * 1000000;
	return pthread_cond_timedwait(cond, mutex, &ts);
}

typedef unsigned long long hiw_tick_t;

hiw_tick_t hiw_get_tick_count()
{
	struct timespec ts;
	return (uint64_t)(ts.tv_nsec / 1000000) + ((uint64_t)ts.tv_sec * 1000ull);
}

#define critical_section_notify(cond) pthread_cond_signal(cond)
#define critical_section_broadcast(cond) pthread_cond_broadcast(cond)
#endif

#include <assert.h>

// flag that's used to tell us that the thread is running on the main/entrypoint thread
#define hiw_thread_flags_main (1 << 0)

/**
 * Internal structure for a thread
 */
struct hiw_thread
{
	// thread flags
	int flags;

	// User-data
	void* data;

	// function to be called when this thread is spawned
	hiw_thread_fn func;

	// the leaf context containing the underlying value
	hiw_thread_context* context;

#if defined(HIW_WINDOWS)
	// a handle to the thread
	HANDLE handle;
#else
	pthread_t handle;
#endif
	bool started;
};

#if defined(HIW_WINDOWS)
void hiw_thread_entrypoint(void* p)
#else
void* hiw_thread_entrypoint(void* const p)
#endif
{
	hiw_thread* const t = p;
	log_debugf("hiw_thread(%p) thread entrypoint", t);
	(t->func)(t);
	log_debugf("hiw_thread(%p) thread entrypoint done", t);
	fflush(stdout);
#if !defined(HIW_WINDOWS)
	return NULL;
#else
#endif
}

hiw_thread* hiw_thread_new(hiw_thread_fn fn)
{
	assert(fn != NULL && "expected 'fn' to exist");
	if (fn == NULL)
		return NULL;
	hiw_thread* const t = hiw_malloc(sizeof(hiw_thread));
	t->flags = 0;
	t->data = NULL;
	t->func = fn;
	t->context = NULL;
#if defined(HIW_WINDOWS)
	t->handle = NULL;
#else
#endif
	t->started = false;
	return t;
}

void hiw_thread_wait(hiw_thread* t, int wait_ms)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return;
	if (hiw_bit_test(t->flags, hiw_thread_flags_main))
		return;

	log_debugf("hiw_thread(%p) stopping and wait", t);

	// TODO: Add some kind of notification for that we want to close the thread. For the library itself
	//       this happens automatically because we are closing the socket and thus causes the recv function in
	//       returning with an error code

#if defined(HIW_WINDOWS)
	if (t->handle != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(t->handle, (DWORD)wait_ms);
		// the handle is automatically closed when the thread exits
		t->handle = INVALID_HANDLE_VALUE;
	}
#else
	if (t->started)
	{
		log_debugf("hiw_thread(%p) joining", t);
		pthread_join(t->handle, NULL);
	}
#endif
	t->started = false;

	log_debugf("hiw_thread(%p) stopped", t);
}

hiw_thread* hiw_thread_main()
{
	static hiw_thread main = {.flags = hiw_thread_flags_main,
							  .data = NULL,
							  .func = NULL,
							  .context = NULL,
#if defined(HIW_WINDOWS)
							  .handle = NULL,
#else
#endif
							  .started = false};
	return &main;
}

void hiw_thread_set_func(hiw_thread* t, hiw_thread_fn fn)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return;
	if (t->handle != 0)
	{
		log_error("you are trying to set thread function while it's running");
		return;
	}
	t->func = fn;
}

void hiw_thread_set_userdata(hiw_thread* const t, void* const data)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return;
	t->data = data;
}

void* hiw_thread_get_userdata(const hiw_thread* const t)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return NULL;
	return t->data;
}

hiw_thread_context* hiw_thread_context_push(hiw_thread* const t, hiw_thread_context* const value)
{
	hiw_thread_context* const prev = t->context;
	value->thread = t;
	value->parent = t->context;
	t->context = value;
	return prev;
}

hiw_thread_context* hiw_thread_context_pop(hiw_thread* const t)
{
	hiw_thread_context* const prev = t->context;
	if (t->context != NULL)
		t->context = t->context->parent;
	return prev;
}

void hiw_thread_context_clear(hiw_thread* const t) { t->context = NULL; }

void* hiw_thread_context_find_traverse(const hiw_thread_context* const context, const void* const key)
{
	if (context->key == key)
		return context->value;
	if (context->parent != NULL)
		return hiw_thread_context_find_traverse(context->parent, key);
	return NULL;
}

void* hiw_thread_context_find(const hiw_thread* const t, const void* key)
{
	assert(key != NULL && "expected 'key' to exist");
	if (key == NULL)
		return NULL;
	if (t->context != NULL)
		return hiw_thread_context_find_traverse(t->context, key);
	return NULL;
}

bool hiw_thread_start(hiw_thread* const t)
{
	if (t == NULL)
		return false;
	if (t->started)
	{
		log_warnf("hiw_thread(%p) is already started", t);
		return true;
	}
	log_debugf("hiw_thread(%p) starting", t);
	if (hiw_bit_test(t->flags, hiw_thread_flags_main))
	{
		log_debugf("hiw_thread(%p) starting main", t);
		t->started = true;
		(t->func)(t);
		log_debugf("hiw_thread(%p) stopped main", t);
		return true;
	}
	else
	{
		t->started = true;
#if defined(HIW_WINDOWS)
		t->handle = (HANDLE)_beginthread(hiw_thread_entrypoint, 0, t);
		if (t->handle == 0)
		{
			t->started = false;
			log_errorf("could not spawn new thread. errno = %d", errno);
			return false;
		}
#else
		const int ret = pthread_create(&t->handle, NULL, hiw_thread_entrypoint, t);
		if (ret != 0)
		{
			t->started = false;
			log_errorf("could not spawn new thread. pthread_create = %d", ret);
			return false;
		}
#endif
		return true;
	}
}

void hiw_thread_delete(hiw_thread* t)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return;
	if (hiw_bit_test(t->flags, hiw_thread_flags_main))
		return;
	log_debugf("hiw_thread(%p) deleting", t);
	// TODO: Add timeout
	hiw_thread_wait(t, HIW_THREAD_WAIT_DEFAULT_TIMEOUT);
	free(t);
}

int HIW_THREAD_POOL_KEY = 0;

typedef struct hiw_thread_pool_work hiw_thread_pool_work;
typedef struct hiw_thread_pool_worker hiw_thread_pool_worker;

/**
 * @brief work to execute as quickly as possible
 */
struct hiw_thread_pool_work
{
	// function to execute
	hiw_thread_fn func;

	// user-data used when running the thread
	void* data;

	// a pointer to the next work in the linked list of work
	hiw_thread_pool_work* next;
};

/**
 * @brief Critical section
 */
struct hiw_thread_critical_sec
{
#if defined(HIW_WINDOWS)
	CRITICAL_SECTION mutex;
	CONDITION_VARIABLE cond;
#else
	pthread_mutex_t mutex;
	pthread_cond_t cond;
#endif
};

void hiw_thread_critical_sec_init(hiw_thread_critical_sec* const c)
{
	critical_section_init(&c->mutex);
	critical_cond_init(&c->cond);
}

void hiw_thread_critical_sec_release(hiw_thread_critical_sec* const c)
{
	critical_cond_destroy(&c->cond);
	critical_section_destroy(&c->mutex);
}

void hiw_thread_critical_sec_enter(hiw_thread_critical_sec* const c) { critical_section_enter(&c->mutex); }

void hiw_thread_critical_sec_exit(hiw_thread_critical_sec* const c) { critical_section_exit(&c->mutex); }

bool hiw_thread_critical_sec_wait(hiw_thread_critical_sec* const c, const int timeout)
{
#if defined(HIW_WINDOWS)
	return SleepConditionVariableCS(&c->cond, &c->mutex, timeout);
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout / 1000;
	ts.tv_nsec += (timeout % 1000) * 1000000;
	return pthread_cond_timedwait(&c->cond, &c->mutex, &ts);
#endif
}

void hiw_thread_critical_sec_notify_one(hiw_thread_critical_sec* const c) { critical_section_notify(&c->cond); }

void hiw_thread_critical_sec_notify_all(hiw_thread_critical_sec* const c) { critical_section_broadcast(&c->cond); }

/**
 * @brief worker to asynchronously execute work
 */
struct hiw_thread_pool_worker
{
	// thread pool running the worker
	hiw_thread_pool* pool;

	// thread to run
	hiw_thread* thread;

	// The next work to execute
	hiw_thread_pool_work* work_next;

	// Last work in the work linked list
	hiw_thread_pool_work* work_last;

	// A pointer to memory that we can use for future work
	hiw_thread_pool_work* work_free;

	// previous worker
	hiw_thread_pool_worker* prev;

	// next worker
	hiw_thread_pool_worker* next;

	// critical section
	hiw_thread_critical_sec critical_section;

	// boolean that tells the worker that it's running or not
	volatile bool running;
};

/**
 * @brief thread pool running work asynchronously as quickly as possible
 */
struct hiw_thread_pool
{
	// thread pool config
	hiw_thread_pool_config config;

	// The first worker in the thread pool
	hiw_thread_pool_worker* worker_first;

	// The last worker in the thread pool
	hiw_thread_pool_worker* worker_last;

	// critical section, used to lock the list of workers and the free work memory
	hiw_thread_critical_sec critical_section;
};

/**
 * @brief Add a new worker in the thread pool
 * @param pool Thread pool
 * @param worker The new Worker
 */
void hiw_thread_pool_add_worker(hiw_thread_pool* const pool, hiw_thread_pool_worker* const worker)
{
	hiw_thread_critical_sec_enter(&pool->critical_section);

	if (pool->worker_last == NULL)
	{
		pool->worker_first = worker;
		pool->worker_last = worker;
	}
	else
	{
		worker->prev = pool->worker_last;
		pool->worker_last->next = worker;
		pool->worker_last = worker;
	}

	hiw_thread_critical_sec_exit(&pool->critical_section);
}

/**
 * @brief Function to be called when work is done and should be returned
 * @param worker Thread pool worker
 * @param work work that's done
 */
void hiw_thread_pool_work_done(hiw_thread_pool_worker* const worker, hiw_thread_pool_work* const work)
{
	hiw_thread_critical_sec_enter(&worker->critical_section);
	if (worker->work_free != NULL)
		work->next = worker->work_free;
	worker->work_free = work;
	hiw_thread_critical_sec_exit(&worker->critical_section);
	// TODO: If we have a small amount of load on this pool, then allow shrinking the pool
}

/**
 * @brief Get pending work to be executed by a thread pool worker
 * @param worker The thread pool worker
 * @return A worker to be executed
 *
 * Please note that this function is UNSAFE, which means that you have to lock any appropriate
 * critical sections before calling this function
 */
hiw_thread_pool_work* hiw_thread_pool_worker_pop_work_UNSAFE(hiw_thread_pool_worker* const worker)
{
	hiw_thread_pool_work* const work = worker->work_next;

	// no work needed
	if (work == NULL)
		return NULL;

	// only one work item exists
	if (work->next == NULL)
	{
		worker->work_next = NULL;
		worker->work_last = NULL;
	}
	else
	{
		worker->work_next = work->next;
	}

	return work;
}

/**
 * @brief a function that represents doing nothing
 */
void hiw_thread_pool_do_nothing(hiw_thread* const t) { hiw_thread_pool_worker_main(t); }

/**
 * @brief function used by a thread pool worker to execute work
 * @param t the thread
 */
void hiw_thread_pool_func(hiw_thread* const t)
{
	hiw_thread_pool_worker* const worker = hiw_thread_get_userdata(t);
	log_debugf("[t:%p] thread starting up", worker);
	hiw_thread_pool* const pool = worker->pool;

	// make sure that the thread pool is available as a context value on the thread
	hiw_thread_context value = {.key = &HIW_THREAD_POOL_KEY, .value = pool};
	hiw_thread_context_push(t, &value);

	log_debugf("[t:%p] initializing worker thread", t);
	pool->config.on_start(t);

	log_debugf("[t:%p] shutting down", t);
	hiw_thread_critical_sec_exit(&worker->critical_section);
	hiw_thread_context_pop(worker->thread);
}

void hiw_thread_pool_worker_main(hiw_thread* const t)
{
	hiw_thread_pool_worker* const worker = hiw_thread_get_userdata(t);
	if (worker == NULL)
	{
		log_errorf("[t:%p] thread worker is not in it's appropriate state", t);
		return;
	}

	while (1)
	{
		hiw_thread_critical_sec_enter(&worker->critical_section);
		const bool current_running = worker->running;

		// check if we should do work
		if (worker->work_next == NULL && current_running)
		{
			// TODO allow for customized timeout
			hiw_thread_critical_sec_wait(&worker->critical_section, INFINITE);
		}

		if (!current_running)
		{
			// Stop running the worker logic when there's no more work.
			if (worker->work_next == NULL)
			{
				hiw_thread_critical_sec_exit(&worker->critical_section);
				break;
			}
		}

		// get work to executed by this worker
		hiw_thread_pool_work* const work = hiw_thread_pool_worker_pop_work_UNSAFE(worker);
		hiw_thread_critical_sec_exit(&worker->critical_section);

		// run work if exists
		if (work)
		{
			log_debugf("[t:%p] running work", t);
			hiw_thread_set_userdata(t, work->data);
			work->func(t);
			hiw_thread_pool_work_done(worker, work);
		}
	}
}

/**
 * @brief Create a new worker
 * @param pool The thread pool this worker is associated with
 * @return A new worker
 */
hiw_thread_pool_worker* hiw_thread_pool_worker_new(hiw_thread_pool* const pool)
{
	assert(pool != NULL && "expected 'pool' to exist");

	hiw_thread_pool_worker* const worker = hiw_malloc(sizeof(hiw_thread_pool_worker));
	worker->pool = pool;
	worker->thread = hiw_thread_new(hiw_thread_pool_func);
	worker->work_next = worker->work_last = NULL;
	worker->work_free = NULL;
	worker->prev = worker->next = NULL;
	hiw_thread_critical_sec_init(&worker->critical_section);
	hiw_thread_set_userdata(worker->thread, worker);

	log_debugf("hiw_thread_pool_worker(%p) created", worker);
	return worker;
}

/**
 * @brief Stop the worker from running
 * @param worker The worker
 */
void hiw_thread_pool_worker_stop(hiw_thread_pool_worker* const worker)
{
	// Mark the worker as non-running and notify is so that it'll notice the change
	// so that it will shut down
	hiw_thread_critical_sec_enter(&worker->critical_section);
	if (worker->running)
	{
		worker->running = false;
		hiw_thread_critical_sec_notify_one(&worker->critical_section);
		hiw_thread_critical_sec_exit(&worker->critical_section);
		hiw_thread_wait(worker->thread, worker->pool->config.worker_timeout);
	}
	else
	{
		hiw_thread_critical_sec_exit(&worker->critical_section);
	}
}

/**
 * @brief Cleanup the supplier workers internal memory and then free al it's memory
 * @param worker The worker to be deleted
 */
void hiw_thread_pool_worker_delete(hiw_thread_pool_worker* const worker)
{
	assert(worker != NULL && "expected 'worker' to exist");

	log_debugf("hiw_thread_pool_worker(%p) destroying", worker);
	hiw_thread_pool_worker_stop(worker);

	// cleanup cached work memory
	hiw_thread_pool_work* work = worker->work_free;
	while (work != NULL)
	{
		hiw_thread_pool_work* const next = work->next;
		free(work);
		work = next;
	}

	hiw_thread_critical_sec_release(&worker->critical_section);
	hiw_thread_delete(worker->thread);
	log_debugf("hiw_thread_pool_worker(%p) destroyed", worker);
	free(worker);
}

hiw_thread_pool* hiw_thread_pool_new(const hiw_thread_pool_config* config)
{
	// TODO add support for shrinking thread count
	assert(config->allow_shrink == false && "shrinking the threads are not supported yet");
	// TODO add support for increasing thread count
	assert(config->max_count == config->count && "increasing thread count dynamically are not supported yet");

	hiw_thread_pool* const impl = hiw_malloc(sizeof(hiw_thread_pool));
	impl->config = *config;
	if (impl->config.on_start == NULL)
		impl->config.on_start = hiw_thread_pool_do_nothing;
	impl->worker_first = NULL;
	impl->worker_last = NULL;

	hiw_thread_critical_sec_init(&impl->critical_section);

	for (int i = 0; i < impl->config.count; ++i)
	{
		hiw_thread_pool_worker* const worker = hiw_thread_pool_worker_new(impl);
		hiw_thread_pool_add_worker(impl, worker);
	}

	return impl;
}

void hiw_thread_pool_delete(hiw_thread_pool* const pool)
{
	log_debugf("hiw_thread_pool(%p) shutting down thread pool", pool);

	// Iterate through all workers and wait for them to shut down
	hiw_thread_pool_worker* worker = pool->worker_first;
	while (worker != NULL)
	{
		hiw_thread_pool_worker* const next = worker->next;
		hiw_thread_pool_worker_stop(worker);
		hiw_thread_pool_worker_delete(worker);
		worker = next;
	}

	hiw_thread_critical_sec_release(&pool->critical_section);
	free(pool);
}

bool hiw_thread_pool_start(hiw_thread_pool* const pool)
{
	log_infof("hiw_thread_pool(%p) starting", pool);

	hiw_thread_pool_worker* worker = pool->worker_first;
	while (worker != NULL)
	{
		worker->running = true;
		if (!hiw_thread_start(worker->thread))
			return false;
		worker = worker->next;
	}
	log_infof("hiw_thread_pool(%p) started", pool);
	return true;
}

/**
 * @param worker The worker
 * @return Create new work memory or reuse cached work memory
 *
 * Please note that this function is UNSAFE, which means that you have to lock any appropriate
 * critical sections before calling this function
 */
hiw_thread_pool_work* hiw_thread_pool_work_new_UNSAFE(hiw_thread_pool_worker* const worker, hiw_thread_fn func,
													  void* data)
{
	// try to get free work memory
	hiw_thread_pool_work* work = worker->work_free;
	if (work == NULL)
		work = hiw_malloc(sizeof(hiw_thread_pool_work));
	else
		worker->work_next = work->next;
	work->func = func;
	work->data = data;
	work->next = NULL;
	return work;
}

void hiw_thread_pool_push(hiw_thread_pool* const pool, hiw_thread_fn func, void* data)
{
	assert(pool != NULL && "expected 'pool' to exist");
	assert(pool->worker_first != NULL && "expected 'worker_first' to exist");

	// Get the first worker and move it to the end
	hiw_thread_critical_sec_enter(&pool->critical_section);
	hiw_thread_pool_worker* const worker = pool->worker_first;
	pool->worker_first = worker->next;
	pool->worker_first->prev = NULL;
	pool->worker_last->next = worker;
	worker->prev = pool->worker_last;
	pool->worker_last = worker;
	hiw_thread_critical_sec_exit(&pool->critical_section);

	// Get a work from the worker and add it to be worked
	hiw_thread_critical_sec_enter(&worker->critical_section);
	hiw_thread_pool_work* const work = hiw_thread_pool_work_new_UNSAFE(worker, func, data);
	if (worker->work_last == NULL)
	{
		worker->work_next = worker->work_last = work;
	}
	else
	{
		worker->work_last->next = work;
	}
	hiw_thread_critical_sec_notify_one(&worker->critical_section);
	hiw_thread_critical_sec_exit(&worker->critical_section);
}

hiw_thread_pool* hiw_thread_pool_get(const hiw_thread* const t)
{
	return hiw_thread_context_find(t, &HIW_THREAD_POOL_KEY);
}

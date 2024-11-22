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
		t->started = false;
	}
#endif

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
	assert(t != NULL && "expected 't' to exist");
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
#if defined(HIW_WINDOWS)
		t->started = true;
		t->handle = (HANDLE)_beginthread(hiw_thread_entrypoint, 0, t);
		if (t->handle == 0)
		{
			t->started = false;
			log_errorf("could not spawn new thread. errno = %d", errno);
			return false;
		}
#else
		t->started = true;
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
 * @brief worker to asynchronously execute work
 */
struct hiw_thread_pool_worker
{
	// thread pool running the worker
	hiw_thread_pool* pool;

	// thread to run
	hiw_thread* thread;

	// previous worker
	hiw_thread_pool_worker* prev;

	// next worker
	hiw_thread_pool_worker* next;
};

/**
 * @brief thread pool running work asynchronously as quickly as possible
 */
struct hiw_thread_pool
{
	// thread pool config
	hiw_thread_pool_config config;

	// thread pool is running
	volatile atomic_bool running;

	// The first worker in the thread pool
	hiw_thread_pool_worker* worker_first;

	// The last worker in the thread pool
	hiw_thread_pool_worker* worker_last;

	// The next work to execute
	hiw_thread_pool_work* work_next;

	// Last work in the work linked list
	hiw_thread_pool_work* work_last;

	// A pointer to memory that we can use for future work
	hiw_thread_pool_work* work_free;

#if defined(HIW_WINDOWS)
	CRITICAL_SECTION mutex;
	CONDITION_VARIABLE work_cond;
#else
	pthread_mutex_t mutex;
	pthread_cond_t work_cond;
#endif
};

/**
 * @brief add a new worker in the thread pool
 * @param pool thread pool
 * @param worker worker
 */
void hiw_thread_pool_add_worker(hiw_thread_pool* const pool, hiw_thread_pool_worker* const worker)
{
	critical_section_enter(&pool->mutex);

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

	critical_section_exit(&pool->mutex);
}

/**
 * @brief function to be called when work is done and should be returned
 * @param pool thread pool
 * @param work work that's done
 */
void hiw_thread_pool_work_done(hiw_thread_pool* const pool, hiw_thread_pool_work* const work)
{
	critical_section_enter(&pool->mutex);
	if (pool->work_free != NULL)
		work->next = pool->work_free;
	pool->work_free = work;
	critical_section_exit(&pool->mutex);

	if (pool->config.allow_shrink)
	{
		// TODO: If we have a small amount of load on this pool, then allow shrinking the pool
	}
}

/**
 * @brief get pending work to be executed by a thread pool worker
 * @param pool the thread pool
 * @return a worker to be executed
 */
hiw_thread_pool_work* hiw_thread_pool_pop_work(hiw_thread_pool* const pool)
{
	hiw_thread_pool_work* work = pool->work_next;

	// no work needed
	if (work == NULL)
		return NULL;

	// only one work item exists
	if (work->next == NULL)
	{
		pool->work_next = NULL;
		pool->work_last = NULL;
	}
	else
	{
		pool->work_next = work->next;
	}

	return work;
}

/**
 * @brief a function that represents doing nothing
 */
void hiw_thread_pool_do_nothing(hiw_thread* const t) {}

/**
 * @brief function used by a thread pool worker to execute work
 * @param t the thread
 */
void hiw_thread_pool_func(hiw_thread* const t)
{
	const hiw_thread_pool_worker* const worker = hiw_thread_get_userdata(t);
	hiw_thread_pool* const pool = worker->pool;

	// make sure that the thread pool is available as a context value on the thread
	hiw_thread_context value = {.key = &HIW_THREAD_POOL_KEY, .value = pool};
	hiw_thread_context_push(t, &value);

	// initialize this worker
	pool->config.init(t);

	while (1)
	{
		critical_section_enter(&pool->mutex);

		// check if we should do work
		if (pool->work_next == NULL && pool->running)
		{
			// TODO allow for customized timeout
			critical_section_wait(&pool->work_cond, &pool->mutex, INFINITE);
		}

		if (!pool->running)
			break;

		// get work if exists
		hiw_thread_pool_work* const work = hiw_thread_pool_pop_work(pool);
		critical_section_exit(&pool->mutex);

		// run work if exists
		if (work)
		{
			log_debugf("[t:%p] running work", t);
			hiw_thread_set_userdata(t, work->data);
			work->func(t);
			hiw_thread_pool_work_done(pool, work);
		}
	}

	log_debugf("[t:%p] shutting down", t);
	critical_section_exit(&pool->mutex);

	// release this worker
	pool->config.release(t);

	hiw_thread_context_pop(worker->thread);
}

hiw_thread_pool* hiw_thread_pool_new(const hiw_thread_pool_config* config)
{
	// TODO add support for shrinking thread count
	assert(config->allow_shrink == false && "shrinking the threads are not supported yet");
	// TODO add support for increasing thread count
	assert(config->max_count == config->count && "increasing thread count dynamically are not supported yet");

	hiw_thread_pool* const impl = hiw_malloc(sizeof(hiw_thread_pool));
	impl->config = *config;
	if (impl->config.init == NULL)
		impl->config.init = hiw_thread_pool_do_nothing;
	if (impl->config.release == NULL)
		impl->config.release = hiw_thread_pool_do_nothing;
	impl->worker_first = NULL;
	impl->worker_last = NULL;
	impl->work_next = NULL;
	impl->work_last = NULL;
	impl->work_free = NULL;
	impl->running = false;

	critical_section_init(&impl->mutex);
	critical_cond_init(&impl->work_cond);

	for (int i = 0; i < impl->config.count; ++i)
	{
		hiw_thread_pool_worker* worker = hiw_malloc(sizeof(hiw_thread_pool_worker));
		worker->pool = impl;
		worker->thread = hiw_thread_new(hiw_thread_pool_func);
		worker->next = NULL;
		worker->prev = NULL;
		hiw_thread_set_userdata(worker->thread, worker);
		hiw_thread_pool_add_worker(impl, worker);
	}

	return impl;
}

void hiw_thread_pool_delete(hiw_thread_pool* const pool)
{
	// if running then shut all worker threads down
	if (pool->running)
	{
		log_debugf("hiw_thread_pool(%p) shutting down thread pool", pool);

		// tell all workers that we are no longer running
		critical_section_enter(&pool->mutex);
		pool->running = false;
		critical_section_broadcast(&pool->work_cond);
		critical_section_exit(&pool->mutex);

		// Iterate through all workers and wait for them to shut down
		hiw_thread_pool_worker* worker = pool->worker_first;
		while (worker != NULL)
		{
			// TODO Add configurable timeout. Should match request timeout or shorter
			log_debugf("hiw_thread_pool(%p) waiting for hiw_thread(%p)", pool, worker->thread);
			hiw_thread_wait(worker->thread, HIW_THREAD_WAIT_DEFAULT_TIMEOUT);
			worker = worker->next;
		}
	}

	// cleanup
	hiw_thread_pool_worker* worker = pool->worker_first;
	while (worker != NULL)
	{
		hiw_thread_pool_worker* next = worker->next;
		hiw_thread_delete(worker->thread);
		free(worker);
		worker = next;
	}

	critical_cond_destroy(&pool->work_cond);
	critical_section_destroy(&pool->mutex);
	free(pool);
}

void hiw_thread_pool_start(hiw_thread_pool* const pool)
{
	log_infof("hiw_thread_pool(%p) starting", pool);
	pool->running = true;

	const hiw_thread_pool_worker* worker = pool->worker_first;
	while (worker != NULL)
	{
		hiw_thread_start(worker->thread);
		worker = worker->next;
	}
	log_infof("hiw_thread_pool(%p) started", pool);
}

void hiw_thread_pool_push(hiw_thread_pool* const poo, hiw_thread_fn func, void* data)
{
	critical_section_enter(&poo->mutex);

	// get work memory
	hiw_thread_pool_work* work = poo->work_free;
	if (work == NULL)
		work = hiw_malloc(sizeof(hiw_thread_pool_work));
	else
		poo->work_next = work->next;
	work->func = func;
	work->data = data;
	work->next = NULL;

	// add work to the linked list

	if (poo->work_last == NULL)
	{
		poo->work_next = poo->work_last = work;
	}
	else
	{
		poo->work_last->next = work;
	}

	critical_section_notify(&poo->work_cond);
	critical_section_exit(&poo->mutex);
}

hiw_thread_pool* hiw_thread_pool_get(hiw_thread* const t) { return hiw_thread_context_find(t, &HIW_THREAD_POOL_KEY); }

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
#define critical_cond_init(cond) pthread_cond_init(mutex, NULL)
#define critical_section_destroy(mutex) pthread_mutex_destroy(mutex)
#define critical_cond_destroy(cond) pthread_cond_destroy(cond)
#define critical_section_enter(mutex) pthread_mutex_lock(mutex)
#define critical_section_exit(mutex) pthread_mutex_unlock(mutex)

bool critical_section_wait(const pthread_cond_t cond, const pthread_mutex_t mutex, const int millis)
{
	struct timeval tv;
	clock_gettime(CLOCK_REALTIME, &ts);
	tv.tv_sec += read_timeout / 1000;
	tv.tv_usec += (read_timeout % 1000) * 1000;
	return pthread_cond_timedwait(cond, mutex, &tv);
}

#define critical_section_wait(cond, mutex) pthread_cond_timedwait(cond, mutex, (&))
#define critical_section_notify(cond) WakeConditionVariable(cond)
#define critical_section_broadcast(cond) pthread_cond_broadcast(cond)
#endif

#include <assert.h>

/**
 * Internal structure for a thread
 */
struct hiw_internal_thread
{
	// the public implementation
	hiw_thread pub;

#if defined(HIW_WINDOWS)
	// a handle to the thread
	HANDLE handle;
#else
	pthread_t handle;
#endif
	bool started;
};

typedef struct hiw_internal_thread hiw_internal_thread;

#if defined(HIW_WINDOWS)
void hiw_thread_entrypoint(void* p)
#else
void* hiw_thread_entrypoint(void* p)
#endif
{
	hiw_internal_thread* const t = (hiw_internal_thread*)p;
	log_debugf("hiw_thread(%p) thread entrypoint", t);
	(t->pub.func)(&t->pub);
	log_debugf("hiw_thread(%p) thread entrypoint done", t);
	fflush(stdout);
#if !defined(HIW_WINDOWS)
	return NULL;
#endif
}

hiw_thread* hiw_thread_new(hiw_thread_fn fn)
{
	assert(fn != NULL && "expected 'fn' to exist");
	if (fn == NULL)
		return NULL;
	hiw_internal_thread* const t = hiw_malloc(sizeof(hiw_internal_thread));
	t->pub.flags = 0;
	t->pub.data = NULL;
	t->pub.func = fn;
	t->pub.context = NULL;
#if defined(HIW_WINDOWS)
	t->handle = NULL;
#else
#endif
	t->started = false;
	return &t->pub;
}

void hiw_thread_wait(hiw_thread* t, int wait_ms)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return;
	if (hiw_bit_test(t->flags, hiw_thread_flags_main))
		return;

	log_debugf("hiw_thread(%p) stopping and wait", t);

	hiw_internal_thread* const impl = (struct hiw_internal_thread*)t;

	// TODO: Add some kind of notification for that we want to close the thread. For the library itself
	//       this happens automatically because we are closing the socket and thus causes the recv function in
	//       returning with an error code

#if defined(HIW_WINDOWS)
	if (impl->handle != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(impl->handle, (DWORD)wait_ms);
		// the handle is automatically closed when the thread exits
		impl->handle = INVALID_HANDLE_VALUE;
	}
#else
	if (impl->started)
	{
		log_debugf("hiw_thread(%p) joining", t);
		pthread_join(impl->handle, NULL);
		impl->started = false;
	}
#endif

	log_debugf("hiw_thread(%p) stopped", t);
}

hiw_thread* hiw_thread_main()
{
	static hiw_internal_thread main = {.pub.flags = hiw_thread_flags_main,
									   .pub.func = NULL,
									   .pub.context = NULL,
#if defined(HIW_WINDOWS)
									   .handle = NULL,
#else
#endif
									   .started = false};
	return &main.pub;
}

void hiw_thread_set_func(hiw_thread* t, hiw_thread_fn fn)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return;
	hiw_internal_thread* const thread = (struct hiw_internal_thread*)t;
	if (thread->handle != 0)
	{
		log_error("you are trying to set thread function while it's running");
		return;
	}
	thread->pub.func = fn;
}

void hiw_thread_set_userdata(hiw_thread* t, void* data)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return;
	t->data = data;
}

void* hiw_thread_get_userdata(hiw_thread* t)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return NULL;
	return t->data;
}

hiw_thread_context* hiw_thread_context_push(hiw_thread* thread, hiw_thread_context* value)
{
	hiw_thread_context* const prev = thread->context;
	value->thread = thread;
	value->parent = thread->context;
	thread->context = value;
	return prev;
}

hiw_thread_context* hiw_thread_context_pop(hiw_thread* thread)
{
	hiw_thread_context* const prev = thread->context;
	if (thread->context != NULL)
		thread->context = thread->context->parent;
	return prev;
}

void* hiw_thread_context_find_traverse(const hiw_thread_context* const context, const void* const key)
{
	if (context->key == key)
		return context->value;
	if (context->parent != NULL)
		return hiw_thread_context_find_traverse(context->parent, key);
	return NULL;
}

void* hiw_thread_context_find(hiw_thread* const thread, const void* key)
{
	assert(key != NULL && "expected 'key' to exist");
	if (key == NULL)
		return NULL;
	if (thread->context != NULL)
		return hiw_thread_context_find_traverse(thread->context, key);
	return NULL;
}

bool hiw_thread_start(hiw_thread* const t)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL)
		return false;
	hiw_internal_thread* const thread = (struct hiw_internal_thread*)t;
	if (thread->started)
	{
		log_warnf("hiw_thread(%p) is already started", t);
		return true;
	}
	log_debugf("hiw_thread(%p) starting", t);
	if (hiw_bit_test(t->flags, hiw_thread_flags_main))
	{
		log_debugf("hiw_thread(%p) starting main", t);
		thread->started = true;
		(t->func)(t);
		log_debugf("hiw_thread(%p) stopped main", t);
		return true;
	}
	else
	{
#if defined(HIW_WINDOWS)
		thread->started = true;
		thread->handle = (HANDLE)_beginthread(hiw_thread_entrypoint, 0, t);
		if (thread->handle == 0)
		{
			thread->started = false;
			log_errorf("could not spawn new thread. errno = %d", errno);
			return false;
		}
#else
		thread->started = true;
		const int ret = pthread_create(&thread->handle, NULL, hiw_thread_entrypoint, t);
		if (ret != 0)
		{
			thread->started = false;
			log_errorf("could not spawn new thread. pthread_create = %d", ret);
			return false;
		}
		pthread_detach(thread->handle);
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
	hiw_internal_thread* const thread = (struct hiw_internal_thread*)t;
	log_debugf("hiw_thread(%p) deleting", t);
	hiw_thread_wait(t, HIW_THREAD_WAIT_DEFAULT_TIMEOUT);
	free(thread);
}

int HIW_THREAD_POOL_KEY = 0;

typedef struct hiw_internal_thread_work hiw_internal_thread_work;
typedef struct hiw_internal_thread_worker hiw_internal_thread_worker;
typedef struct hiw_internal_thread_pool hiw_internal_thread_pool;

struct hiw_internal_thread_work
{
	// Function to execute
	hiw_thread_fn func;

	// Data to run
	void* data;

	// Next thread
	hiw_internal_thread_work* next;
};

struct hiw_internal_thread_worker
{
	// thread pool running the worker
	hiw_internal_thread_pool* pool;

	// thread to run
	hiw_thread* thread;

	// previous worker
	hiw_internal_thread_worker* prev;

	// next worker
	hiw_internal_thread_worker* next;
};

struct hiw_internal_thread_pool
{
	// public interface
	hiw_thread_pool pub;

	// thread pool is running
	volatile atomic_bool running;

	// The first worker in the thread pool
	hiw_internal_thread_worker* worker_first;

	// The last worker in the thread pool
	hiw_internal_thread_worker* worker_last;

	// The next work to execute
	hiw_internal_thread_work* work_next;

	// Last work in the work linked list
	hiw_internal_thread_work* work_last;

	// A pointer to memory that we can use for future work
	hiw_internal_thread_work* work_free;

#if defined(HIW_WINDOWS)
	CRITICAL_SECTION mutex;
	CONDITION_VARIABLE work_cond;
#else
	pthread_mutex_t mutex;
#endif
};

void hiw_thread_pool_add_worker(hiw_internal_thread_pool* impl, hiw_internal_thread_worker* worker)
{
	critical_section_enter(&impl->mutex);

	if (impl->worker_last == NULL)
	{
		impl->worker_first = worker;
		impl->worker_last = worker;
	}
	else
	{
		worker->prev = impl->worker_last;
		impl->worker_last->next = worker;
		impl->worker_last = worker;
	}

	critical_section_exit(&impl->mutex);
}

hiw_internal_thread_work* hiw_thread_pool_pop_work(hiw_internal_thread_pool* impl)
{
	hiw_internal_thread_work* work = impl->work_next;

	// no work needed
	if (work == NULL)
		return NULL;

	// only one work item exists
	if (work->next == NULL)
	{
		impl->work_next = NULL;
		impl->work_last = NULL;
	}
	else
	{
		impl->work_next = work->next;
	}

	return work;
}

void hiw_thread_pool_func(hiw_thread* t)
{
	const hiw_internal_thread_worker* const worker = hiw_thread_get_userdata(t);
	hiw_internal_thread_pool* const pool = worker->pool;

	// make sure that the thread pool is available as a context value on the thread
	hiw_thread_context value = {.key = &HIW_THREAD_POOL_KEY, .value = pool};
	hiw_thread_context_push(t, &value);

	while (1)
	{
		critical_section_enter(&pool->mutex);

		// check if we should do work
		if (pool->work_next == NULL && pool->running)
		{
			critical_section_wait(&pool->work_cond, &pool->mutex, INFINITE);
		}

		if (!pool->running)
			break;

		// get work if exists
		hiw_internal_thread_work* const work = hiw_thread_pool_pop_work(pool);
		critical_section_exit(&pool->mutex);

		// run work if exists
		if (work)
		{
			log_debugf("[t:%p] running work", t);
			work->func(work->data);
		}
	}

	log_debugf("[t:%p] shutting down", t);
	critical_section_exit(&pool->mutex);
	hiw_thread_context_pop(worker->thread);
}

hiw_thread_pool* hiw_thread_pool_new(const int initial_count, const int max_count)
{
	hiw_internal_thread_pool* const impl = hiw_malloc(sizeof(hiw_internal_thread_pool));
	impl->pub.count = initial_count;
	impl->pub.max_count = max_count;
	impl->worker_first = NULL;
	impl->worker_last = NULL;
	impl->work_next = NULL;
	impl->work_last = NULL;
	impl->work_free = NULL;
	impl->running = false;

	critical_section_init(&impl->mutex);
	critical_cond_init(&impl->work_cond);

	for (int i = 0; i < impl->pub.count; ++i)
	{
		hiw_internal_thread_worker* worker = hiw_malloc(sizeof(hiw_internal_thread_worker));
		worker->pool = impl;
		worker->thread = hiw_thread_new(hiw_thread_pool_func);
		worker->next = NULL;
		worker->prev = NULL;
		hiw_thread_set_userdata(worker->thread, worker);
		hiw_thread_pool_add_worker(impl, worker);
	}

	return &impl->pub;
}

void hiw_thread_pool_delete(hiw_thread_pool* pool)
{
	hiw_internal_thread_pool* const impl = (hiw_internal_thread_pool*)pool;

	// if running then shut all worker threads down
	if (impl->running)
	{
		log_debugf("hiw_thread_pool(%p) shutting down thread pool", impl);

		// tell all workers that we are no longer running
		critical_section_enter(&impl->mutex);
		impl->running = false;
		critical_section_broadcast(&impl->work_cond);
		critical_section_exit(&impl->mutex);

		// Iterate through all workers and wait for them to shut down
		hiw_internal_thread_worker* worker = impl->worker_first;
		while (worker != NULL)
		{
			// TODO Add configurable timeout. Should match request timeout or shorter
			log_debugf("hiw_thread_pool(%p) waiting for hiw_thread(%p)", impl, worker->thread);
			hiw_thread_wait(worker->thread, HIW_THREAD_WAIT_DEFAULT_TIMEOUT);
			worker = worker->next;
		}
	}

	// cleanup
	hiw_internal_thread_worker* worker = impl->worker_first;
	while (worker != NULL)
	{
		hiw_internal_thread_worker* next = worker->next;
		hiw_thread_delete(worker->thread);
		free(worker);
		worker = next;
	}

	critical_cond_destroy(impl->work_cond);
	critical_section_destroy(&impl->mutex);
	free(impl);
}

void hiw_thread_pool_start(hiw_thread_pool* pool)
{
	hiw_internal_thread_pool* const impl = (hiw_internal_thread_pool*)pool;
	impl->running = true;

	const hiw_internal_thread_worker* worker = impl->worker_first;
	while (worker != NULL)
	{
		hiw_thread_start(worker->thread);
		worker = worker->next;
	}
}

void hiw_thread_pool_push(hiw_thread_pool* pool, hiw_thread_fn func, void* data)
{
	hiw_internal_thread_pool* const impl = (hiw_internal_thread_pool*)pool;

	critical_section_enter(&impl->mutex);

	// get work memory
	hiw_internal_thread_work* work = impl->work_free;
	if (work == NULL)
	{
		work = hiw_malloc(sizeof(hiw_internal_thread_work));
		work->func = func;
		work->data = data;
	}
	else
	{
		impl->work_next = work->next;
	}
	work->next = NULL;

	// add work to the linked list

	if (impl->work_last == NULL)
	{
		impl->work_next = impl->work_last = work;
	}
	else
	{
		impl->work_last->next = work;
	}

	critical_section_notify(&impl->work_cond);
	critical_section_exit(&impl->mutex);
}

hiw_thread_pool* hiw_thread_pool_get(hiw_thread* t) { return hiw_thread_context_find(t, &HIW_THREAD_POOL_KEY); }

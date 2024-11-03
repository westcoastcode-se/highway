//
// Part of the highway project, under the MIT License
// See the LICENSE file in the project root for license terms
//

#include "hiw_thread.h"
#include "hiw_logger.h"
#include "hiw_std.h"

#if defined(HIW_WINDOWS)
#	include <process.h>
#   include <windows.h>
#elif defined(HIW_LINUX)
#	include <pthread.h>
#else
#	error OSX not implemented
#endif

#ifdef __unix__
#	include <pthread.h>
#	define HIW_THREAD_HANDLE pthread_t
#elif defined(HIW_WINDOWS)
#	include <process.h>
#   include <windows.h>
#	define HIW_THREAD_HANDLE HANDLE
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
	bool thread_initialized;
#endif
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
	if (fn == NULL) return NULL;
	hiw_internal_thread* const t = (hiw_internal_thread*)malloc(sizeof(hiw_internal_thread));
	if (t == NULL)
	{
		log_error("out of memory");
		return NULL;
	}
	t->pub.id = (size_t)(t);
	t->pub.flags = 0;
	t->pub.data = NULL;
	t->pub.func = fn;
	t->pub.context = NULL;
#if defined(HIW_WINDOWS)
	t->handle = NULL;
#else
	t->thread_initialized = false;
#endif
	return &t->pub;
}

void hiw_thread_stop_and_wait(hiw_thread* t, int wait_ms)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL) return;
	if (hiw_bit_test(t->flags, hiw_thread_flags_main)) return;

	log_debugf("hiw_thread(%p) stopping and wait", t);

	struct hiw_internal_thread* const impl = (struct hiw_internal_thread*)t;

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
	if (impl->thread_initialized)
	{
		log_debugf("hiw_thread(%p) joining", t);
		pthread_join(impl->handle, NULL);
		impl->thread_initialized = false;
	}
#endif

	log_debugf("hiw_thread(%p) stopped", t);
}

hiw_thread* hiw_thread_main()
{
	static hiw_internal_thread main = {
			.pub.flags = hiw_thread_flags_main,
			.pub.id = 0,
			.pub.func = NULL,
			.pub.context = NULL,
#if defined(HIW_WINDOWS)
			.handle = NULL,
#else
			.thread_initialized = false,
#endif
	};
	return &main.pub;
}

void hiw_thread_set_func(hiw_thread* t, hiw_thread_fn fn)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL) return;
	struct hiw_internal_thread* const thread = (struct hiw_internal_thread*)t;
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
	if (t == NULL) return;
	t->data = data;
}

void* hiw_thread_get_userdata(hiw_thread* t)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL) return NULL;
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
	if (thread->context != NULL) thread->context = thread->context->parent;
	return prev;
}

void* hiw_thread_context_find_traverse(hiw_thread_context* context, const void* key)
{
	if (context->key == key) return context->value;
	if (context->parent != NULL) return hiw_thread_context_find_traverse(context->parent, key);
	return NULL;
}

void* hiw_thread_context_find(hiw_thread* thread, const void* key)
{
	assert(key != NULL && "expected 'key' to exist");
	if (key == NULL) return NULL;
	if (thread->context != NULL) return hiw_thread_context_find_traverse(thread->context, key);
	return NULL;
}

bool hiw_thread_start(hiw_thread* t)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL) return false;
	struct hiw_internal_thread* const thread = (struct hiw_internal_thread*)t;
	if (thread->handle != 0)
	{
		log_warnf("thread %lld has already started", thread->pub.id);
		return true;
	}
	log_debugf("hiw_thread(%p) starting", t);
	if (hiw_bit_test(t->flags, hiw_thread_flags_main))
	{
		log_debugf("hiw_thread(%p) starting main", t);
		(t->func)(t);
		log_debugf("hiw_thread(%p) stopped main", t);
		return true;
	}
	else
	{

#if defined(HIW_WINDOWS)
		thread->handle = (HANDLE)_beginthread(hiw_thread_entrypoint, 0, t);
		if (thread->handle == 0)
		{
			log_error("could not spawn new thread");
			return false;
		}
#else
		const int ret = pthread_create(&thread->handle, NULL, hiw_thread_entrypoint, t);
		if (ret != 0)
		{
			log_errorf("could not spawn new thread. pthread_create = %d", ret);
			return false;
		}
		thread->thread_initialized = true;
		pthread_detach(thread->handle);
#endif
		return true;
	}
}

void hiw_thread_delete(hiw_thread* t)
{
	assert(t != NULL && "expected 't' to exist");
	if (t == NULL) return;
	if (hiw_bit_test(t->flags, hiw_thread_flags_main)) return;
	struct hiw_internal_thread* const thread = (struct hiw_internal_thread*)t;
	log_debugf("hiw_thread(%p) deleting", t);
	hiw_thread_stop_and_wait(t, 30000);
	free(thread);
}
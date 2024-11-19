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

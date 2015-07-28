/*
 * libusb synchronization on Microsoft Windows
 *
 * Copyright © 2010 Michael Plante <michael.plante@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>
#include <objbase.h>
#include <errno.h>
#include <stdarg.h>

#include "libusbi.h"

extern const uint64_t epoch_time;

int usbi_mutex_init(usbi_mutex_t *mutex,
					const usbi_mutexattr_t *attr) {
	UNUSED(attr);
	if(! mutex) return ((errno=EINVAL));
	*mutex = CreateMutex(NULL, FALSE, NULL);
	if(!*mutex) return ((errno=ENOMEM));
	return 0;
}
int usbi_mutex_destroy(usbi_mutex_t *mutex) {
	// It is not clear if CloseHandle failure is due to failure to unlock.
	//   If so, this should be errno=EBUSY.
	if(!mutex || !CloseHandle(*mutex)) return ((errno=EINVAL));
	*mutex = NULL;
	return 0;
}
int usbi_mutex_trylock(usbi_mutex_t *mutex) {
	DWORD result;
	if(!mutex) return ((errno=EINVAL));
	result = WaitForSingleObject(*mutex, 0);
	if(result == WAIT_OBJECT_0 || result == WAIT_ABANDONED)
		return 0; // acquired (ToDo: check that abandoned is ok)
	if(result == WAIT_TIMEOUT)
		return ((errno=EBUSY));
	return ((errno=EINVAL)); // don't know how this would happen
							 //   so don't know proper errno
}
int usbi_mutex_lock(usbi_mutex_t *mutex) {
	DWORD result;
	if(!mutex) return ((errno=EINVAL));
	result = WaitForSingleObject(*mutex, INFINITE);
	if(result == WAIT_OBJECT_0 || result == WAIT_ABANDONED)
		return 0; // acquired (ToDo: check that abandoned is ok)
	return ((errno=EINVAL)); // don't know how this would happen
							 //   so don't know proper errno
}
int usbi_mutex_unlock(usbi_mutex_t *mutex) {
	if(!mutex)                return ((errno=EINVAL));
	if(!ReleaseMutex(*mutex)) return ((errno=EPERM ));
	return 0;
}

int usbi_mutex_static_lock(usbi_mutex_static_t *mutex) {
	if(!mutex)               return ((errno=EINVAL));
	while (InterlockedExchange((LONG *)mutex, 1) == 1) {
		SleepEx(0, TRUE);
	}
	return 0;
}
int usbi_mutex_static_unlock(usbi_mutex_static_t *mutex) {
	if(!mutex)               return ((errno=EINVAL));
	*mutex = 0;
	return 0;
}

int usbi_cond_init(usbi_cond_t *cond,
				   const usbi_condattr_t *attr) {
	UNUSED(attr);
	if(!cond)           return ((errno=EINVAL));
	list_init(&cond->waiters    );
	list_init(&cond->not_waiting);
	return 0;
}
int usbi_cond_destroy(usbi_cond_t *cond) {
	// This assumes no one is using this anymore.  The check MAY NOT BE safe.
	struct usbi_cond_perthread *pos, *next_pos = NULL;
	if(!cond) return ((errno=EINVAL));
	if(!list_empty(&cond->waiters)) return ((errno=EBUSY )); // (!see above!)
	list_for_each_entry_safe(pos, next_pos, &cond->not_waiting, list, struct usbi_cond_perthread) {
		CloseHandle(pos->event);
		list_del(&pos->list);
		free(pos);
	}

	return 0;
}

int usbi_cond_broadcast(usbi_cond_t *cond) {
	// Assumes mutex is locked; this is not in keeping with POSIX spec, but
	//   libusb does this anyway, so we simplify by not adding more sync
	//   primitives to the CV definition!
	int fail = 0;
	struct usbi_cond_perthread *pos;
	if(!cond)                      return ((errno=EINVAL));
	list_for_each_entry(pos, &cond->waiters, list, struct usbi_cond_perthread) {
		if(!SetEvent(pos->event))
			fail = 1;
	}
	// The wait function will remove its respective item from the list.
	return fail ? ((errno=EINVAL)) : 0;
}
int usbi_cond_signal(usbi_cond_t *cond) {
	// Assumes mutex is locked; this is not in keeping with POSIX spec, but
	//   libusb does this anyway, so we simplify by not adding more sync
	//   primitives to the CV definition!
	struct usbi_cond_perthread *pos;
	if(!cond)                      return ((errno=EINVAL));
	if(list_empty(&cond->waiters)) return 0; // no one to wakeup.
	pos = list_entry(&cond->waiters.next, struct usbi_cond_perthread, list);
	// The wait function will remove its respective item from the list.
	return SetEvent(pos->event) ? 0 : ((errno=EINVAL));
}
__inline static int usbi_cond_intwait(usbi_cond_t *cond,
									  usbi_mutex_t *mutex,
									  DWORD timeout_ms) {
	struct usbi_cond_perthread *pos;
	int found = 0, r;
	DWORD r2,tid = GetCurrentThreadId();
	if(!cond || !mutex) return ((errno=EINVAL));
	list_for_each_entry(pos, &cond->not_waiting, list, struct usbi_cond_perthread) {
		if(tid == pos->tid) {
			found = 1;
			break;
		}
	}
	if(!found) {
		pos      = (struct usbi_cond_perthread*) calloc(1, sizeof(struct usbi_cond_perthread));
		if(!pos) return ((errno=ENOMEM)); // This errno is not POSIX-allowed.
		pos->tid = tid;
		pos->event = CreateEvent(NULL, FALSE, FALSE, NULL); // auto-reset.
		if(!pos->event) {
			free(pos);
			return      ((errno=ENOMEM));
		}
		list_add(&pos->list, &cond->not_waiting);
	}

	list_del(&pos->list); // remove from not_waiting list.
	list_add(&pos->list, &cond->waiters);

	r  = usbi_mutex_unlock(mutex);
	if(r) return r;
	r2 = WaitForSingleObject(pos->event, timeout_ms);
	r  = usbi_mutex_lock(mutex);
	if(r) return r;

	list_del(&pos->list);
	list_add(&pos->list, &cond->not_waiting);

	if(r2 == WAIT_TIMEOUT) return ((errno=ETIMEDOUT));

	return 0;
}
// N.B.: usbi_cond_*wait() can also return ENOMEM, even though pthread_cond_*wait cannot!
int usbi_cond_wait(usbi_cond_t *cond, usbi_mutex_t *mutex) {
	return usbi_cond_intwait(cond, mutex, INFINITE);
}
int usbi_cond_timedwait(usbi_cond_t *cond,
						usbi_mutex_t *mutex,
						const struct timespec *abstime) {
	FILETIME filetime;
	ULARGE_INTEGER rtime;
	struct timeval targ_time, cur_time, delta_time;
	struct timespec cur_time_ns;
	DWORD millis;

	// GetSystemTimeAsFileTime() is not available on CE
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &filetime);
	rtime.LowPart   = filetime.dwLowDateTime;
	rtime.HighPart  = filetime.dwHighDateTime;
	rtime.QuadPart -= epoch_time;
	cur_time_ns.tv_sec = (long)(rtime.QuadPart / 10000000);
	cur_time_ns.tv_nsec = (long)((rtime.QuadPart % 10000000)*100);
	TIMESPEC_TO_TIMEVAL(&cur_time, &cur_time_ns);

	TIMESPEC_TO_TIMEVAL(&targ_time, abstime);
	timersub(&targ_time, &cur_time, &delta_time);
	if(delta_time.tv_sec < 0) // abstime already passed?
		millis = 0;
	else {
		millis  = delta_time.tv_usec/1000;
		millis += delta_time.tv_sec *1000;
		if (delta_time.tv_usec % 1000) // round up to next millisecond
			millis++;
	}

	return usbi_cond_intwait(cond, mutex, millis);
}

int usbi_get_tid(void) {
	return GetCurrentThreadId();
}

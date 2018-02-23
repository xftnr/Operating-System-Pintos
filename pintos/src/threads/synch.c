/*
* Authors: Yige Wang, Pengdi Xia, Peijie Yang
* Date: 02/23/2018
* Description: Synchronization primitives of the System including semaphore,
* lock, and monitor.
*/

/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value)
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
// Yige, Pengdi, and Peijie Driving
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;    /* Old interrupt level. */

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) {
    /* Insert the thread based on its priority. */
    list_insert_ordered (&sema->waiters, &thread_current()->elem,
                         compare_priorities, NULL);
    thread_block ();
  }
  sema->value--;
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema)
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0)
    {
      sema->value--;
      success = true;
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
// Yige, Pengdi, and Peijie Driving
void
sema_up (struct semaphore *sema)
{
  enum intr_level old_level;    /* Old interrupt level. */

  ASSERT (sema != NULL);

  old_level = intr_disable ();

  // Peijie Driving

  /* Unblock the thread with highest priority. */
  if (!list_empty (&sema->waiters)) {
    /* Sort incase priority was changed while blocked. */
    list_sort(&sema->waiters, compare_priorities, NULL);
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  }
  sema->value++;

  /* Always check pre-emption after thread_unblock. */
  check_preemption();

  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void)
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++)
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_)
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++)
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  lock->max_priority = 0;
  sema_init (&lock->semaphore, 1);
}

/*
* Compares the max_priority between two locks.
*
* a - the element to be inserted
* b - the element already in the list
*
* Returns true if the max_priority of a is less than that of b.
*/
bool compare_priorities_lock(const struct list_elem *a,
                   const struct list_elem *b,
                   void *aux) {
  struct lock *l1 = NULL;
  struct lock *l2 = NULL;

  /* Gets the locks that contains element a and b. */
  l1 = list_entry (a, struct lock, holding_elem);
  l2 = list_entry (b, struct lock, holding_elem);

  /* Returns the comparison between wake up times. */
  return l1->max_priority < l2->max_priority;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

/*
* Acquires lock if available.
* Otherwise, add the lock to current thread's lock_waiting list.
*
* In a while loop, check whether donation is needed for the thread
* we are checking. If the lock this thread is waiting for has a lower
* priority, donate. Update the thread we are checking to this lock holder.
* Stop when there is no more threads.
*/
// Yige, Pengdi, and Peijie Driving
void
lock_acquire (struct lock *lock)
{

  enum intr_level old_level;    /* Old interrupt level. */
  struct lock *l = NULL;              /* Temp lock. */
  struct thread *t = NULL;            /* Temp thread. */
  struct list_elem *e = NULL;         /* Temp lock element. */

  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  old_level = intr_disable ();

  if (lock->holder != NULL) {     /* Cannot acquire lock. */
    /* Add add lock to thread's waiting list. */
    list_push_back (&thread_current ()->lock_waiting, &lock->waiting_elem);

    l = lock;
    t = lock->holder;

    while(t != NULL && thread_current ()->priority > t->priority) {
      /* Priority donation is needed.*/
      t->priority = thread_current ()->priority;
      if (t->priority > l->max_priority) {
        l->max_priority = t->priority;
      }

      /* Get next lock thread is waiting for and lock holder
         who may be a donee. */
      if (list_empty(&t->lock_waiting)) {
        break;
      }
      e = list_begin (&t->lock_waiting);
      l = list_entry(e, struct lock, waiting_elem);
      t = l->holder;
    }
  }

  sema_down (&lock->semaphore);

  /* Current thread now has the lock. Update list of locks holding. */
  list_insert_ordered (&thread_current ()->lock_holding,
                         &lock->holding_elem, compare_priorities_lock, NULL);

  lock->holder = thread_current ();

  intr_set_level (old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */

/*
* Release lock.
*
* Update max_priority of this lock to the next highest priority
* of threads waiting for this lock.
*
* Update current thread's priority to the next highest max_priority
* of the locks it holds, which should be the second highest donor's priority
* if nested donation happened. Else, return to old priority.
*
*/
// Yige, Pengdi, and Peijie Driving
void
lock_release (struct lock *lock)
{
  /* Second list element in lock waiting list. */
  struct list_elem *second = NULL;
  /* Waiting thread with highest priority. */
  struct thread *t = NULL;
  /* Elemant with highest lock max priority. */
  struct list_elem *max = NULL;
  /* Lock with highest priority in holding list.*/
  struct lock *l = NULL;
  /* Current thread. */
  struct thread *curr = NULL;

  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  enum intr_level old_level;

  old_level = intr_disable ();

  /* Update lock's priority to the highest priority of waiting threads. */
  if (list_size(&(&lock->semaphore)->waiters) > 1) {
    second = list_next( list_begin(&(&lock->semaphore)->waiters));
    t = list_entry (second, struct thread, elem);
    lock->max_priority = t->priority;
  } else {
    lock->max_priority = 0;
  }

  /* This lock is no longer held by thread. */
  list_remove(&lock->holding_elem);

  curr = thread_current();

  /* Update current thread's priority. */
  if (!list_empty(&curr->lock_holding)) {
    max = list_max(&curr->lock_holding, compare_priorities_lock, NULL);
    l = list_entry (max, struct lock, holding_elem);
    if (l->max_priority > curr-> old_priority) {  /* Nest donation happened. */
      curr-> priority = l->max_priority;
    } else {    /* Keep the higher old_priority. */
      curr-> priority = curr-> old_priority;
    }
  } else {  /* No more donation happened. */
    curr-> priority = curr-> old_priority;
  }

  lock->holder = NULL;
  sema_up (&lock->semaphore);

  intr_set_level (old_level);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock)
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem
{
  int priority;                       /* Priority. */
  struct list_elem elem;              /* List element. */
  struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/*
* Compares the priorities between two semaphores.
*
* a - the element to be inserted
* b - the element already in the list
*
* Returns true if the priority of a is less than that of b.
*
* Used in cond_wait.
*/
// Peijie Driving
static bool compare_priorities_sema(const struct list_elem *a,
                   const struct list_elem *b,
                   void *aux) {
  struct semaphore_elem *s1 = NULL;
  struct semaphore_elem *s2 = NULL;

  /* Gets the semaphore that contains elements a and b. */
  s1 = list_entry (a, struct semaphore_elem, elem);
  s2 = list_entry (b, struct semaphore_elem, elem);

  /* Returns the comparison between priorities. */
  return s1->priority > s2->priority;
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned bthreadack on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock)
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  sema_init (&waiter.semaphore, 0);

  waiter.priority = thread_current()->priority;

  // Peijie Driving

  /* Insert the waiter based on its priority. */
  list_insert_ordered (&cond->waiters, &waiter.elem,
                          compare_priorities_sema, NULL);

  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

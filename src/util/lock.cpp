#include "util/lock.h"
#include "util/system.h"
#include "util/assert.h"

namespace Thor {

struct Waiter {
	Waiter(System& sys)
		: sys{sys}
		, mutex{sys.scheduler.mutex_create(sys)}
		, cond{sys.scheduler.cond_create(sys)}
	{
	}
	~Waiter() {
		sys.scheduler.cond_destroy(sys, cond);
		sys.scheduler.mutex_destroy(sys, mutex);
	}
	System&           sys;
	Bool              park  = false;
	Scheduler::Mutex* mutex = nullptr;
	Scheduler::Cond*  cond  = nullptr;
	Waiter*           next  = nullptr;
	Waiter*           tail  = nullptr;
};

void Lock::lock_slow(System& sys) {
	Ulen spin_count = 0;
	Ulen spin_limit = 40;
	for (;;) {
		auto current_word = word_.load();
		if (!(current_word & IS_LOCKED_BIT)) {
			// Not possible for another thread to hold the queue lock while the lock
			// itself is no longer held since the queue lock is only acquired when the
			// lock is held. Thus the queue lock prevents unlock.
			THOR_ASSERT(sys, !(current_word & IS_QUEUE_LOCKED_BIT));
			if (word_.compare_exchange_weak(current_word, current_word | IS_LOCKED_BIT)) {
				// We acquired the lock.
				return;
			}
		}

		// No queue and haven't spun too much, just try again.
		if (!(current_word & ~QUEUE_HEAD_MASK) && spin_count < spin_limit) {
			spin_count++;
			sys.scheduler.yield(sys);
			continue;
		}

		// Put this thread on the queue. We can do this without allocating memory
		// since lock_slow will be held in place, meaning the stack frame is as good
		// a place as any for the Waiter.
		Waiter waiter{sys};
		
		// Reload the current word though since some time might've passed.
		current_word = word_.load();

		// Only proceed if the queue lock is not held, the Lock is held, and we
		// succeed in acquiring the queue lock.
		if ((current_word & IS_QUEUE_LOCKED_BIT)
			|| !(current_word & IS_LOCKED_BIT)
			|| !word_.compare_exchange_weak(current_word, current_word | IS_QUEUE_LOCKED_BIT))
		{
			sys.scheduler.yield(sys);
			continue;
		}

		waiter.park = true;

		// This thread now owns the queue. No other thread can enqueue or dequeue
		// until we're done. It's also not possible to release the Lock while we
		// hold this queue lock.
		auto head = reinterpret_cast<Waiter*>(current_word & ~QUEUE_HEAD_MASK);
		if (head) {
			// Put this thread at the end of the queue.
			head->tail->next = &waiter;
			head->tail = &waiter;

			// Release the queue lock now.
			current_word = word_.load();
			THOR_ASSERT(sys, current_word & ~QUEUE_HEAD_MASK);
			THOR_ASSERT(sys, current_word & IS_QUEUE_LOCKED_BIT);
			THOR_ASSERT(sys, current_word & IS_LOCKED_BIT);
			word_.store(current_word & ~IS_QUEUE_LOCKED_BIT);
		} else {
			// Otherwise this thread is the head of the queue.
			head = &waiter;
			waiter.tail = &waiter;

			// Release the queue lock and install ourselves as the head. No CAS needed
			// since we own the queue lock currently.
			current_word = word_.load();
			THOR_ASSERT(sys, ~(current_word & ~QUEUE_HEAD_MASK));
			THOR_ASSERT(sys, current_word & IS_QUEUE_LOCKED_BIT);
			THOR_ASSERT(sys, current_word & IS_LOCKED_BIT);
			auto new_word = current_word;
			new_word |= reinterpret_cast<Address>(head);
			new_word &= ~IS_QUEUE_LOCKED_BIT;
			word_.store(new_word);
		}

		// At this point any other thread which acquires the queue lock will see
		// this thread on the queue now, and any thread that acquires waiter's lock
		// will see that waiter wants to park.
		sys.scheduler.mutex_lock(sys, waiter.mutex);
		while (waiter.park) {
			sys.scheduler.cond_wait(sys, waiter.cond, waiter.mutex);
		}
		sys.scheduler.mutex_unlock(sys, waiter.mutex);

		THOR_ASSERT(sys, !waiter.park);
		THOR_ASSERT(sys, !waiter.next);
		THOR_ASSERT(sys, !waiter.tail);

		// Loop around and try again.
	}
}

void Lock::unlock_slow(System& sys) {
	// Generally speaking the fast path can only fail for three reasons:
	// 	1. Spurious CAS failure, unlikely but does happen.
	// 	2. Someone put a thread on the queue
	// 	3. The queue lock is held.
	//
	// However (3) is essentially the same as (2) since it can only be held if
	// someone is *about* to put a thread on the wait queue.

	// Here we will acquire (or release) the wait queue lock.
	for (;;) {
		auto current_word = word_.load();
		THOR_ASSERT(sys, current_word & IS_LOCKED_BIT);
		if (current_word == IS_LOCKED_BIT) {
			if (word_.compare_exchange_weak(IS_LOCKED_BIT, 0)) {
				// The fast path's weak CAS failed spuriously, but we made it to the
				// slow path and now we succeeded. This is a sort of "semi-fast" or
				// medium-path, the lock is unlocked though and we're done.
				return;
			}
			// Loop around and try again.
			sys.scheduler.yield(sys);
			continue;
		}

		// The slow path now begins.
		if (current_word & IS_QUEUE_LOCKED_BIT) {
			sys.scheduler.yield(sys);
			continue;
		}

		// Wasn't just the fast path's weak CAS failing spuriously. The queue lock
		// is not held either, so there should be an entry on the queue.
		THOR_ASSERT(sys, current_word & ~QUEUE_HEAD_MASK);

		if (word_.compare_exchange_weak(current_word, current_word | IS_QUEUE_LOCKED_BIT)) {
			break;
		}
	}

	auto current_word = word_.load();

	// Once we acquire the queue lock, the Lock must still be held and the queue
	// must be non-empty because only lock_slow could have held the queue lock and
	// if it did, then it only releases it after it puts something on the queue.
	THOR_ASSERT(sys, current_word & IS_LOCKED_BIT);
	THOR_ASSERT(sys, current_word & IS_QUEUE_LOCKED_BIT);
	auto head = reinterpret_cast<Waiter*>(current_word & ~QUEUE_HEAD_MASK);
	THOR_ASSERT(sys, head);

	// Either this was the only thread on the queue, in which case the queue can
	// be deleted, or there are still more threads on the queue, in which case we
	// need to create a new head for the queue.
	auto new_head = head->next;
	if (new_head) {
		new_head->tail = head->tail;
	}

	// Change the queue head, possibly removing it if new_head is nullptr. Like
	// in lock_slow, no CAS needed since we own the queue lock and the lock itself
	// so nothing about the lock can actually be changed at this point in time.
	current_word = word_.load();
	THOR_ASSERT(sys, current_word & IS_LOCKED_BIT);
	THOR_ASSERT(sys, current_word & IS_QUEUE_LOCKED_BIT);
	THOR_ASSERT(sys, (current_word & ~QUEUE_HEAD_MASK) == reinterpret_cast<Address>(head));
	auto new_word = current_word;
	new_word &= ~IS_LOCKED_BIT;                      // Release the lock
	new_word &= ~IS_QUEUE_LOCKED_BIT;                // Release the queue lock
	new_word &= QUEUE_HEAD_MASK;                     // Clear out the queue head
	new_word |= reinterpret_cast<Address>(new_head); // Install new queue head
	word_.store(new_word);

	// The lock is now available to be locked. We just have to wake up the old
	// queue head. This has to be done very carefully though.
	head->next = nullptr;
	head->tail = nullptr;

	// This scope can run either before or during the Waiter's critical section
	// acquired in lock_slow.
	{
		// So be sure to hold the lock across the call to signal because a spurious
		// wakeup could cause the thread at the head of the queue to exit and delete
		// head before we have a change to mutate it here.
		sys.scheduler.mutex_lock(sys, head->mutex);
		head->park = false;
		sys.scheduler.cond_signal(sys, head->cond);
		sys.scheduler.mutex_unlock(sys, head->mutex);
	}
}

} // namespace Thor
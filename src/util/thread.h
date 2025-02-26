#ifndef THOR_THREAD_H
#define THOR_THREAD_H
#include "util/system.h"

namespace Thor {

struct Thread {
	using Fn = void(System&, void*);

	static Maybe<Thread> start(System& sys, Fn fn, void* user = nullptr);

	void join();
	
	Thread(Thread&& other)
		: sys_{other.sys_}
		, thread_{exchange(other.thread_, nullptr)}
	{
	}

	~Thread() { join(); }

private:
	Thread(System& sys, Scheduler::Thread* thread)
		: sys_{sys}
		, thread_{thread}
	{
	}
	System&            sys_;
	Scheduler::Thread* thread_;
};

} // namespace Thor

#endif // THOR_THREAD_H
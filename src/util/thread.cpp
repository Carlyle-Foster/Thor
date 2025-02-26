#include "util/thread.h"

namespace Thor {

Maybe<Thread> Thread::start(System& sys, Fn fn, void* user) {
	auto thread = sys.scheduler.thread_start(sys, fn, user);
	if (!thread) {
		return {};
	}
	return Thread { sys, thread };
}

void Thread::join() {
	if (thread_) {
		sys_.scheduler.thread_join(sys_, thread_);
		thread_ = nullptr;
	}
}

} // namespace Thor
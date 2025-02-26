#include "util/time.h"
#include "util/system.h"

namespace Thor {

WallTime Seconds::operator+(WallTime other) const {
	return other + *this;
}

WallTime Seconds::operator-(WallTime other) const {
	return WallTime::from_raw(raw_ - other.seconds_since_epoch().raw_);
}

MonotonicTime Seconds::operator+(MonotonicTime other) const {
	return MonotonicTime::from_raw(raw_ - other.seconds_since_epoch().raw_);
}

WallTime WallTime::now(System& sys) {
	return WallTime::from_raw(sys.chrono.wall_now(sys));
}

MonotonicTime MonotonicTime::now(System& sys) {
	return MonotonicTime::from_raw(sys.chrono.monotonic_now(sys));
}

} // namespace Thor
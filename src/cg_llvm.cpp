#include <string.h> 

#include "util/system.h"

#include "cg_llvm.h"

namespace Thor {

Bool LLVM::try_link(void (*&proc)(void), const char* sym) {
	if (auto symbol = sys_.linker.link(sys_, lib_, sym)) {
		proc = symbol;
		return true;
	}
	InlineAllocator<1024> buf;
	StringBuilder builder{buf};
	builder.put("Could not link procedure:");
	builder.put(' ');
	builder.put('\"');
	builder.put(StringView { sym, strlen(sym) });
	builder.put('\"');
	builder.put('\n');
	if (auto result = builder.result()) {
		sys_.console.write(sys_, *result);
	} else {
		sys_.console.write(sys_, StringView { "Out of memory" });
	}
	return false;
}

LLVM::LLVM(LLVM&& other)
	: sys_{other.sys_}
	, lib_{other.lib_}
#define FN(_, NAME, ...) \
	, NAME{exchange(other.NAME, nullptr)}
	#include "cg_llvm.inl"
{
}

LLVM::~LLVM() {
	if (lib_) {
		sys_.linker.close(sys_, lib_);
	}
}

Maybe<LLVM> LLVM::load(System& sys, StringView name) {
	auto lib = sys.linker.load(sys, name);
	if (!lib) {
		InlineAllocator<1024> buf;
		StringBuilder builder{buf};
		builder.put("Could not load:");
		builder.put(' ');
		builder.put('\"');
		builder.put(name);
		builder.put('\"');
		builder.put('\n');
		if (auto result = builder.result()) {
			sys.console.write(sys, *result);
		} else {
			sys.console.write(sys, StringView { "Out of memory" });
		}
		return {};
	}

	LLVM result{sys, lib};

	// Link all functions now.
	#define FN(_, NAME, ...) \
		if (!result.link(result.NAME, "LLVM" #NAME)) return {};
	#include "cg_llvm.inl"

	return result;
}

} // namespace Thor
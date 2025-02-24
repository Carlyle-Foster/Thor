#ifndef THOR_CG_LLVM_H
#define THOR_CG_LLVM_H
#include "util/maybe.h"
#include "util/string.h"
#include "util/system.h"

namespace Thor {

struct LLVM {
	LLVM(System& sys, Linker::Library* lib)
		: sys_{sys}
		, lib_{lib}
	{
	}

	LLVM(LLVM&& other);
	~LLVM();

	static Maybe<LLVM> load(System& sys, StringView name);

	struct OpaqueContext;
	struct OpaqueModule;
	struct OpaqueType;
	struct OpaqueValue;
	struct OpaqueBasicBlock;
	struct OpaqueBuilder;
	struct OpaqueTargetMachineOptions;
	struct OpaqueTargetMachine;
	struct OpaqueTarget;
	struct OpaquePassBuilderOptions;
	struct OpaqueError;
	struct OpaqueAttribute;

	using ContextRef               = OpaqueContext*;
	using ModuleRef                = OpaqueModule*;
	using TypeRef                  = OpaqueType*;
	using ValueRef                 = OpaqueValue*;
	using BasicBlockRef            = OpaqueBasicBlock*;
	using BuilderRef               = OpaqueBuilder*;
	using TargetMachineOptionsRef  = OpaqueTargetMachineOptions*;
	using TargetMachineRef         = OpaqueTargetMachine*;
	using TargetRef                = OpaqueTarget*;
	using PassBuilderOptionsRef    = OpaquePassBuilderOptions*;
	using ErrorRef                 = OpaqueError*;
	using AttributeRef             = OpaqueAttribute*;
	using Bool                     = int;
	using Ulen                     = decltype(sizeof 0);
	using Opcode                   = int;
	using AttributeIndex           = unsigned;

	enum class CodeGenOptLevel       : int { None, Less, Default, Aggressive };
	enum class RelocMode             : int { Default, Static, PIC, DynamicNoPic, ROPI, RWPI, ROPI_RWPI };
	enum class CodeModel             : int { Default, JITDefault, Tiny, Small, Kernel, Medium, Large };
	enum class CodeGenFileType       : int { Assembly, Object };
	enum class VerifierFailureAction : int { AbortProcess, PrintMessage, ReturnStatus };
	enum class IntPredicate          : int { EQ = 32, NE, UGT, UGE, ULT, ULE, SGT, SGE, SLT, SLE };
	enum class RealPredicate         : int { False, OEQ, OGT, OGE, OLT, OLE, ONE, ORD, UNO, UEQ, UGT, UGE, ULT, ULE, UNE, True };

	enum class Linkage : int {
		External,
		AvailableExternally,
		OnceAny,
		OnceODR,
		OnceODRAutoHide,
		WeakAny,
		WeakODE,
		Appending,
		Internal,
		Private,
		DLLImport,
		DLLExport,
		ExternalWeak,
		Ghost,
		Common,
		LinkerPrivate,
		LinkerPrivateWeak
	};

private:
	Thor::Bool try_link(void (*&)(void), const char* sym);

	System&          sys_;
	Linker::Library* lib_;

public:
	template<typename F>
	[[nodiscard]] Thor::Bool link(F& proc, const char* sym) {
		return try_link(reinterpret_cast<void (*&)(void)>(proc), sym);
	}

	#define FN(RETURN, NAME, ...) \
		RETURN (*NAME)(__VA_ARGS__) = nullptr;
	#include "cg_llvm.inl"
};

} // namespace Thor

#endif // THOR_CG_LLVM_H
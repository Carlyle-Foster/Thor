#ifndef THOR_IR_H
#define THOR_IR_H
#include "util/slab.h"
#include "util/string.h"

namespace Thor {

struct MirSlabID {
	// Only 6-bit slab index (2^6 = 64)
	static inline constexpr const auto MAX = 64_u32;
	template<typename T>
	static Uint32 id() {
		static const Uint32 id = s_id++;
		return id;
	}
private:
	static inline Uint32 s_id;
};

struct MirNode {
	// Only 14-bit node index (2^14 = 16384)
	static inline constexpr const auto MAX = 16384_u32;
};

struct MirID {
	// Only 12-bit pool index(2^12 = 4096)
	static inline constexpr const auto MAX = 4096_u32;
	constexpr MirID() = default;
	constexpr MirID(Uint32 value)
		: value_{value}
	{
	}
	[[nodiscard]] constexpr auto is_valid() const {
		return value_ != ~0_u32;
	}
	[[nodiscard]] constexpr operator Bool() const {
		return is_valid();
	}
private:
	template<typename>
	friend struct MirRef;
	friend struct Mir;
	Uint32 value_ = ~0_u32;
};
static_assert(sizeof(MirID) == 4);

template<typename T>
struct MirRef {
	constexpr MirRef() = default;
	constexpr MirRef(MirID id)
		: id_{id}
	{
	}
	[[nodiscard]] constexpr auto is_valid() const {
		return id_.is_valid();
	}
	[[nodiscard]] constexpr operator Bool() const {
		return is_valid();
	}
	template<typename U>
	[[nodiscard]] constexpr operator MirRef<U>() const
		requires DerivedFrom<T, U>
	{
		return MirRef<U>{id_};
	}
private:
	friend struct Mir;
	MirID id_;
};

struct Mir;

struct MirType : MirNode {
};

struct MirAddr : MirNode {
	MirRef<MirType> type; // Always a pointer type
};
struct MirVal : MirNode {
	MirRef<MirType> type;
	void dump(const Mir&, StringBuilder& builder) const;
};
struct MirVar : MirNode {
	MirRef<MirAddr> addr;
};

struct MirInst : MirNode {
	virtual void dump(const Mir&, StringBuilder& builder) const = 0;
};

struct MirArithInst : MirInst {
	constexpr MirArithInst(MirRef<MirVal> lhs, MirRef<MirVal> rhs)
		: lhs{lhs}
		, rhs{rhs}
	{
	}
	virtual void dump(const Mir&, StringBuilder& builder) const;
	MirRef<MirVal> lhs;
	MirRef<MirVal> rhs;
};

struct MirBlock : MirNode {
	constexpr MirBlock(Allocator& allocator)
		: insts{allocator}
	{
	}
	void dump(const Mir&, StringBuilder& builder) const;
	MirRef<MirBlock>       parent;
	Array<MirRef<MirInst>> insts;
	Bool append(MirRef<MirInst> inst) {
		return insts.push_back(inst);
	}
};

struct Mir {
	constexpr Mir(Allocator& allocator)
		: slabs_{allocator}
	{
	}

	static inline constexpr const auto MAX = MirID::MAX * MirNode::MAX;

	template<typename T, typename... Ts>
	MirRef<T> create(Ts&&... args) {
		auto slab_idx = MirSlabID::id<T>();
		if (slab_idx >= slabs_.length() && !slabs_.resize(slab_idx + 1)) {
			return {};
		}
		auto& slab = slabs_[slab_idx];
		if (!slab) {
			slab.emplace(slabs_.allocator(), sizeof(T), MirNode::MAX);
		}
		if (auto slab_ref = slab->allocate()) {
			new ((*slab)[*slab_ref], Nat{}) T{forward<Ts>(args)...};
			return MirID { slab_idx * MAX + slab_ref->index };
		}
		return {};
	}

	template<typename T>
	constexpr const T& operator[](MirRef<T> ref) const {
		const auto slab_idx = ref.id_.value_ / MAX;
		const auto slab_ref = ref.id_.value_ % MAX;
		return *reinterpret_cast<const T*>((*slabs_[slab_idx])[SlabRef { slab_ref }]);
	}

	template<typename T>
	constexpr T& operator[](MirRef<T> ref) {
		const auto slab_idx = ref.id_.value_ / MAX;
		const auto slab_ref = ref.id_.value_ % MAX;
		return *reinterpret_cast<T*>((*slabs_[slab_idx])[SlabRef { slab_ref }]);
	}

	[[nodiscard]] constexpr Allocator& allocator() const {
		return slabs_.allocator();
	}

private:
	Array<Maybe<Slab>> slabs_;
};

struct MirBuilder {
	constexpr MirBuilder(Mir& mir)
		: mir_{mir}
		, blocks_{mir_.allocator()}
	{
	}

	enum class IPred : Uint8 {
		EQ, NE, UGT, UGE, ULT, ULE, IGT, IGE, ILT, SLE
	};
	enum class FPred : Uint8 {
		OEQ, OGT, OGE, OLT, OLE, ONE,
		UEQ, UGT, UGE, ULT, ULE, UNE,
	};

	const Array<MirRef<MirBlock>>& blocks() const {
		return blocks_;
	}

	Bool append(MirRef<MirBlock> block) {
		return blocks_.push_back(block);
	}

	// Arithmetic
	MirRef<MirVal> build_iadd(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_fadd(MirRef<MirVal> lhs, MirRef<MirVal> rhs) {
		return build_arith(lhs, rhs);
	}
	MirRef<MirVal> build_isub(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_fsub(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_imul(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_fmul(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_udiv(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_idiv(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_urem(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_irem(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_frem(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_ineg(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_fneg(MirRef<MirVal> lhs, MirRef<MirVal> rhs);

	// Relational
	MirRef<MirVal> build_icmp(IPred pred, MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_fcmp(FPred pred, MirRef<MirVal> lhs, MirRef<MirVal> rhs);

	// Bitwise
	MirRef<MirVal> build_shl(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_lshr(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_ashr(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_and(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_or(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_xor(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	MirRef<MirVal> build_not(MirRef<MirVal> lhs, MirRef<MirVal> rhs);

	// Memory
	MirRef<MirVal> build_alloca(MirRef<MirType> type);
	MirRef<MirVal> build_load(MirRef<MirType> type, MirRef<MirVal> value);
	MirRef<MirVal> build_store(MirRef<MirVal> dst, MirRef<MirVal> src);

	// Cast
	MirRef<MirVal> build_cast(MirRef<MirVal> value, MirRef<MirType> type); 

	// Terminators
	MirRef<MirVal> build_ret(Maybe<MirRef<MirVal>> value);
	MirRef<MirVal> build_br(MirRef<MirBlock>);
	MirRef<MirVal> build_cbr(MirRef<MirVal> cond, MirRef<MirBlock> on_true, MirRef<MirBlock> on_false);

	// MirRef<MirVal> build_memcpy(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
	// MirRef<MirVal> build_memset(MirRef<MirVal> lhs, MirRef<MirVal> rhs);
private:
	MirRef<MirVal> build_arith(MirRef<MirVal> lhs, MirRef<MirVal> rhs);

	Mir&                    mir_;
	Array<MirRef<MirBlock>> blocks_;
};

} // namespace Thor

#endif // THOR_IR_H
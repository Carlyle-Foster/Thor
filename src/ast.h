#ifndef THOR_AST_H
#define THOR_AST_H
#include "util/slab.h"
#include "util/string.h"

namespace Thor {

struct System;

struct AstSlabID {
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

struct AstNode {
	// Only 12-bit node index (2^12 = 4096)
	static inline constexpr const auto MAX = 4096_u32;
};

struct AstID {
	// Only 14-bit pool index (2^14 = 16384)
	static inline constexpr const auto MAX = 16384_u32;
	constexpr AstID() = default;
	constexpr AstID(Uint32 value)
		: value_{value}
	{
	}
	[[nodiscard]] constexpr Bool is_valid() const {
		return value_ != ~0_u32;
	}
	[[nodiscard]] constexpr operator Bool() const {
		return is_valid();
	}
private:
	template<typename>
	friend struct AstRef;
	friend struct Ast;
	Uint32 value_ = ~0_u32;
};
static_assert(sizeof(AstID) == 4);

struct Ast;

template<typename T>
struct AstRef {
	constexpr AstRef() = default;
	constexpr AstRef(AstID id)
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
	[[nodiscard]] constexpr operator AstRef<U>() const 
		requires DerivedFrom<T, U>
	{
		return AstRef<U>(id_);
	}
private:
	friend struct Ast;
	AstID id_;
};

struct AstStmt {
	virtual void dump(StringBuilder& builder) const = 0;
};

struct AstImportStmt : AstStmt {
	constexpr AstImportStmt(StringView path) : path{path} {}
	virtual void dump(StringBuilder& builder) const;
	StringView path;
};

struct AstPackageStmt : AstStmt {
	constexpr AstPackageStmt(StringView name) : name{name} {}
	virtual void dump(StringBuilder& builder) const;
	StringView name;
};

struct Ast {
	constexpr Ast(Allocator& allocator)
		: slabs_{allocator}
	{
	}

	static inline constexpr const auto MAX = AstID::MAX * AstNode::MAX;

	template<typename T, typename... Ts>
	AstRef<T> create(Ts&&... args) {
		auto slab_idx = AstSlabID::id<T>();
		if (slab_idx >= slabs_.length() && !slabs_.resize(slab_idx + 1)) {
			return {};
		}
		auto& slab = slabs_[slab_idx];
		if (!slab) {
			slab.emplace(slabs_.allocator(), sizeof(T), AstNode::MAX);
		}
		if (auto slab_ref = slab->allocate()) {
			new ((*slab)[*slab_ref], Nat{}) T{forward<Ts>(args)...};
			return AstID { slab_idx * MAX + slab_ref->index };
		}
		return {};
	}

	template<typename T>
	constexpr const T& operator[](AstRef<T> ref) const {
		const auto slab_idx = ref.id_.value_ / MAX;
		const auto slab_ref = ref.id_.value_ % MAX;
		return *reinterpret_cast<T*>((*slabs_[slab_idx])[SlabRef { slab_ref }]);
	}

private:
	Array<Maybe<Slab>> slabs_;
};


} // namespace Thor

#endif // THOR_AST_H
#ifndef THOR_AST_H
#define THOR_AST_H
#include "util/slab.h"
#include "util/string.h"
#include "lexer.h"

namespace Thor {

struct AstFile;
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
	friend struct AstFile;
	Uint32 value_ = ~0_u32;
};
static_assert(sizeof(AstID) == 4);

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
		return AstRef<U>{id_};
	}
private:
	friend struct AstFile;
	AstID id_;
};

struct AstExpr : AstNode {
	enum class Kind : Uint8 {
		BIN,
		UNARY,
		TERNARY,
		IDENT,
		UNDEF,
		CONTEXT,
		STRUCT,
		TYPE,
		PROC,
		INTEGER,
		FLOAT,
	};
	constexpr AstExpr(Kind kind)
		: kind{kind}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const = 0;
	Kind kind;
};

struct AstBinExpr : AstExpr {
	static constexpr const auto KIND = Kind::BIN;
	constexpr AstBinExpr(AstRef<AstExpr> lhs, AstRef<AstExpr> rhs, OperatorKind op)
		: AstExpr{KIND}
		, lhs{lhs}
		, rhs{rhs}
		, op{op}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> lhs;
	AstRef<AstExpr> rhs;
	OperatorKind    op;
};

struct AstUnaryExpr : AstExpr {
	static constexpr const auto KIND = Kind::UNARY;
	constexpr AstUnaryExpr(AstRef<AstExpr> operand, OperatorKind op)
		: AstExpr{KIND}
		, operand{operand}
		, op{op}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	OperatorKind    op;
};

struct AstTernaryExpr : AstExpr {
	static constexpr const auto KIND = Kind::TERNARY;
	constexpr AstTernaryExpr(AstRef<AstExpr> cond, AstRef<AstExpr> on_true, AstRef<AstExpr> on_false)
		: AstExpr{KIND}
		, cond{cond}
		, on_true{on_true}
		, on_false{on_false}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> cond;
	AstRef<AstExpr> on_true;
	AstRef<AstExpr> on_false;
};

struct AstIdentExpr : AstExpr {
	static constexpr const auto KIND = Kind::IDENT;
	constexpr AstIdentExpr(StringRef ident)
		: AstExpr{KIND}
		, ident{ident}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	StringRef ident;
};

struct AstIntExpr : AstExpr {
	static constexpr const auto KIND = Kind::INTEGER;
	constexpr AstIntExpr(Uint64 value)
		: AstExpr{KIND}
		, value{value}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	Uint64 value;
};

struct AstFloatExpr : AstExpr {
	static constexpr const auto KIND = Kind::FLOAT;
	constexpr AstFloatExpr(Float64 value)
		: AstExpr{KIND}
		, value{value}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	Float64 value;
};

struct AstUndefExpr : AstExpr {
	static constexpr const auto KIND = Kind::UNDEF;
	constexpr AstUndefExpr()
		: AstExpr{KIND}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
};

struct AstContextExpr : AstExpr {
	static constexpr const auto KIND = Kind::CONTEXT;
	constexpr AstContextExpr()
		: AstExpr{KIND}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
};

struct AstDeclStmt;

struct AstStructExpr : AstExpr {
	static constexpr const auto KIND = Kind::STRUCT;
	constexpr AstStructExpr(Array<AstRef<AstDeclStmt>>&& decls)
		: AstExpr{KIND}
		, decls{move(decls)}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	Array<AstRef<AstDeclStmt>> decls;
};

struct AstTypeExpr : AstExpr {
	static constexpr const auto KIND = Kind::TYPE;
	constexpr AstTypeExpr(AstRef<AstExpr> expr)
		: AstExpr{KIND}
		, expr{expr}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> expr;
};

struct AstBlockStmt;

struct AstProcExpr : AstExpr {
	static constexpr const auto KIND = Kind::PROC;
	constexpr AstProcExpr(Maybe<Array<AstRef<AstDeclStmt>>>&&   params,
		                    AstRef<AstBlockStmt>                body,
		                    AstRef<AstTypeExpr>                 ret)
		: AstExpr{KIND}
		, params{move(params)}
		, body{body}
		, ret{ret}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder) const;
	Maybe<Array<AstRef<AstDeclStmt>>> params;
	AstRef<AstBlockStmt>              body;
	AstRef<AstTypeExpr>               ret;
};

struct AstStmt : AstNode {
	enum struct Kind : Uint8 {
		EMPTY,
		EXPR,
		ASSIGN,
		BLOCK,
		IMPORT,
		PACKAGE,
		DEFER,
		BREAK,
		CONTINUE,
		FALLTHROUGH,
		IF,
		DECL,
	};

	constexpr AstStmt(Kind kind)
		: kind{kind}
	{
	}

	template<typename T>
	[[nodiscard]] constexpr Bool is_stmt() const {
		return kind == T::KIND;
	}

	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const = 0;

	Kind kind;
};

struct AstEmptyStmt : AstStmt {
	static constexpr const auto KIND = Kind::EMPTY;
	constexpr AstEmptyStmt()
		: AstStmt{KIND}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstExprStmt : AstStmt {
	static constexpr const auto KIND = Kind::EXPR;
	constexpr AstExprStmt(AstRef<AstExpr> expr)
		: AstStmt{KIND}
		, expr{expr}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstExpr> expr;
};

struct AstAssignStmt : AstStmt {
	static constexpr const auto KIND = Kind::ASSIGN;
	constexpr AstAssignStmt(Array<AstRef<AstExpr>>&& lhs,
	                        Token                    token,
	                        Array<AstRef<AstExpr>>&& rhs)
		: AstStmt{KIND}
		, lhs{move(lhs)}
		, rhs{move(rhs)}
		, token{token}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	Array<AstRef<AstExpr>> lhs;
	Array<AstRef<AstExpr>> rhs;
	Token                  token;
};

struct AstBlockStmt : AstStmt {
	static constexpr const auto KIND = Kind::BLOCK;
	constexpr AstBlockStmt(Array<AstRef<AstStmt>>&& stmts)
		: AstStmt{KIND}
		, stmts{move(stmts)}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	Array<AstRef<AstStmt>> stmts;
};

struct AstImportStmt : AstStmt {
	static constexpr const auto KIND = Kind::IMPORT;
	constexpr AstImportStmt(StringRef path)
		: AstStmt{KIND}
		, path{path}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	StringRef path;
};

struct AstPackageStmt : AstStmt {
	static constexpr const auto KIND = Kind::PACKAGE;
	constexpr AstPackageStmt(StringRef name)
		: AstStmt{KIND}
		, name{name}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	StringRef name;
};

struct AstDeferStmt : AstStmt {
	static constexpr const auto KIND = Kind::DEFER;
	constexpr AstDeferStmt(AstRef<AstStmt> stmt)
		: AstStmt{KIND}
		, stmt{stmt}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstStmt> stmt;
};

struct AstBreakStmt : AstStmt {
	static constexpr const auto KIND = Kind::BREAK;
	constexpr AstBreakStmt(StringRef label)
		: AstStmt{KIND}
		, label{label}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	StringRef label;
};

struct AstContinueStmt : AstStmt {
	static constexpr const auto KIND = Kind::CONTINUE;
	constexpr AstContinueStmt(StringRef label)
		: AstStmt{KIND}
		, label{label}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	StringRef label;
};

struct AstFallthroughStmt : AstStmt {
	static constexpr const auto KIND = Kind::FALLTHROUGH;
	constexpr AstFallthroughStmt()
		: AstStmt{KIND}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstIfStmt : AstStmt {
	static constexpr const auto KIND = Kind::IF;
	constexpr AstIfStmt(Maybe<AstRef<AstStmt>>&& init,
	                    AstRef<AstExpr>          cond,
	                    AstRef<AstStmt>          on_true,
	                    Maybe<AstRef<AstStmt>>&& on_false)
		: AstStmt{KIND}
		, init{move(init)}
		, cond{cond}
		, on_true{on_true}
		, on_false{move(on_false)}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	Maybe<AstRef<AstStmt>> init;
	AstRef<AstExpr>        cond;
	AstRef<AstStmt>        on_true;
	Maybe<AstRef<AstStmt>> on_false;
};

struct AstExpr;

struct AstDeclStmt : AstStmt {
	static constexpr const auto KIND = Kind::DECL;
	using List = Array<AstRef<AstExpr>>;
	constexpr AstDeclStmt(List&& lhs, AstRef<AstTypeExpr> type, Maybe<List>&& rhs)
		: AstStmt{KIND}
		, lhs{move(lhs)}
		, type{type}
		, rhs{move(rhs)}
	{
	}
	virtual void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	List                lhs;
	AstRef<AstTypeExpr> type;
	Maybe<List>         rhs;
};

struct AstFile {
	constexpr AstFile(Allocator& allocator)
		: string_table_{allocator}
		, slabs_{allocator}
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

	// Lookup an Ast node by AstRef
	template<typename T>
	constexpr const T& operator[](AstRef<T> ref) const {
		const auto slab_idx = ref.id_.value_ / MAX;
		const auto slab_ref = ref.id_.value_ % MAX;
		return *reinterpret_cast<const T*>((*slabs_[slab_idx])[SlabRef { slab_ref }]);
	}

	// Lookup a StrinfView by StringRef
	[[nodiscard]] constexpr StringView operator[](StringRef ref) const {
		return string_table_[ref];
	}

	[[nodiscard]] StringRef insert(StringView view) {
		return string_table_.insert(view);
	}

	[[nodiscard]] const StringTable& string_table() const {
		return string_table_;
	}

private:
	StringTable        string_table_;
	Array<Maybe<Slab>> slabs_;
};

} // namespace Thor

#endif // THOR_AST_H

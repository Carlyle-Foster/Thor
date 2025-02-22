#ifndef THOR_AST_H
#define THOR_AST_H
#include "util/slab.h"
#include "util/string.h"
#include "lexer.h"

namespace Thor {

struct AstExpr;
struct AstStmt;
struct AstType;
struct AstEnum;
struct AstAttribute;
struct AstDirective;

struct AstFile;
struct AstProcType;
struct AstBlockStmt;
struct AstDeclStmt;

using AstStringRef = StringRef;

// The following type represents a list of IDs.
struct AstIDArray {
	constexpr AstIDArray() = default;
	constexpr AstIDArray(Unit) : AstIDArray{} {}
	constexpr AstIDArray(Uint64 offset, Uint64 length)
		: offset_{offset}
		, length_{length}
	{
	}
	[[nodiscard]] constexpr auto is_empty() const { return length_ == 0; }
	[[nodiscard]] constexpr auto length() const { return length_; }
private:
	friend struct AstFile;
	Uint64 offset_ = 0; // The offset into Ast::ids_
	Uint64 length_ = 0; // The length of the array.
	// The actual IDs are essentially:
	// 	Ast::ids_.slice(offset_).truncate(length_)
};
static_assert(sizeof(AstIDArray) == 16);

// This is the same as AstIDArray but carries a compile-time type with it for
// convenience so that AstFile::operator[] can produce Slice<AstRef<T>> which
// represents the correct underlying type held in the list which is a list of
// AstIDs (where AstRef<T> is the typed version of that as well).
template<typename T>
struct AstRefArray {
	constexpr AstRefArray() = default;
	constexpr AstRefArray(Unit) : AstRefArray{} {}
	constexpr AstRefArray(AstIDArray id)
		: id_{id}
	{
	}
	[[nodiscard]] constexpr auto is_empty() const { return id_.is_empty(); }
	[[nodiscard]] constexpr auto length() const { return id_.length(); }
private:
	friend struct AstFile;
	AstIDArray id_;
};

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
	constexpr AstID(Unit) : AstID{} {}
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
	constexpr AstRef(Unit) : AstRef{} {}
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

// Enum
struct AstEnum : AstNode {
	constexpr AstEnum(AstStringRef name, AstRef<AstExpr> expr)
		: name{name}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef    name;
	AstRef<AstExpr> expr;
};

// Attribute
struct AstAttribute : AstNode {
	constexpr AstAttribute(AstStringRef name, AstRef<AstExpr> expr)
		: name{name}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef    name;
	AstRef<AstExpr> expr; // Optional value associated with attribute
};

// Directive
struct AstDirective : AstNode {
	constexpr AstDirective(AstStringRef name, AstRefArray<AstExpr> args)
		: name{name}
		, args{args}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef         name;
	AstRefArray<AstExpr> args;
};

// Expr
struct AstExpr : AstNode {
	enum class Kind : Uint8 {
		BIN,
		UNARY,
		TERNARY,
		IDENT,
		UNDEF,
		CONTEXT,
		PROC,
		INTEGER,
		FLOAT,
		STRING,
		CAST,
	};
	constexpr AstExpr(Kind kind)
		: kind{kind}
	{
	}
	template<typename T>
	[[nodiscard]] Bool is_expr() const
		requires DerivedFrom<T, AstExpr>
	{
		return kind == T::KIND;
	}
	template<typename T>
	[[nodiscard]] const T* to_expr() const
		requires DerivedFrom<T, AstExpr>
	{
		return is_expr<T>() ? static_cast<const T*>(this) : nullptr;
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
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
	void dump(const AstFile& ast, StringBuilder& builder) const;
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
	void dump(const AstFile& ast, StringBuilder& builder) const;
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
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> cond;
	AstRef<AstExpr> on_true;
	AstRef<AstExpr> on_false;
};

struct AstIdentExpr : AstExpr {
	static constexpr const auto KIND = Kind::IDENT;
	constexpr AstIdentExpr(AstStringRef ident)
		: AstExpr{KIND}
		, ident{ident}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef ident;
};

struct AstUndefExpr : AstExpr {
	static constexpr const auto KIND = Kind::UNDEF;
	constexpr AstUndefExpr()
		: AstExpr{KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
};

struct AstContextExpr : AstExpr {
	static constexpr const auto KIND = Kind::CONTEXT;
	constexpr AstContextExpr()
		: AstExpr{KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
};

struct AstProcExpr : AstExpr {
	static constexpr const auto KIND = Kind::PROC;
	constexpr AstProcExpr(AstRef<AstProcType> type, AstRef<AstBlockStmt> body)
		: AstExpr{KIND}
		, type{type}
		, body{body}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstProcType>   type;
	AstRef<AstBlockStmt>  body;
};

struct AstIntExpr : AstExpr {
	static constexpr const auto KIND = Kind::INTEGER;
	constexpr AstIntExpr(Uint64 value)
		: AstExpr{KIND}
		, value{value}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	Uint64 value;
};

struct AstFloatExpr : AstExpr {
	static constexpr const auto KIND = Kind::FLOAT;
	constexpr AstFloatExpr(Float64 value)
		: AstExpr{KIND}
		, value{value}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	Float64 value;
};

struct AstStringExpr : AstExpr {
	static constexpr const auto KIND = Kind::STRING;
	constexpr AstStringExpr(AstStringRef value)
		: AstExpr{KIND}
		, value{value}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef value;
};

struct AstCastExpr : AstExpr {
	static constexpr const auto KIND = Kind::CAST;
	constexpr AstCastExpr(AstRef<AstType> type, AstRef<AstExpr> expr)
		: AstExpr{KIND}
		, type{type}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> type; // When !type this is an auto_cast
	AstRef<AstExpr> expr;
};

// The reference Odin compiler treats all types as expressions in the ast. Here,
// we prefer a more typed ast where types are their own category.
//
// There are a few places in the Odin language where an expression evaluates to
// a type, but these expressions are regular and can be separately described by
// their own grammar independent from an expression.
//
// StructType   := 'struct' '{' (Field (',' Field)*)? '}'
// UnionType    := 'union' '{' (Type (',' Type)*)? '}'
// EnumType     := 'enum' '{' (EnumValue (',' EnumValue)*)? '}'
// EnumValue    := Ident ('=' Expr)?
// ProcType     := 'proc' StrLit? '(' (Field (',' Field)*)? ')' ('->' (Type | (Field (',' Field)+))
// PtrType      := '^' Type
// MultiPtrType := '[^]' Type
// SliceType    := '[]' Type
// ArrayType    := '[' (Expr | '?') ']' Type
// NamedType    := Ident ('.' Ident)?
// ParamType    := NamedType '(' (Expr (',' Expr)*)? ')'
// ParenType    := '(' Type ')'
// Type         := Directive* AnyType
// Directive    := '#' Ident ('(' (Expr (',' Expr)*)? ')')?
//
// ParamType is the interesting one as it handles all "parameterized" types. The
// types where the reference compiler represents with an expression node. Some
// examples of a parameterized types include:
//
//	ParaPolyStruct(int)
//	ParaPolyUnion(123, 456)
//	type_of(expr)
//	intrinsics.something(T)
//
// * AnyType is any of the types above excluding Type itself

struct AstType : AstNode {
	enum class Kind : Uint8 {
		STRUCT,    // struct { }
		UNION,     // union { }
		ENUM,      // enum { }
		PROC,      // proc()
		PTR,       // ^T
		MULTIPTR,  // [^]T
		SLICE,     // []T
		ARRAY,     // [Expr]T or [?]T (could be an EnumType, but enum arrays are in fact not part of the grammar)
		DYNARRAY,  // [dynamic]T
		MAP,       // map[K]V
		MATRIX,    // matrix[r,c]T
		NAMED,     // Name
		PARAM,     // T(args)
		PAREN,     // (T)
		DISTINCT,  // distinct T
	};
	constexpr AstType(Kind kind)
		: kind{kind}
	{
	}
	template<typename T>
	[[nodiscard]] constexpr Bool is_type() const
		requires DerivedFrom<T, AstType>
	{
		return kind == T::KIND;
	}
	template<typename T>
	[[nodiscard]] const T* to_type() const
		requires DerivedFrom<T, AstType>
	{
		return is_type<T>() ? static_cast<const T*>(this) : nullptr;
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	Kind kind;
};

struct AstUnionType : AstType {
	static constexpr const auto KIND = Kind::UNION;
	constexpr AstUnionType(AstRefArray<AstType> types)
		: AstType{KIND}
		, types{types}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRefArray<AstType> types;
};

struct AstEnumType : AstType {
	static constexpr const auto KIND = Kind::ENUM;
	constexpr AstEnumType(AstRef<AstType> base, AstRefArray<AstEnum> enums)
		: AstType{KIND}
		, base{base}
		, enums{enums}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType>      base;
	AstRefArray<AstEnum> enums;
};

struct AstProcType : AstType {
	static constexpr const auto KIND = Kind::PROC;
	void dump(const AstFile& ast, StringBuilder& builder) const;
	// TODO(dweiler): FieldList args
	// TODO(dweiler): FieldList rets
};

struct AstPtrType : AstType {
	static constexpr const auto KIND = Kind::PTR;
	constexpr AstPtrType(AstRef<AstType> base)
		: AstType{KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstMultiPtrType : AstType {
	static constexpr const auto KIND = Kind::MULTIPTR;
	constexpr AstMultiPtrType(AstRef<AstType> base)
		: AstType{KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstSliceType : AstType {
	static constexpr const auto KIND = Kind::SLICE;
	constexpr AstSliceType(AstRef<AstType> base)
		: AstType{KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstArrayType : AstType {
	static constexpr const auto KIND = Kind::ARRAY;
	constexpr AstArrayType(AstRef<AstExpr> size, AstRef<AstType> base)
		: AstType{KIND}
		, size{size}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> size; // Optional, empty represents [?]T
	AstRef<AstType> base;
};

struct AstDynArrayType : AstType {
	static constexpr const auto KIND = Kind::DYNARRAY;
	constexpr AstDynArrayType(AstRef<AstType> base)
		: AstType{KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstMapType : AstType {
	static constexpr const auto KIND = Kind::MAP;
	constexpr AstMapType(AstRef<AstType> kt, AstRef<AstType> vt)
		: AstType{KIND}
		, kt{kt}
		, vt{vt}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> kt;
	AstRef<AstType> vt;
};

struct AstMatrixType : AstType {
	static constexpr const auto KIND = Kind::MATRIX;
	constexpr AstMatrixType(AstRef<AstExpr> rows, AstRef<AstExpr> cols, AstRef<AstType> base)
		: AstType{KIND}
		, rows{rows}
		, cols{cols}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> rows;
	AstRef<AstExpr> cols;
	AstRef<AstType> base;
};

struct AstNamedType : AstType {
	static constexpr const auto KIND = Kind::NAMED;
	constexpr AstNamedType(AstStringRef pkg, AstStringRef name)
		: AstType{KIND}
		, pkg{pkg}
		, name{name}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef pkg; // Optional package name
	AstStringRef name;
};

struct AstParamType : AstType {
	static constexpr const auto KIND = Kind::PARAM;
	constexpr AstParamType(AstRef<AstNamedType> name, AstRefArray<AstExpr> exprs)
		: AstType{KIND}
		, name{name}
		, exprs{exprs}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstNamedType> name;
	AstRefArray<AstExpr> exprs;
};

struct AstParenType : AstType {
	static constexpr const auto KIND = Kind::PAREN;
	constexpr AstParenType(AstRef<AstType> type)
		: AstType{KIND}
		, type{type}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> type;
};

struct AstDistinctType : AstType {
	static constexpr const auto KIND = Kind::DISTINCT;
	constexpr AstDistinctType(AstRef<AstType> type)
		: AstType{KIND}
		, type{type}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> type;
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
		RETURN,
		BREAK,
		CONTINUE,
		FALLTHROUGH,
		FOREIGNIMPORT,
		IF,
		WHEN,
		DECL,
		USING,
	};

	constexpr AstStmt(Kind kind)
		: kind{kind}
	{
	}

	template<typename T>
	[[nodiscard]] constexpr Bool is_stmt() const
		requires DerivedFrom<T, AstStmt>
	{
		return kind == T::KIND;
	}

	template<typename T>
	const T* to_stmt() const
		requires DerivedFrom<T, AstStmt>
	{
		return is_stmt<T>() ? static_cast<const T*>(this) : nullptr;
	}

	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;

	Kind kind;
};

struct AstEmptyStmt : AstStmt {
	static constexpr const auto KIND = Kind::EMPTY;
	constexpr AstEmptyStmt()
		: AstStmt{KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstExprStmt : AstStmt {
	static constexpr const auto KIND = Kind::EXPR;
	constexpr AstExprStmt(AstRef<AstExpr> expr)
		: AstStmt{KIND}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstExpr> expr;
};

struct AstAssignStmt : AstStmt {
	static constexpr const auto KIND = Kind::ASSIGN;
	constexpr AstAssignStmt(AstRefArray<AstExpr> lhs,
	                        Token                token,
	                        AstRefArray<AstExpr> rhs)
		: AstStmt{KIND}
		, lhs{lhs}
		, rhs{rhs}
		, token{token}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRefArray<AstExpr> lhs;
	AstRefArray<AstExpr> rhs;
	Token                token;
};

struct AstBlockStmt : AstStmt {
	static constexpr const auto KIND = Kind::BLOCK;
	constexpr AstBlockStmt(AstRefArray<AstStmt> stmts)
		: AstStmt{KIND}
		, stmts{stmts}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRefArray<AstStmt> stmts;
};

struct AstImportStmt : AstStmt {
	static constexpr const auto KIND = Kind::IMPORT;
	constexpr AstImportStmt(AstStringRef alias, AstRef<AstStringExpr> expr)
		: AstStmt{KIND}
		, alias{alias}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef          alias;
	AstRef<AstStringExpr> expr;
};

struct AstPackageStmt : AstStmt {
	static constexpr const auto KIND = Kind::PACKAGE;
	constexpr AstPackageStmt(AstStringRef name)
		: AstStmt{KIND}
		, name{name}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef name;
};

struct AstDeferStmt : AstStmt {
	static constexpr const auto KIND = Kind::DEFER;
	constexpr AstDeferStmt(AstRef<AstStmt> stmt)
		: AstStmt{KIND}
		, stmt{stmt}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstStmt> stmt;
};

struct AstReturnStmt : AstStmt {
	static constexpr const auto KIND = Kind::RETURN;
	constexpr AstReturnStmt(AstRefArray<AstExpr> exprs)
		: AstStmt{KIND}
		, exprs{exprs}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRefArray<AstExpr> exprs;
};

struct AstBreakStmt : AstStmt {
	static constexpr const auto KIND = Kind::BREAK;
	constexpr AstBreakStmt(AstStringRef label)
		: AstStmt{KIND}
		, label{label}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef label;
};

struct AstContinueStmt : AstStmt {
	static constexpr const auto KIND = Kind::CONTINUE;
	constexpr AstContinueStmt(AstStringRef label)
		: AstStmt{KIND}
		, label{label}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef label;
};

struct AstFallthroughStmt : AstStmt {
	static constexpr const auto KIND = Kind::FALLTHROUGH;
	constexpr AstFallthroughStmt()
		: AstStmt{KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstForeignImportStmt : AstStmt {
	static constexpr const auto KIND = Kind::FOREIGNIMPORT;
	constexpr AstForeignImportStmt(AstStringRef ident, AstRefArray<AstExpr> names)
		: AstStmt{KIND}
		, ident{ident}
		, names{names}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef         ident; // Optional
	AstRefArray<AstExpr> names;
};

struct AstIfStmt : AstStmt {
	static constexpr const auto KIND = Kind::IF;
	constexpr AstIfStmt(AstRef<AstStmt> init,
	                    AstRef<AstExpr> cond,
	                    AstRef<AstStmt> on_true,
	                    AstRef<AstStmt> on_false)
		: AstStmt{KIND}
		, init{init} 
		, cond{cond}
		, on_true{on_true}
		, on_false{on_false}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstStmt> init; // Optional
	AstRef<AstExpr> cond;
	AstRef<AstStmt> on_true;
	AstRef<AstStmt> on_false; // Optional
};

struct AstWhenStmt : AstStmt {
	static constexpr const auto KIND = Kind::WHEN;
	constexpr AstWhenStmt(AstRef<AstExpr> cond, AstRef<AstBlockStmt> on_true, AstRef<AstBlockStmt> on_false)
		: AstStmt{KIND}
		, cond{cond}
		, on_true{on_true}
		, on_false{on_false}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstExpr>      cond;
	AstRef<AstBlockStmt> on_true;
	AstRef<AstBlockStmt> on_false; // Optional
};

struct AstExpr;

struct AstDeclStmt : AstStmt {
	static constexpr const auto KIND = Kind::DECL;
	using List = AstRefArray<AstExpr>;
	constexpr AstDeclStmt(List lhs, AstRef<AstType> type, Maybe<List>&& rhs)
		: AstStmt{KIND}
		, lhs{move(lhs)}
		, type{type}
		, rhs{move(rhs)}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	List            lhs;
	AstRef<AstType> type;
	Maybe<List>     rhs;
};

struct AstUsingStmt : AstStmt {
	static constexpr const auto KIND = Kind::USING;
	constexpr AstUsingStmt(AstRef<AstExpr> expr)
		: AstStmt{KIND}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstExpr> expr;
};

// It is important that none of the Ast node types are polymorphic because they
// can be serialized as nothing more than a flat array of bytes. This series of
// static asserts checks that. If you're reading this it's because you added a
// virtual function to one of the derived or base classes and this is strictly
// not supported. You can switch on the kind member to emulate the polymorphic 
// behavior.
static_assert(!is_polymorphic<AstExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstBinExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstUnaryExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstTernaryExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstIdentExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstUndefExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstContextExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstProcExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstIntExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstFloatExpr>, "Cannot be polymorphic");

static_assert(!is_polymorphic<AstStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstEmptyStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstExprStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstAssignStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstBlockStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstImportStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstPackageStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstDeferStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstBreakStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstContinueStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstFallthroughStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstIfStmt>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstDeclStmt>, "Cannot be polymorphic");

struct AstFile {
	static Maybe<AstFile> create(Allocator& allocator, StringView filename);

	StringView filename() const {
		return string_table_[filename_];
	}

	AstFile(AstFile&&) = default;
	~AstFile();

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

	// Lookup a StringView by AstStringRef
	[[nodiscard]] constexpr StringView operator[](AstStringRef ref) const {
		return string_table_[ref];
	}
	// Lookup a Slice<AstRef<T>> by AstRefArray
	template<typename T>
	[[nodiscard]] constexpr Slice<const AstRef<T>> operator[](AstRefArray<T> ref) const {
		return ids_.slice()
		           .slice(ref.id_.offset_)
		           .truncate(ref.id_.length_)
		           .template cast<const AstRef<T>>();
	}

	[[nodiscard]] AstStringRef insert(StringView view) {
		return string_table_.insert(view);
	}

	template<typename T>
	[[nodiscard]] AstRefArray<T> insert(Array<AstRef<T>>&& refs) {
		const auto ids = refs.slice().template cast<const AstID>();
		return insert(ids);
	}

	[[nodiscard]] const StringTable& string_table() const {
		return string_table_;
	}

private:
	[[nodiscard]] AstIDArray insert(Slice<const AstID> ids);

	AstFile(StringTable&& string_table, Allocator& allocator, AstStringRef filename)
		: string_table_{move(string_table)}
		, slabs_{allocator}
		, ids_{allocator}
		, filename_{filename}
	{
	}

	// The flattened Ast structure is held here with nodes indexing these. In
	// detail:
	//  * AstStringRef is an offset into [string_table_] which is just a list of
	//    UTF-8 characters. The AstStringRef also stores a Uint32 length for when
	//    to stop reading the string in the table.
	//  * AstRef<T> is a typed AstID which indexes [slab_] based on the type,
	//    which then further indexes [Slab::caches_], which then further indexes
	//    [Pool::data_]. The triple indirection works by decomposing a single
	//    Uint32 index into a 6-bit [slab_] index. A 12-bit [Slab::caches_] index,
	//    and a 14-bit [Pool::data_] index. You can think of reading the data for
	//    a AstRef<T> or AstID as slabs_[d0(id)].caches_[d1(id)].data_[d2(id)]
	//    where d0, d1, and d2 are decoding functions which take 6, 12 and 14 bits
	//    from the 32-bit [id] respectively.
	//  * AstRefArray<T> is a typed AstIDArray which indexes [ids_] based on an
	//    offset and length stored in the AstRefArray itself. The [ids_] array is
	//    just an array of AstID, i.e Uint32.
	StringTable        string_table_;
	Array<Maybe<Slab>> slabs_;
	Array<AstID>       ids_;
	AstStringRef       filename_;
};

} // namespace Thor

#endif // THOR_AST_H

#ifndef THOR_AST_H
#define THOR_AST_H
#include "util/slab.h"
#include "util/string.h"
#include "util/assert.h"
#include "util/system.h"

#include "lexer.h"

namespace Thor {

struct AstExpr;
struct AstStmt;
struct AstType;
struct AstField;
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
	static Uint32 id(System& sys) {
		static const Uint32 id = s_id++;
		// We've run out of IDs for slabs. This indicates that there are too many
		// distinct types and they will need to be consolidated. The scheme used by
		// this representation can only support up to [MAX] unique types.
		THOR_ASSERT(sys, id <= MAX);
		return id;
	}
private:
	static inline Uint32 s_id;
};

struct AstNode {
	// Only 10-bit node index (2^10 = 2048)
	static inline constexpr const auto MAX = 1024_u32;
	constexpr AstNode(Uint32 offset)
		: offset{offset}
	{
	}
	Uint32 offset;
};

struct AstID {
	// Only 16-bit pool index (2^16 = 65536)
	static inline constexpr const auto MAX = 65536_u32;
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

// Field
struct AstField : AstNode {
	constexpr AstField(Uint32 offset, AstRef<AstExpr> operand, AstRef<AstExpr> expr)
		: AstNode{offset}
		, operand{operand}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	AstRef<AstExpr> expr; // Optional value associated with attribute, enum, or parameter
};

using AstAttribute = AstField;

// Directive
struct AstDirective : AstNode {
	constexpr AstDirective(Uint32 offset, AstStringRef name, AstRefArray<AstExpr> args)
		: AstNode{offset}
		, name{name}
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
		IF,
		WHEN,
		DEREF,
		OR_RETURN,
		OR_BREAK,
		OR_CONTINUE,
		CALL,
		IDENT,
		UNDEF,
		CONTEXT,
		PROC,
		SLICE,
		INDEX,
		INT,
		FLOAT,
		STRING,
		IMAGINARY,
		COMPOUND,
		CAST,
		SELECTOR,
		ACCESS,
		ASSERT,
		TYPE,
	};
	constexpr AstExpr(Uint32 offset, Kind kind)
		: AstNode{offset}
		, kind{kind}
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
	constexpr AstBinExpr(Uint32 offset, AstRef<AstExpr> lhs, AstRef<AstExpr> rhs, OperatorKind op)
		: AstExpr{offset, KIND}
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
	constexpr AstUnaryExpr(Uint32 offset, AstRef<AstExpr> operand, OperatorKind op)
		: AstExpr{offset, KIND}
		, operand{operand}
		, op{op}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	OperatorKind    op;
};

struct AstIfExpr : AstExpr {
	static constexpr const auto KIND = Kind::IF;
	constexpr AstIfExpr(Uint32 offset, AstRef<AstExpr> cond, AstRef<AstExpr> on_true, AstRef<AstExpr> on_false)
		: AstExpr{offset, KIND}
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

struct AstWhenExpr : AstExpr {
	static constexpr const auto KIND = Kind::WHEN;
	constexpr AstWhenExpr(Uint32 offset, AstRef<AstExpr> cond, AstRef<AstExpr> on_true, AstRef<AstExpr> on_false)
		: AstExpr{offset, KIND}
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

struct AstDerefExpr : AstExpr {
	static constexpr const auto KIND = Kind::DEREF;
	constexpr AstDerefExpr(Uint32 offset, AstRef<AstExpr> operand)
		: AstExpr{offset, KIND}
		, operand{operand}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
};

struct AstOrReturnExpr : AstExpr {
	static constexpr const auto KIND = Kind::OR_RETURN;
	constexpr AstOrReturnExpr(Uint32 offset, AstRef<AstExpr> operand)
		: AstExpr{offset, KIND}
		, operand{operand}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
};

struct AstOrBreakExpr : AstExpr {
	static constexpr const auto KIND = Kind::OR_BREAK;
	constexpr AstOrBreakExpr(Uint32 offset, AstRef<AstExpr> operand)
		: AstExpr{offset, KIND}
		, operand{operand}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
};

struct AstOrContinueExpr : AstExpr {
	static constexpr const auto KIND = Kind::OR_CONTINUE;
	constexpr AstOrContinueExpr(Uint32 offset, AstRef<AstExpr> operand)
		: AstExpr{offset, KIND}
		, operand{operand}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
};

// Represents a call expression, e.g:
// 	operand()
// 	operand(a, b)
// 	operand(10, a=10)
// 	etc
//
// The [args] list stores the argument list for the callable operand. The list
// is encoded with AstField which is a node that encodes: (Expr ('=' Expr)?),
// this permits calling with named arguments.
struct AstCallExpr : AstExpr {
	static constexpr const auto KIND = Kind::CALL;
	constexpr AstCallExpr(Uint32 offset, AstRef<AstExpr> operand, AstRefArray<AstField> args)
		: AstExpr{offset, KIND}
		, operand{operand}
		, args{args}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr>       operand;
	AstRefArray<AstField> args;
};

// Represents an indent expression.
struct AstIdentExpr : AstExpr {
	static constexpr const auto KIND = Kind::IDENT;
	constexpr AstIdentExpr(Uint32 offset, AstStringRef ident)
		: AstExpr{offset, KIND}
		, ident{ident}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef ident;
};

// Represents an undef expression.
struct AstUndefExpr : AstExpr {
	static constexpr const auto KIND = Kind::UNDEF;
	constexpr AstUndefExpr(Uint32 offset)
		: AstExpr{offset, KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
};

// Represents a context expression.
struct AstContextExpr : AstExpr {
	static constexpr const auto KIND = Kind::CONTEXT;
	constexpr AstContextExpr(Uint32 offset)
		: AstExpr{offset, KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
};

// Represents a procedure literal expression.
struct AstProcExpr : AstExpr {
	static constexpr const auto KIND = Kind::PROC;
	constexpr AstProcExpr(Uint32 offset, AstRef<AstProcType> type, AstRef<AstBlockStmt> body)
		: AstExpr{offset, KIND}
		, type{type}
		, body{body}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstProcType>   type;
	AstRef<AstBlockStmt>  body;
};

// Represents a slice expression, e.g:
// 	1. operand[:]
// 	2. operand[:rhs]
// 	3. operand[lhs:]
// 	4. operand[lhs:rhs]
//
// This is encoded by making both [lhs] and [rhs] optional, that is:
// 	1. is encoded as [lhs] = nil, [rhs] = nil.
// 	2. is encoded as [lhs] = nil, [rhs] != nil.
// 	3. is encoded as [lhs] != nil, [rhs] = nil.
// 	4. is encoded as [lhs] != nil, [rhs] != nil.
struct AstSliceExpr : AstExpr {
	static constexpr const auto KIND = Kind::SLICE;
	constexpr AstSliceExpr(Uint32 offset, AstRef<AstExpr> operand, AstRef<AstExpr> lhs, AstRef<AstExpr> rhs)
		: AstExpr{offset, KIND}
		, operand{operand}
		, lhs{lhs}
		, rhs{rhs}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	AstRef<AstExpr> lhs; // Optional
	AstRef<AstExpr> rhs; // Optional

};

// Represents an indexing expression, e.g:
// 	operand[lhs]
// 	operand[lhs, rhs]
//
// The latter format is reserved for indexing matrices and is encoded when [rhs]
// is not nil.
struct AstIndexExpr : AstExpr {
	static constexpr const auto KIND = Kind::INDEX;
	constexpr AstIndexExpr(Uint32 offset, AstRef<AstExpr> operand, AstRef<AstExpr> lhs, AstRef<AstExpr> rhs)
		: AstExpr{offset, KIND}
		, operand{operand}
		, lhs{lhs}
		, rhs{rhs}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	AstRef<AstExpr> lhs;
	AstRef<AstExpr> rhs; // Optional
	// Represents indexing in Odin, that is:
	// 	operand[lhs]
	//
	// However if [rhs] is present it represents matrix indexing:
	// 	operand[lhs, rhs]
};

struct AstIntExpr : AstExpr {
	static constexpr const auto KIND = Kind::INT;
	constexpr AstIntExpr(Uint32 offset, Uint64 value)
		: AstExpr{offset, KIND}
		, value{value}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	Uint64 value;
};

struct AstFloatExpr : AstExpr {
	static constexpr const auto KIND = Kind::FLOAT;
	constexpr AstFloatExpr(Uint32 offset, Float64 value)
		: AstExpr{offset, KIND}
		, value{value}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	Float64 value;
};

struct AstStringExpr : AstExpr {
	static constexpr const auto KIND = Kind::STRING;
	constexpr AstStringExpr(Uint32 offset, AstStringRef value)
		: AstExpr{offset, KIND}
		, value{value}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef value;
};

struct AstImaginaryExpr : AstExpr {
	static constexpr const auto KIND = Kind::IMAGINARY;
	constexpr AstImaginaryExpr(Uint32 offset, Float64 value)
		: AstExpr{offset, KIND}
		, value{value}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	Float64 value;
};

// Represents a compound literal expression, e.g:
//	{ ... }
struct AstCompoundExpr : AstExpr {
	static constexpr const auto KIND = Kind::COMPOUND;
	constexpr AstCompoundExpr(Uint32 offset, AstRefArray<AstField> fields)
		: AstExpr{offset, KIND}
		, fields{fields}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRefArray<AstField> fields;
};

// Represents a cast expression, e.g:
//	1. type(expr)
//	2. cast(type)expr
//	3. auto_cast expr
//
// There is no distinction between (1) and (2) in this representation. Instead
// the parser canonicalizes to a single form. The (3) case is encoded with an
// empty type.
struct AstCastExpr : AstExpr {
	static constexpr const auto KIND = Kind::CAST;
	constexpr AstCastExpr(Uint32 offset, AstRef<AstType> type, AstRef<AstExpr> expr)
		: AstExpr{offset, KIND}
		, type{type}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> type; // When !type this is an auto_cast
	AstRef<AstExpr> expr;
};

// Represents an implicit selector expression.
//	.name
struct AstSelectorExpr : AstExpr {
	static constexpr const auto KIND = Kind::SELECTOR;
	constexpr AstSelectorExpr(Uint32 offset, AstStringRef name)
		: AstExpr{offset, KIND}
		, name{name}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstStringRef name;
};

// Represents a field access, e.g:
//	operand.field
//	operand->field
//
// The latter form is encoded with arrow = true.
struct AstAccessExpr : AstExpr {
	static constexpr const auto KIND = Kind::ACCESS;
	constexpr AstAccessExpr(Uint32 offset, AstRef<AstExpr> operand, AstStringRef field, Bool arrow)
		: AstExpr{offset, KIND}
		, operand{operand}
		, field{field}
		, arrow{arrow}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	AstStringRef    field;
	Bool            arrow;
};

// Represents a type assertion, e.g:
//	operand.(type)
//	operand.?
//
// The latter form is encoded with empty type
struct AstAssertExpr : AstExpr {
	static constexpr const auto KIND = Kind::ASSERT;
	constexpr AstAssertExpr(Uint32 offset, AstRef<AstExpr> operand, AstRef<AstType> type)
		: AstExpr{offset, KIND}
		, operand{operand}
		, type{type}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	AstRef<AstType> type; // Optional
};

// Represents a type expression, e.g:
//	The same as an Type but usable in an Expr context.
struct AstTypeExpr : AstExpr {
	static constexpr const auto KIND = Kind::TYPE;
	constexpr AstTypeExpr(Uint32 offset, AstRef<AstType> type)
		: AstExpr{offset, KIND}
		, type{type}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> type;
};

// The reference Odin compiler treats all types as expressions in the ast. Here,
// we prefer a more typed ast where types are their own category.
//
// There are a few places in the Odin language where an expression evaluates to
// a type, but these expressions are regular and can be separately described by
// their own grammar independent from an expression.
//
// TypeIDType   := 'typeid'
// StructType   := 'struct' '{' (Field (',' Field)*)? '}'
// UnionType    := 'union' '{' (Type (',' Type)*)? '}'
// EnumType     := 'enum' '{' (EnumValue (',' EnumValue)*)? '}'
// EnumValue    := Ident ('=' Expr)?
// ProcType     := 'proc' StrLit? '(' (Field (',' Field)*)? ')' ('->' (Type | (Field (',' Field)+))
// PtrType      := '^' Type
// MultiPtrType := '[^]' Type
// SliceType    := '[]' Type
// ArrayType    := '[' (Expr | '?') ']' Type
// DynArrayType := '[' 'dynamic' ']' Type
// MapType      := 'map' '[' Type ']' Type
// MatrixType   := 'matrix' '[' Expr ',' Expr ']' Type
// NamedType    := Ident ('.' Ident)?
// ParamType    := NamedType '(' (Expr (',' Expr)*)? ')'
// ParenType    := '(' Type ')'
// DistinctType := 'distinct' Type
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
		TYPEID,    // typeid
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
		BITSET,    // bit_set
		NAMED,     // Name
		PARAM,     // T(args)
		PAREN,     // (T)
		DISTINCT,  // distinct T
	};
	constexpr AstType(Uint32 offset, Kind kind)
		: AstNode{offset}
		, kind{kind}
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

struct AstTypeIDType : AstType {
	static constexpr const auto KIND = Kind::TYPEID;
	constexpr AstTypeIDType(Uint32 offset)
		: AstType{offset, KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
};

struct AstUnionType : AstType {
	static constexpr const auto KIND = Kind::UNION;
	constexpr AstUnionType(Uint32 offset, AstRefArray<AstType> types)
		: AstType{offset, KIND}
		, types{types}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRefArray<AstType> types;
};

struct AstEnumType : AstType {
	static constexpr const auto KIND = Kind::ENUM;
	constexpr AstEnumType(Uint32 offset, AstRef<AstType> base, AstRefArray<AstField> enums)
		: AstType{offset, KIND}
		, base{base}
		, enums{enums}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType>       base;
	AstRefArray<AstField> enums;
};

struct AstProcType : AstType {
	static constexpr const auto KIND = Kind::PROC;
	void dump(const AstFile& ast, StringBuilder& builder) const;
	// TODO(dweiler): FieldList args
	// TODO(dweiler): FieldList rets
};

struct AstPtrType : AstType {
	static constexpr const auto KIND = Kind::PTR;
	constexpr AstPtrType(Uint32 offset, AstRef<AstType> base)
		: AstType{offset, KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstMultiPtrType : AstType {
	static constexpr const auto KIND = Kind::MULTIPTR;
	constexpr AstMultiPtrType(Uint32 offset, AstRef<AstType> base)
		: AstType{offset, KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstSliceType : AstType {
	static constexpr const auto KIND = Kind::SLICE;
	constexpr AstSliceType(Uint32 offset, AstRef<AstType> base)
		: AstType{offset, KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstArrayType : AstType {
	static constexpr const auto KIND = Kind::ARRAY;
	constexpr AstArrayType(Uint32 offset, AstRef<AstExpr> size, AstRef<AstType> base)
		: AstType{offset, KIND}
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
	constexpr AstDynArrayType(Uint32 offset, AstRef<AstType> base)
		: AstType{offset, KIND}
		, base{base}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> base;
};

struct AstMapType : AstType {
	static constexpr const auto KIND = Kind::MAP;
	constexpr AstMapType(Uint32 offset, AstRef<AstType> kt, AstRef<AstType> vt)
		: AstType{offset, KIND}
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
	constexpr AstMatrixType(Uint32 offset, AstRef<AstExpr> rows, AstRef<AstExpr> cols, AstRef<AstType> base)
		: AstType{offset, KIND}
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

struct AstBitsetType : AstType {
	static constexpr const auto KIND = Kind::BITSET;
	constexpr AstBitsetType(Uint32 offset, AstRef<AstExpr> expr, AstRef<AstType> type)
		: AstType{offset, KIND}
		, expr{expr}
		, type{type}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstExpr> expr;
	AstRef<AstType> type; // Optional
};

struct AstNamedType : AstType {
	static constexpr const auto KIND = Kind::NAMED;
	constexpr AstNamedType(Uint32 offset, AstStringRef pkg, AstStringRef name)
		: AstType{offset, KIND}
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
	constexpr AstParamType(Uint32 offset, AstRef<AstNamedType> name, AstRefArray<AstExpr> exprs)
		: AstType{offset, KIND}
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
	constexpr AstParenType(Uint32 offset, AstRef<AstType> type)
		: AstType{offset, KIND}
		, type{type}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder) const;
	AstRef<AstType> type;
};

struct AstDistinctType : AstType {
	static constexpr const auto KIND = Kind::DISTINCT;
	constexpr AstDistinctType(Uint32 offset, AstRef<AstType> type)
		: AstType{offset, KIND}
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

	constexpr AstStmt(Uint32 offset, Kind kind)
		: AstNode{offset}
		, kind{kind}
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
	constexpr AstEmptyStmt(Uint32 offset)
		: AstStmt{offset, KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstExprStmt : AstStmt {
	static constexpr const auto KIND = Kind::EXPR;
	constexpr AstExprStmt(Uint32 offset, AstRef<AstExpr> expr)
		: AstStmt{offset, KIND}
		, expr{expr}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstExpr> expr;
};

struct AstAssignStmt : AstStmt {
	static constexpr const auto KIND = Kind::ASSIGN;
	constexpr AstAssignStmt(Uint32               offset,
	                        AstRefArray<AstExpr> lhs,
	                        AstRefArray<AstExpr> rhs,
	                        AssignKind           kind)
		: AstStmt{offset, KIND}
		, lhs{lhs}
		, rhs{rhs}
		, kind{kind}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRefArray<AstExpr> lhs;
	AstRefArray<AstExpr> rhs;
	AssignKind           kind;
};

struct AstBlockStmt : AstStmt {
	static constexpr const auto KIND = Kind::BLOCK;
	constexpr AstBlockStmt(Uint32 offset, AstRefArray<AstStmt> stmts)
		: AstStmt{offset, KIND}
		, stmts{stmts}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRefArray<AstStmt> stmts;
};

struct AstImportStmt : AstStmt {
	static constexpr const auto KIND = Kind::IMPORT;
	constexpr AstImportStmt(Uint32 offset, AstStringRef alias, AstRef<AstStringExpr> expr)
		: AstStmt{offset, KIND}
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
	constexpr AstPackageStmt(Uint32 offset, AstStringRef name)
		: AstStmt{offset, KIND}
		, name{name}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef name;
};

struct AstDeferStmt : AstStmt {
	static constexpr const auto KIND = Kind::DEFER;
	constexpr AstDeferStmt(Uint32 offset, AstRef<AstStmt> stmt)
		: AstStmt{offset, KIND}
		, stmt{stmt}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstStmt> stmt;
};

struct AstReturnStmt : AstStmt {
	static constexpr const auto KIND = Kind::RETURN;
	constexpr AstReturnStmt(Uint32 offset, AstRefArray<AstExpr> exprs)
		: AstStmt{offset, KIND}
		, exprs{exprs}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstRefArray<AstExpr> exprs;
};

struct AstBreakStmt : AstStmt {
	static constexpr const auto KIND = Kind::BREAK;
	constexpr AstBreakStmt(Uint32 offset, AstStringRef label)
		: AstStmt{offset, KIND}
		, label{label}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef label;
};

struct AstContinueStmt : AstStmt {
	static constexpr const auto KIND = Kind::CONTINUE;
	constexpr AstContinueStmt(Uint32 offset, AstStringRef label)
		: AstStmt{offset, KIND}
		, label{label}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	AstStringRef label;
};

struct AstFallthroughStmt : AstStmt {
	static constexpr const auto KIND = Kind::FALLTHROUGH;
	constexpr AstFallthroughStmt(Uint32 offset)
		: AstStmt{offset, KIND}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstForeignImportStmt : AstStmt {
	static constexpr const auto KIND = Kind::FOREIGNIMPORT;
	constexpr AstForeignImportStmt(Uint32 offset, AstStringRef ident, AstRefArray<AstExpr> names)
		: AstStmt{offset, KIND}
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
	constexpr AstIfStmt(Uint32          offset,
	                    AstRef<AstStmt> init,
	                    AstRef<AstExpr> cond,
	                    AstRef<AstStmt> on_true,
	                    AstRef<AstStmt> on_false)
		: AstStmt{offset, KIND}
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
	constexpr AstWhenStmt(Uint32               offset,
	                      AstRef<AstExpr>      cond,
	                      AstRef<AstBlockStmt> on_true,
	                      AstRef<AstBlockStmt> on_false)
		: AstStmt{offset, KIND}
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
	constexpr AstDeclStmt(Uint32                    offset,
	                      Bool                      is_const,
	                      Bool                      is_using,
	                      List                      lhs,
	                      AstRef<AstType>           type,
	                      List                      rhs,
	                      AstRefArray<AstDirective> directives,
	                      AstRefArray<AstAttribute> attributes)
		: AstStmt{offset, KIND}
		, is_const{is_const}
		, is_using{is_using}
		, lhs{move(lhs)}
		, type{type}
		, rhs{move(rhs)}
		, directives{directives}
		, attributes{attributes}
	{
	}
	void dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const;
	Bool                      is_const;
	Bool                      is_using;
	List                      lhs;
	AstRef<AstType>           type;
	List                      rhs; // Optional
	AstRefArray<AstDirective> directives; // Optional
	AstRefArray<AstAttribute> attributes; // Optional
};

// Represents a procedure-level using stmt, e.g:
// 	using fmt
//
// Not to be confused with using on a DeclStmt which is just indicated by the
// DeclStmt::is_using flag.
struct AstUsingStmt : AstStmt {
	static constexpr const auto KIND = Kind::USING;
	constexpr AstUsingStmt(Uint32 offset, AstRef<AstExpr> expr)
		: AstStmt{offset, KIND}
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
static_assert(!is_polymorphic<AstIfExpr>, "Cannot be polymorphic");
static_assert(!is_polymorphic<AstWhenExpr>, "Cannot be polymorphic");
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
	static Maybe<AstFile> create(System& sys, StringView filename);
	static Maybe<AstFile> load(System& sys, Stream& stream);

	Bool save(Stream& stream) const;

	StringView filename() const {
		return string_table_[filename_];
	}

	AstFile(AstFile&&) = default;
	~AstFile();

	static inline constexpr const auto MAX = AstID::MAX * AstNode::MAX;

	template<typename T, typename... Ts>
	AstRef<T> create(Ts&&... args) {
		auto slab_idx = AstSlabID::id<T>(sys_);
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

	AstFile(System& sys, StringTable&& string_table, AstStringRef filename)
		: sys_{sys}
		, string_table_{move(string_table)}
		, filename_{filename}
		, slabs_{sys.allocator}
		, ids_{sys.allocator}
	{
	}

	AstFile(System& sys, StringTable&& string_table, AstStringRef filename, Array<Maybe<Slab>>&& slabs, Array<AstID>&& ids)
		: sys_{sys}
		, string_table_{move(string_table)}
		, filename_{filename}
		, slabs_{move(slabs)}
		, ids_{move(ids)}
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
	System&            sys_;
	StringTable        string_table_;
	AstStringRef       filename_;
	Array<Maybe<Slab>> slabs_;
	Array<AstID>       ids_;
};

} // namespace Thor

#endif // THOR_AST_H

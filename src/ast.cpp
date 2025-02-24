#include "util/allocator.h"
#include "util/file.h"
#include "util/stream.h"

#include "ast.h"

#include <stdlib.h>
#include <string.h> // memcpy

namespace Thor {

struct AstFileHeader {
	Uint8  magic[4]; // tast
	Uint32 version;
	Uint64 slabs;
};

Maybe<AstFile> AstFile::create(System& sys, StringView filename) {
	StringTable table{sys.allocator};
	auto ref = table.insert(filename);
	if (!ref) {
		return {};
	}
	return AstFile { sys, move(table), ref };
}

Maybe<AstFile> AstFile::load(System& sys, Stream& stream) {
	AstFileHeader header;
	if (!stream.read(Slice{&header, 1}.cast<Uint8>())) {
		return {};
	}
	if (header.magic != Slice{"tast"}.cast<const Uint8>()) {
		return {};
	}
	if (header.version != 1) {
		return {};
	}
	auto string_table = StringTable::load(sys.allocator, stream);
	if (!string_table) {
		return {};
	}
	// Read in the filename AstStringRef which is just Uint32[2]
	AstStringRef filename;
	if (!stream.read(Slice{&filename, 1}.cast<Uint8>())) {
		return {};
	}
	// Count the # of slabs indicated by the bitset.
	Ulen n_slabs = 0;
	for (Uint64 i = 0; i < 64; i++) {
		if ((header.slabs & (1_u64 << Uint64(i))) != 0) {
			n_slabs++;
		}
	}
	Array<Maybe<Slab>> slabs{sys.allocator};
	if (!slabs.resize(n_slabs)) {
		return {};
	}
	for (Ulen i = 0; i < n_slabs; i++) {
		if ((header.slabs & (1_u64 << Uint64(i))) != 0) {
			if (!slabs[i]->load(sys.allocator, stream)) {
				return {};
			}
		}
	}
	// Read in the IDs
	Array<AstID> ids{sys.allocator};
	// Read the AstID list in.
	return AstFile {
		sys,
		move(*string_table),
		filename,
		move(slabs),
		move(ids)
	};
}

Bool AstFile::save(Stream& stream) const {
	AstFileHeader header {
		.magic   = { 't', 'a', 's', 't' },
		.version = 1,
		.slabs   = 0
	};
	// Determine which slabs are in-use. There is only 64 possible slab types
	// due to a 6-bit AstSlabID. We can store the occupancy in one 64-bit word.
	Ulen i = 0;
	for (const auto& slab : slabs_) {
		if (slab) {
			header.slabs |= 1_u64 << Uint64(i);
		}
		i++;
	}
	auto src = Slice{&header, 1}.cast<const Uint8>();
	if (!stream.write(src)) {
		return false;
	}
	if (!string_table_.save(stream)) {
		return false;
	}
	for (const auto& slab : slabs_) {
		if (slab && !slab->save(stream)) {
			return false;
		}
	}
	return true;
}

AstFile::~AstFile() {
	// TODO(dweiler): Call destructors on nodes
}

AstIDArray AstFile::insert(Slice<const AstID> ids) {
	const auto offset = ids_.length();
	if (!ids_.resize(offset + ids.length())) {
		return {};
	}
	memcpy(ids_.data() + offset, ids.data(), ids.length() * sizeof(AstID));
	return AstIDArray { Uint64(offset), Uint64(ids.length()) };
}

// Stmt
void AstStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	using enum Kind;
	switch (kind) {
	case EMPTY:         return to_stmt<const AstEmptyStmt>()->dump(ast, builder, nest);
	case EXPR:          return to_stmt<const AstExprStmt>()->dump(ast, builder, nest);
	case ASSIGN:        return to_stmt<const AstAssignStmt>()->dump(ast, builder, nest);
	case BLOCK:         return to_stmt<const AstBlockStmt>()->dump(ast, builder, nest);
	case IMPORT:        return to_stmt<const AstImportStmt>()->dump(ast, builder, nest);
	case PACKAGE:       return to_stmt<const AstPackageStmt>()->dump(ast, builder, nest);
	case DEFER:         return to_stmt<const AstDeferStmt>()->dump(ast, builder, nest);
	case RETURN:        return to_stmt<const AstReturnStmt>()->dump(ast, builder, nest);
	case BREAK:         return to_stmt<const AstBreakStmt>()->dump(ast, builder, nest);
	case CONTINUE:      return to_stmt<const AstContinueStmt>()->dump(ast, builder, nest);
	case FALLTHROUGH:   return to_stmt<const AstFallthroughStmt>()->dump(ast, builder, nest);
	case FOREIGNIMPORT: return to_stmt<const AstForeignImportStmt>()->dump(ast, builder, nest);
	case IF:            return to_stmt<const AstIfStmt>()->dump(ast, builder, nest);
	case WHEN:          return to_stmt<const AstWhenStmt>()->dump(ast, builder, nest);
	case DECL:          return to_stmt<const AstDeclStmt>()->dump(ast, builder, nest);
	case USING:         return to_stmt<const AstUsingStmt>()->dump(ast, builder, nest);
	}
}

void AstEmptyStmt::dump(const AstFile&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	// builder.put(';');
}

void AstExprStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	ast[expr].dump(ast, builder);
	builder.put(';');
	builder.put('\n');
}

void AstAssignStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	static constexpr const StringView OP[] = {
		#define ASSIGN(ENUM, NAME, MATCH) MATCH,
		#include "lexer.inl"
	};

	builder.rep(nest * 2, ' ');
	Bool first = true;
	for (auto value : ast[lhs]) {
		if (!first) {
			builder.put(", ");
		}
		ast[value].dump(ast, builder);
		first = false;
	}

	builder.put(' ');
	builder.put(OP[Uint32(kind)]);
	builder.put(' ');

	first = true;
	for (auto value : ast[rhs]) {
		if (!first) {
			builder.put(", ");
		}
		ast[value].dump(ast, builder);
		first = false;
	}

	builder.put(';');
	builder.put('\n');
}

void AstBlockStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put('{');
	builder.put('\n');
	for (auto stmt : ast[stmts]) {
		ast[stmt].dump(ast, builder, nest+1);
		builder.put('\n');
	}
	builder.rep(nest * 2, ' ');
	builder.put('}');
}

void AstImportStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("import");
	builder.put(' ');
	if (alias) {
		builder.put(ast[alias]);
		builder.put(' ');
	}
	ast[expr].dump(ast, builder);
	builder.put(';');
	builder.put('\n');
}

void AstPackageStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("package");
	builder.put(' ');
	builder.put(ast[name]);
	builder.put(';');
	builder.put('\n');
}

void AstDeferStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("defer");
	builder.put(' ');
	const auto& defer = ast[stmt];
	if (defer.is_stmt<AstBlockStmt>()) {
		builder.put('\n');
		defer.dump(ast, builder, nest);
	} else {
		defer.dump(ast, builder, 0);
	}
}

void AstReturnStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("return");
	builder.put(' ');
	Bool first = true;
	for (auto expr : ast[exprs]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[expr].dump(ast, builder);
		first = false;
	}
}

void AstBreakStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("break");
	if (label) {
		builder.put(' ');
		builder.put(ast[label]);
	}
	builder.put(';');
}

void AstContinueStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("continue");
	if (label) {
		builder.put(' ');
		builder.put(ast[label]);
	}
	builder.put(';');
}

void AstFallthroughStmt::dump(const AstFile&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("fallthrough");
	builder.put(';');
}

void AstForeignImportStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("foreign import");
	builder.put(' ');
	if (ident) {
		builder.put(ast[ident]);
		builder.put(' ');
	}
	if (names.length() == 1) {
		ast[ast[names][0]].dump(ast, builder);
	} else {
		builder.put('{');
		builder.put('\n');
		Bool first = true;
		for (auto expr : ast[names]) {
			if (!first) {
				builder.put(',');
				builder.put('\n');
			}
			builder.rep((nest + 1) * 2, ' ');
			ast[expr].dump(ast, builder);
			first = false;
		}
		builder.put('\n');
		builder.put('}');
	}
	builder.put('\n');
}

void AstIfStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("if");
	if (init) {
		ast[init].dump(ast, builder, nest);
		builder.put(' ');
		builder.put(';');
	}
	ast[cond].dump(ast, builder);
	ast[on_true].dump(ast, builder, nest);
	if (on_false) {
		builder.put("else");
		ast[on_false].dump(ast, builder, 0);
	}
}

void AstWhenStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("when");
	ast[cond].dump(ast, builder);
	ast[on_true].dump(ast, builder, nest+1);
	if (on_false) {
		builder.put("else");
		ast[on_false].dump(ast, builder, 0);
	}
}

void AstDeclStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	Bool first = true;
	builder.rep(nest * 2, ' ');
	for (auto value : ast[lhs]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[value].dump(ast, builder);
		first = false;
	}
	if (type) {
		builder.put(':');
		builder.put(' ');
		ast[type].dump(ast, builder);
		builder.put(' ');
	} else {
		builder.put(' ');
		builder.put(':');
	}
	if (!rhs.is_empty()) {
		if (is_const) {
			builder.put(':');
		} else {
			builder.put('=');
		}
		builder.put(' ');
		Bool first = true;
		for (auto value : ast[rhs]) {
			if (!first) {
				builder.put(',');
				builder.put(' ');
			}
			ast[value].dump(ast, builder);
			first = false;
		}
	}
	builder.put('\n');
}

void AstUsingStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("using");
	builder.put(' ');
	ast[expr].dump(ast, builder);
}

// Expr
void AstExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	using enum Kind;
	switch (kind) {
	case BIN:         return to_expr<const AstBinExpr>()->dump(ast, builder);
	case UNARY:       return to_expr<const AstUnaryExpr>()->dump(ast, builder);
	case IF:          return to_expr<const AstIfExpr>()->dump(ast, builder);
	case WHEN:        return to_expr<const AstWhenExpr>()->dump(ast, builder);
	case DEREF:       return to_expr<const AstDerefExpr>()->dump(ast, builder);
	case OR_RETURN:   return to_expr<const AstOrReturnExpr>()->dump(ast, builder);
	case OR_BREAK:    return to_expr<const AstOrBreakExpr>()->dump(ast, builder);
	case OR_CONTINUE: return to_expr<const AstOrContinueExpr>()->dump(ast, builder);
	case CALL:        return to_expr<const AstCallExpr>()->dump(ast, builder);
	case IDENT:       return to_expr<const AstIdentExpr>()->dump(ast, builder);
	case UNDEF:       return to_expr<const AstUndefExpr>()->dump(ast, builder);
	case CONTEXT:     return to_expr<const AstContextExpr>()->dump(ast, builder);
	case PROC:        return to_expr<const AstProcExpr>()->dump(ast, builder);
	case SLICE:       return to_expr<const AstSliceExpr>()->dump(ast, builder);
	case INDEX:       return to_expr<const AstIndexExpr>()->dump(ast, builder);
	case INT:         return to_expr<const AstIntExpr>()->dump(ast, builder);
	case FLOAT:       return to_expr<const AstFloatExpr>()->dump(ast, builder);
	case STRING:      return to_expr<const AstStringExpr>()->dump(ast, builder);
	case IMAGINARY:   return to_expr<const AstImaginaryExpr>()->dump(ast, builder);
	case COMPOUND:    return to_expr<const AstCompoundExpr>()->dump(ast, builder);
	case CAST:        return to_expr<const AstCastExpr>()->dump(ast, builder);
	case SELECTOR:    return to_expr<const AstSelectorExpr>()->dump(ast, builder);
	case ACCESS:      return to_expr<const AstAccessExpr>()->dump(ast, builder);
	case ASSERT:      return to_expr<const AstAssertExpr>()->dump(ast, builder);
	case TYPE:        return to_expr<const AstTypeExpr>()->dump(ast, builder);
	}
}

void AstBinExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('(');
	static constexpr const StringView OP[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) MATCH,
		#include "lexer.inl"
	};
	ast[lhs].dump(ast, builder);
	builder.put(' ');
	builder.put(OP[Uint32(op)]);
	builder.put(' ');
	ast[rhs].dump(ast, builder);
	builder.put(')');
}

void AstUnaryExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('(');
	static constexpr const StringView OP[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) MATCH,
		#include "lexer.inl"
	};
	builder.put(OP[Uint32(op)]);
	ast[operand].dump(ast, builder);
	builder.put(')');
}

void AstIfExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[on_true].dump(ast, builder);
	builder.put(' ');
	builder.put("if");
	builder.put(' ');
	ast[cond].dump(ast, builder);
	builder.put(' ');
	builder.put("else");
	builder.put(' ');
	ast[on_false].dump(ast, builder);
}

void AstWhenExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[on_true].dump(ast, builder);
	builder.put(' ');
	builder.put("when");
	builder.put(' ');
	ast[cond].dump(ast, builder);
	builder.put(' ');
	builder.put("else");
	builder.put(' ');
	ast[on_false].dump(ast, builder);
}

void AstDerefExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put('^');
}

void AstOrReturnExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put(' ');
	builder.put("or_return");
}

void AstOrBreakExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put(' ');
	builder.put("or_break");
}

void AstOrContinueExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put(' ');
	builder.put("or_continue");
}

void AstCallExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put('(');
	Bool first = true;
	for (auto arg : ast[args]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[arg].dump(ast, builder);
		first = false;
	}
	builder.put(')');
}

void AstIdentExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put(ast[ident]);
}

void AstUndefExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put("---");
}

void AstContextExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put("context");
}

void AstProcExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[type].dump(ast, builder);
	ast[body].dump(ast, builder, 0);
}

void AstSliceExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put('[');
	if (lhs) {
		ast[lhs].dump(ast, builder);
	}
	builder.put(':');
	if (rhs) {
		ast[rhs].dump(ast, builder);
	}
	builder.put(']');
}

void AstIndexExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put('[');
	ast[lhs].dump(ast, builder);
	if (rhs) {
		builder.put(',');
		builder.put(' ');
		ast[rhs].dump(ast, builder);
	}
	builder.put(']');
}

void AstIntExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put(value);
}

void AstFloatExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put(value);
}

void AstStringExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('"');
	builder.put(ast[value]);
	builder.put('"');
}

void AstImaginaryExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put(value);
	builder.put('i');
}

void AstCompoundExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('{');
	Bool first = true;
	for (auto field : ast[fields]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[field].dump(ast, builder);
		first = false;
	}
	builder.put('}');
}

void AstCastExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	if (type) {
		builder.put('(');
		ast[type].dump(ast, builder);
		builder.put(')');
		builder.put('(');
		ast[expr].dump(ast, builder);
		builder.put(')');
	} else {
		builder.put("auto_cast");
		ast[expr].dump(ast, builder);
	}
}

void AstSelectorExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('.');
	builder.put(ast[name]);
}

void AstAccessExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	if (is_arrow) {
		builder.put("->");
	} else {
		builder.put('.');
	}
	builder.put(ast[field]);
}

void AstAssertExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	builder.put('.');
	if (type) {
		builder.put('(');
		ast[type].dump(ast, builder);
		builder.put(')');
	} else {
		builder.put('?');
	}
}

void AstTypeExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[type].dump(ast, builder);
}

// Type
void AstType::dump(const AstFile& ast, StringBuilder& builder) const {
	using enum Kind;
	switch (kind) {
	case TYPEID:   return to_type<const AstTypeIDType>()->dump(ast, builder);
	case UNION:    return to_type<const AstUnionType>()->dump(ast, builder);
	case STRUCT:   return to_type<const AstStructType>()->dump(ast, builder);
	case ENUM:     return to_type<const AstEnumType>()->dump(ast, builder);
	case PROC:     /* TODO */ break;
	case PTR:      return to_type<const AstPtrType>()->dump(ast, builder);
	case MULTIPTR: return to_type<const AstMultiPtrType>()->dump(ast, builder);
	case SLICE:    return to_type<const AstSliceType>()->dump(ast, builder);
	case ARRAY:    return to_type<const AstArrayType>()->dump(ast, builder);
	case DYNARRAY: return to_type<const AstDynArrayType>()->dump(ast, builder);
	case MAP:      return to_type<const AstMapType>()->dump(ast, builder);
	case MATRIX:   return to_type<const AstMatrixType>()->dump(ast, builder);
	case BITSET:   return to_type<const AstBitsetType>()->dump(ast, builder);
	case NAMED:    return to_type<const AstNamedType>()->dump(ast, builder);
	case PARAM:    return to_type<const AstParamType>()->dump(ast, builder);
	case PAREN:    return to_type<const AstParenType>()->dump(ast, builder);
	case DISTINCT: return to_type<const AstDistinctType>()->dump(ast, builder);
	}
}

void AstTypeIDType::dump(const AstFile&, StringBuilder& builder) const {
	builder.put("typeid");
}

void AstUnionType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("union");
	builder.put(' ');
	builder.put('{');
	builder.put(' ');
	Bool first = true;
	for (const auto type : ast[types]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[type].dump(ast, builder);
		first = false;
	}
	builder.put(' ');
	builder.put('}');
}

void AstStructType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("struct");
	builder.put(' ');
	builder.put('{');
	builder.put(' ');
	Bool first = true;
	for (const auto decl : ast[decls]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[decl].dump(ast, builder, 0);
		first = false;
	}
	builder.put(' ');
	builder.put('}');
}

void AstEnumType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("enum");
	builder.put(' ');
	if (base) {
		ast[base].dump(ast, builder);
		builder.put(' ');
	}
	builder.put('{');
	Bool first = true;
	for (const auto e : ast[enums]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[e].dump(ast, builder);
		first = false;
	}
	builder.put('}');
}

void AstProcType::dump(const AstFile& ast, StringBuilder& builder) const {
	// TODO(dweiler):
	(void)ast;
	(void)builder;
}

void AstPtrType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('^');
	ast[base].dump(ast, builder);
}

void AstMultiPtrType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("[^]");
	ast[base].dump(ast, builder);
}

void AstSliceType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("[]");
	ast[base].dump(ast, builder);
}

void AstArrayType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('[');
	if (size) {
		ast[size].dump(ast, builder);
	} else {
		builder.put('?');
	}
	builder.put(']');
	ast[base].dump(ast, builder);
}

void AstDynArrayType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('[');
	builder.put("dynamic");
	builder.put(']');
	ast[base].dump(ast, builder);
}

void AstMapType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("map");
	builder.put('[');
	ast[kt].dump(ast, builder);
	builder.put(']');
	ast[vt].dump(ast, builder);
}

void AstMatrixType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("matrix");
	builder.put('[');
	ast[rows].dump(ast, builder);
	builder.put(',');
	builder.put(' ');
	ast[cols].dump(ast, builder);
	builder.put(']');
	ast[base].dump(ast, builder);
}

void AstBitsetType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("bit_set");
	builder.put('[');
	ast[expr].dump(ast, builder);
	if (type) {
		builder.put(';');
		builder.put(' ');
		ast[type].dump(ast, builder);
	}
	builder.put(']');
}

void AstNamedType::dump(const AstFile& ast, StringBuilder& builder) const {
	if (pkg) {
		builder.put(ast[pkg]);
		builder.put('.');
	}
	builder.put(ast[name]);
}

void AstParamType::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[name].dump(ast, builder);
	builder.put('(');
	Bool first = true;
	for (const auto expr : ast[exprs]) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[expr].dump(ast, builder);
		first = false;
	}
	builder.put(')');
}

void AstParenType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('(');
	ast[type].dump(ast, builder);
	builder.put(')');
}

void AstDistinctType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("distinct");
	builder.put(' ');
	ast[type].dump(ast, builder);
}

// Field
void AstField::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[operand].dump(ast, builder);
	if (expr) {
		builder.put('=');
		ast[expr].dump(ast, builder);
	}
}

// Directive
void AstDirective::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('#');
	builder.put(ast[name]);
	if (!args.is_empty()) {
		builder.put('(');
		Bool first = true;
		for (auto arg : ast[args]) {
			if (!first) {
				builder.put(',');
				builder.put(' ');
			}
			ast[arg].dump(ast, builder);
			first = false;
		}
		builder.put(')');
	}
}

} // namespace Thor

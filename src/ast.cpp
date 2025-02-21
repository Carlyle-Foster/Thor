#include "util/allocator.h"
#include "util/file.h"

#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

namespace Thor {

Maybe<AstFile> AstFile::create(Allocator& allocator, StringView filename) {
	StringTable table{allocator};
	auto ref = table.insert(filename);
	if (!ref) {
		return {};
	}
	return AstFile { move(table), allocator, ref };
}

AstFile::~AstFile() {
	// TODO(dweiler): Call destructors on nodes
}

// Stmt
void AstStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	using enum Kind;
	switch (kind) {
	case EMPTY:       return to_stmt<const AstEmptyStmt>()->dump(ast, builder, nest);
	case EXPR:        return to_stmt<const AstExprStmt>()->dump(ast, builder, nest);
	case ASSIGN:      return to_stmt<const AstAssignStmt>()->dump(ast, builder, nest);
	case BLOCK:       return to_stmt<const AstBlockStmt>()->dump(ast, builder, nest);
	case IMPORT:      return to_stmt<const AstImportStmt>()->dump(ast, builder, nest);
	case PACKAGE:     return to_stmt<const AstPackageStmt>()->dump(ast, builder, nest);
	case DEFER:       return to_stmt<const AstDeferStmt>()->dump(ast, builder, nest);
	case BREAK:       return to_stmt<const AstBreakStmt>()->dump(ast, builder, nest);
	case CONTINUE:    return to_stmt<const AstContinueStmt>()->dump(ast, builder, nest);
	case FALLTHROUGH: return to_stmt<const AstFallthroughStmt>()->dump(ast, builder, nest);
	case IF:          return to_stmt<const AstIfStmt>()->dump(ast, builder, nest);
	case DECL:        return to_stmt<const AstDeclStmt>()->dump(ast, builder, nest);
	}
}
void AstEmptyStmt::dump(const AstFile&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put(';');
}

void AstExprStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	ast[expr].dump(ast, builder);
	builder.put(';');
}

void AstBlockStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put('{');
	builder.put('\n');
	for (auto stmt : stmts) {
		ast[stmt].dump(ast, builder, nest+1);
		builder.put('\n');
	}
	builder.rep(nest * 2, ' ');
	builder.put('}');
}

void AstPackageStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("package");
	builder.put(' ');
	builder.put(ast[name]);
	builder.put(';');
	builder.put('\n');
}

void AstImportStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("import");
	builder.put(' ');
	builder.put(ast[path]);
	builder.put(';');
	builder.put('\n');
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

void AstAssignStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	static constexpr const StringView OP[] = {
		#define ASSIGN(ENUM, NAME, MATCH) MATCH,
		#include "lexer.inl"
	};

	builder.rep(nest * 2, ' ');
	Bool first = true;
	for (auto value : lhs) {
		if (!first) {
			builder.put(", ");
		}
		ast[value].dump(ast, builder);
		first = false;
	}

	builder.put(' ');
	builder.put(OP[Uint32(token.as_assign)]);
	builder.put(' ');

	first = true;
	for (auto value : rhs) {
		if (!first) {
			builder.put(", ");
		}
		ast[value].dump(ast, builder);
		first = false;
	}

	builder.put(';');
}

void AstIfStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("if");
	if (init) {
		ast[*init].dump(ast, builder, nest);
		builder.put(' ');
		builder.put(';');
	}
	ast[cond].dump(ast, builder);
	ast[on_true].dump(ast, builder, nest);
	if (on_false) {
		builder.put("else");
		ast[*on_false].dump(ast, builder, nest);
	}
}

void AstExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	using enum Kind;
	switch (kind) {
	case BIN:     return to_expr<const AstBinExpr>()->dump(ast, builder);
	case UNARY:   return to_expr<const AstUnaryExpr>()->dump(ast, builder);
	case TERNARY: return to_expr<const AstTernaryExpr>()->dump(ast, builder);
	case IDENT:   return to_expr<const AstIdentExpr>()->dump(ast, builder);
	case UNDEF:   return to_expr<const AstUndefExpr>()->dump(ast, builder);
	case CONTEXT: return to_expr<const AstContextExpr>()->dump(ast, builder);
	case STRUCT:  return to_expr<const AstStructExpr>()->dump(ast, builder);
	case TYPE:    return to_expr<const AstTypeExpr>()->dump(ast, builder);
	case PROC:    return to_expr<const AstProcExpr>()->dump(ast, builder);
	case INTEGER: return to_expr<const AstIntExpr>()->dump(ast, builder);
	case FLOAT:   return to_expr<const AstFloatExpr>()->dump(ast, builder);
	}
}

void AstProcExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("proc");
	builder.put('(');
	if (params) {
		for (auto param : *params) {
			ast[param].dump(ast, builder, 0);
			builder.put(',');
			builder.put(' ');
		}
	}
	builder.put(')');
	builder.put(' ');
	builder.put("->");
	builder.put(' ');
	ast[ret].dump(ast, builder);
	ast[body].dump(ast, builder, 0);
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

void AstDeclStmt::dump(const AstFile& ast, StringBuilder& builder, Ulen nest) const {
	Bool first = true;
	builder.rep(nest * 2, ' ');
	for (auto value : lhs) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[value].dump(ast, builder);
		first = false;
	}
	builder.put(':');
	ast[type].dump(ast, builder);
	if (rhs) {
		builder.put(':');
		Bool first = true;
		for (auto value : *rhs) {
			if (!first) {
				builder.put(',');
			}
			ast[value].dump(ast, builder);
			first = false;
		}
	}
}

// Expr
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

void AstTernaryExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put('(');
	ast[cond].dump(ast, builder);
	builder.put(' ');
	builder.put('?');
	builder.put(' ');
	ast[on_true].dump(ast, builder);
	builder.put(' ');
	builder.put(':');
	builder.put(' ');
	ast[on_false].dump(ast, builder);
	builder.put(')');
}

void AstIdentExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put(ast[ident]);
}

void AstIntExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put(value);
}

void AstFloatExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put(value);
}

void AstUndefExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put("---");
}

void AstContextExpr::dump(const AstFile&, StringBuilder& builder) const {
	builder.put("context");
}

void AstStructExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("struct");
	builder.put(' ');
	builder.put('{');
	builder.put('\n');
	for (auto decl : decls) {
		ast[decl].dump(ast, builder, 0);
		builder.put(',');
		builder.put('\n');
	}
	builder.put('}');
	builder.put('\n');
}

void AstTypeExpr::dump(const AstFile& ast, StringBuilder& builder) const {
	if (expr) {
		ast[expr].dump(ast, builder);
	}
}

// Type
void AstType::dump(const AstFile& ast, StringBuilder& builder) const {
	using enum Kind;
	switch (kind) {
	case UNION:    return to_type<const AstUnionType>()->dump(ast, builder);
	case PTR:      return to_type<const AstPtrType>()->dump(ast, builder);
	case MULTIPTR: return to_type<const AstMultiPtrType>()->dump(ast, builder);
	case SLICE:    return to_type<const AstSliceType>()->dump(ast, builder);
	case ARRAY:    return to_type<const AstArrayType>()->dump(ast, builder);
	case NAMED:    return to_type<const AstNamedType>()->dump(ast, builder);
	case PARAM:    return to_type<const AstParamType>()->dump(ast, builder);
	default:
		break;
	}
}

void AstUnionType::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("union");
	builder.put(' ');
	builder.put('{');
	builder.put(' ');
	Bool first = true;
	for (const auto type : types) {
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

void AstParamType::dump(const AstFile& ast, StringBuilder& builder) const {
	ast[name].dump(ast, builder);
	builder.put('(');
	Bool first = true;
	for (const auto expr : exprs) {
		if (!first) {
			builder.put(',');
			builder.put(' ');
		}
		ast[expr].dump(ast, builder);
		first = false;
	}
	builder.put(')');
}

void AstNamedType::dump(const AstFile& ast, StringBuilder& builder) const {
	if (pkg) {
		builder.put(ast[pkg]);
		builder.put('.');
	}
	builder.put(ast[name]);
}

} // namespace Thor

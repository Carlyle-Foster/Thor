#include "util/file.h"

#include "ast.h"

namespace Thor {

// Stmt
void AstEmptyStmt::dump(const Ast&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put(';');
}

void AstBlockStmt::dump(const Ast& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put('{');
	builder.put('\n');
	const auto n_stmts = stmts.length();
	for (Ulen i = 0; i < n_stmts; i++) {
		ast[stmts[i]].dump(ast, builder, nest+1);
		builder.put('\n');
	}
	builder.rep(nest * 2, ' ');
	builder.put('}');
}

void AstPackageStmt::dump(const Ast&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("package");
	builder.put(' ');
	builder.put(name);
	builder.put(';');
	builder.put('\n');
}

void AstImportStmt::dump(const Ast&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("import");
	builder.put(' ');
	builder.put(path);
	builder.put(';');
	builder.put('\n');
}

void AstBreakStmt::dump(const Ast&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("break");
	if (!label.is_empty()) {
		builder.put(' ');
		builder.put(label);
	}
	builder.put(';');
}

void AstContinueStmt::dump(const Ast&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("continue");
	if (!label.is_empty()) {
		builder.put(' ');
		builder.put(label);
	}
	builder.put(';');
}

void AstFallthroughStmt::dump(const Ast&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put("fallthrough");
	builder.put(';');
}

void AstDeferStmt::dump(const Ast& ast, StringBuilder& builder, Ulen nest) const {
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

// Expr
void AstBinExpr::dump(const Ast& ast, StringBuilder& builder) const {
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

void AstUnaryExpr::dump(const Ast& ast, StringBuilder& builder) const {
	builder.put('(');
	static constexpr const StringView OP[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) MATCH,
		#include "lexer.inl"
	};
	builder.put(OP[Uint32(op)]);
	ast[operand].dump(ast, builder);
	builder.put(')');
}

void AstTernaryExpr::dump(const Ast& ast, StringBuilder& builder) const {
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

void AstIdentExpr::dump(const Ast&, StringBuilder& builder) const {
	builder.put(ident);
}

void AstUndefExpr::dump(const Ast&, StringBuilder& builder) const {
	builder.put("---");
}

void AstContextExpr::dump(const Ast&, StringBuilder& builder) const {
	builder.put("context");
}

// Type

} // namespace Thor
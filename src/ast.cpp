#include "util/file.h"

#include "ast.h"

namespace Thor {

// Stmt
void AstEmptyStmt::dump(const Ast&, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	builder.put(';');
}

void AstExprStmt::dump(const Ast& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	ast[expr].dump(ast, builder);
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


void AstIfStmt::dump(const Ast& ast, StringBuilder& builder, Ulen nest) const {
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
	builder.put('\n');
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

void AstDeclStmt::dump(const Ast& ast, StringBuilder& builder, Ulen nest) const {
	builder.rep(nest * 2, ' ');
	const auto n_lhs = lhs.length();
	for (Ulen i = 0; i < n_lhs; i++) {
		ast[lhs[i]].dump(ast, builder);
		if (i != n_lhs - 1) {
			builder.put(',');
			builder.put(' ');
		}
	}
	builder.put(':');
	ast[type].dump(ast, builder);
	if (rhs) {
		const auto n_rhs = rhs->length();
		builder.put(':');
		for (Ulen i = 0; i < n_rhs; i++) {
			ast[(*rhs)[i]].dump(ast, builder);
			if (i != n_rhs - 1) {
				builder.put(',');
			}
		}
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

void AstStructExpr::dump(const Ast& ast, StringBuilder& builder) const {
	builder.put("struct");
	builder.put(' ');
	builder.put('{');
	builder.put('\n');
	const auto n_decls = decls.length();
	for (Ulen i = 0; i < n_decls; i++) {
		ast[decls[i]].dump(ast, builder, 0);
		builder.put(',');
		builder.put('\n');
	}
	builder.put('}');
	builder.put('\n');
}

void AstTypeExpr::dump(const Ast& ast, StringBuilder& builder) const {
	if (expr) {
		ast[expr].dump(ast, builder);
	}
}

// Type

} // namespace Thor
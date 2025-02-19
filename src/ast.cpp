#include "util/file.h"

#include "ast.h"

namespace Thor {

// Stmt
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
	const auto n_stmts = stmts.length();
	for (Ulen i = 0; i < n_stmts; i++) {
		ast[stmts[i]].dump(ast, builder, nest+1);
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
	for (Ulen i = 0; i < lhs.length(); i++) {
		if (i != 0) {
			builder.put(", ");
		}
		ast[lhs[i]].dump(ast, builder);
	}

	builder.put(' ');
	builder.put(OP[Uint32(token.as_assign)]);
	builder.put(' ');

	for (Ulen i = 0; i < rhs.length(); i++) {
		if (i != 0) {
			builder.put(", ");
		}
		ast[rhs[i]].dump(ast, builder);
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

void AstProc::dump(const AstFile& ast, StringBuilder& builder) const {
	builder.put("proc");
	builder.put('(');
	if (params) {
		for(int i = 0; i < params->length(); i++) {
			ast[(*params)[i]].dump(ast, builder, 0);
			builder.put(", ");
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
	const auto n_decls = decls.length();
	for (Ulen i = 0; i < n_decls; i++) {
		ast[decls[i]].dump(ast, builder, 0);
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

} // namespace Thor
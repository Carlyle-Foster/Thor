#include "util/allocator.h"
#include "util/file.h"

#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

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
		Bool first = false;
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

void AstInteger::dump(const AstFile& ast, StringBuilder& builder) const {
	Uint8 buffer[128] = {0};
	Ulen len = snprintf((char*)buffer, 128, "%llu", value);
	StringView string = {(char*)buffer, len};
	builder.put(string);
}

void AstFloat::dump(const AstFile& ast, StringBuilder& builder) const {
	Uint8 buffer[128] = {0};
	Ulen len = snprintf((char*)buffer, 128, "%f", value);
	StringView string = {(char*)buffer, len};
	builder.put(string);
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

} // namespace Thor

#include "util/file.h"

#include "ast.h"

namespace Thor {

void AstPackageStmt::dump(const Ast&, StringBuilder& builder) const {
	builder.put("package");
	builder.put(' ');
	builder.put(name);
	builder.put(';');
	builder.put('\n');
}

void AstImportStmt::dump(const Ast&, StringBuilder& builder) const {
	builder.put("import");
	builder.put(' ');
	builder.put(path);
	builder.put(';');
	builder.put('\n');
}

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

} // namespace Thor
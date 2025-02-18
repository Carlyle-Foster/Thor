#ifndef THOR_PARSER_H
#define THOR_PARSER_H
#include "lexer.h"
#include "ast.h"
#include "util/system.h"

namespace Thor {

struct Parser {
	static Maybe<Parser> open(System& sys, StringView file);
	AstRef<AstStmt> parse_stmt();
	AstRef<AstExpr> parse_expr(Bool lhs);
	AstRef<AstExpr> parse_operand();
	AstRef<AstExpr> parse_bin_expr(Bool lhs, Uint32 prec);
	AstRef<AstExpr> parse_unary_expr(Bool lhs);
	AstRef<AstExpr> parse_operand(Bool lhs); // Operand parser for AstBinExpr or AstUnaryExpr

	AstRef<AstImportStmt> parse_import_stmt();
	AstRef<AstPackageStmt> parse_package_stmt();
	AstRef<AstIfStmt> parse_if_stmt();
	AstRef<AstWhenStmt> parse_when_stmt();
	AstRef<AstForStmt> parse_for_stmt();
	AstRef<AstDeferStmt> parse_defer_stmt();
	AstRef<AstReturnStmt> parse_return_stmt();

	[[nodiscard]] Ast& ast() { return ast_; }
	[[nodiscard]] const Ast& ast() const { return ast_; }
private:
	Parser(System& sys, Lexer&& lexer);

	template<Ulen E, typename... Ts>
	void error(const char (&msg)[E], Ts&&...) {
		sys_.console.write(sys_, StringView { msg });
		sys_.console.flush(sys_);
	}
	constexpr Bool is_kind(TokenKind kind) const {
		return token_.kind == kind;
	}
	constexpr Bool is_keyword(KeywordKind kind) const {
		return is_kind(TokenKind::KEYWORD) && token_.as_keyword == kind;
	}
	constexpr Bool is_operator(OperatorKind kind) const {
		return is_kind(TokenKind::OPERATOR) && token_.as_operator == kind;
	}
	constexpr Bool is_literal(LiteralKind kind) const {
		return is_kind(TokenKind::LITERAL) && token_.as_literal == kind;
	}
	void eat() {
		token_ = lexer_.next();
	}
	System&            sys_;
	TemporaryAllocator temporary_;
	Ast                ast_;
	Lexer              lexer_;
	Token              token_;
};

} // namespace Thor

#endif // THOR_PARSER_H
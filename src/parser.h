#ifndef THOR_PARSER_H
#define THOR_PARSER_H
#include "lexer.h"
#include "ast.h"
#include "util/system.h"

namespace Thor {

struct Parser {
	constexpr Parser(const System& sys, Lexer& lexer)
		: sys_{sys}
		, allocator_{sys}
		, temporary_{allocator_}
		, ast_{allocator_}
		, lexer_{lexer}
		, token_{lexer.next()}
	{
	}
	AstRef<AstStmt> parse_stmt();
	AstRef<AstImportStmt> parse_import_stmt();
	AstRef<AstPackageStmt> parse_package_stmt();
	Ast& ast() { return ast_; }
	const Ast& ast() const { return ast_; }
private:
	template<Ulen E, typename... Ts>
	void error(const char (&msg)[E], Ts&&...) {
		sys_.console.write(sys_, StringView { msg });
		sys_.console.flush(sys_);
	}
	inline Bool is_kind(TokenKind kind) const {
		return token_.kind == kind;
	}
	inline Bool is_keyword(KeywordKind kind) const {
		return is_kind(TokenKind::KEYWORD) && token_.as_keyword == kind;
	}
	inline Bool is_operator(OperatorKind kind) const {
		return is_kind(TokenKind::OPERATOR) && token_.as_operator == kind;
	}
	inline Bool is_literal(LiteralKind kind) const {
		return is_kind(TokenKind::LITERAL) && token_.as_literal == kind;
	}
	void eat() {
		token_ = lexer_.next();
	}
	const System&      sys_;
	SystemAllocator    allocator_;
	TemporaryAllocator temporary_;
	Ast                ast_;
	Lexer&             lexer_;
	Token              token_;
};

} // namespace Thor

#endif // THOR_PARSER_H
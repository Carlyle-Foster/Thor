#include "parser.h"

namespace Thor {

AstRef<AstPackageStmt> Parser::parse_package_stmt() {
	eat(); // Eat 'package'
	if (is_kind(TokenKind::IDENTIFIER)) {
		const auto ident = lexer_.string(token_);
		eat(); // Eat <ident>
		return ast_.create<AstPackageStmt>(ident);
	}
	error("Expected identifier for package\n");
	return {};
}

AstRef<AstImportStmt> Parser::parse_import_stmt() {
	eat(); // Eat 'import'
	if (is_literal(LiteralKind::STRING)) {
		const auto path = lexer_.string(token_);
		eat(); // Eat ""
		return ast_.create<AstImportStmt>(path);
	}
	error("Expected string literal for import path\n");
	return {};
}

AstRef<AstStmt> Parser::parse_stmt() {
	AstRef<AstStmt> stmt;
	if (is_keyword(KeywordKind::IMPORT)) {
		stmt = parse_import_stmt();
	} else if (is_keyword(KeywordKind::PACKAGE)) {
		stmt = parse_package_stmt();
	} else {
		error("Unimplemented statement\n");
		// Skip entire line for now for better error messages.
		while (!is_kind(TokenKind::SEMICOLON) && !is_kind(TokenKind::ENDOF)) {
			eat();
		}
	}
	if (is_kind(TokenKind::SEMICOLON)) {
		eat(); // Eat ';'
		return stmt;
	}
	if (!is_kind(TokenKind::ENDOF)) {
		error("Expected ';' or newline after statement\n");
	}
	return {};
}

} // namespace Thor
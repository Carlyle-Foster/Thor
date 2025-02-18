#include "parser.h"

namespace Thor {

Maybe<Parser> Parser::open(System& sys, StringView file) {
	if (auto lexer = Lexer::open(sys, file)) {
		return Parser { sys, move(*lexer) };
	}
	return {};
}

Parser::Parser(System& sys, Lexer&& lexer)
	: sys_{sys}
	, temporary_{sys.allocator}
	, ast_{sys.allocator}
	, lexer_{move(lexer)}
	, token_{lexer_.next()}
{
}

StringView Parser::parse_ident() {
	const auto result = lexer_.string(token_);
	eat(); // Eat <ident>
	return result;
}

AstRef<AstStmt> Parser::parse_stmt() {
	AstRef<AstStmt> stmt;
	if (is_kind(TokenKind::SEMICOLON)) {
		return parse_empty_stmt();
	} else if (is_kind(TokenKind::LBRACE)) {
		return parse_block_stmt();
	} else if (is_keyword(KeywordKind::PACKAGE)) {
		stmt = parse_package_stmt();
	} else if (is_keyword(KeywordKind::IMPORT)) {
		stmt = parse_import_stmt();
	} else if (is_keyword(KeywordKind::DEFER)) {
		stmt = parse_defer_stmt();
	} else if (is_keyword(KeywordKind::BREAK)) {
		stmt = parse_break_stmt();
	} else if (is_keyword(KeywordKind::CONTINUE)) {
		stmt = parse_continue_stmt();
	} else if (is_keyword(KeywordKind::FALLTHROUGH)) {
		stmt = parse_fallthrough_stmt();
	} else {
		// TODO
		error("Unimplemented\n");
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

AstRef<AstEmptyStmt> Parser::parse_empty_stmt() {
	if (!is_kind(TokenKind::SEMICOLON)) {
		error("Expected ';' (or newline)");
		return {};
	}
	eat(); // Eat ';'
	return ast_.create<AstEmptyStmt>();
}

AstRef<AstBlockStmt> Parser::parse_block_stmt() {
	if (!is_kind(TokenKind::LBRACE)) {
		error("Expected '{'");
		return {};
	}
	eat(); // Eat '{'
	Array<AstRef<AstStmt>> stmts{sys_.allocator};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto stmt = parse_stmt();
		if (!stmt || !stmts.push_back(stmt)) {
			return {};
		}
	}
	if (!is_kind(TokenKind::RBRACE)) {
		error("Expected '}'");
		return {};
	}
	eat(); // Eat '}'
	return ast_.create<AstBlockStmt>(move(stmts));
}

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

AstRef<AstBreakStmt> Parser::parse_break_stmt() {
	if (!is_keyword(KeywordKind::BREAK)) {
		error("Expected 'break'");
		return {};
	}
	eat(); // Eat 'break'
	StringView label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = parse_ident();
	}
	return ast_.create<AstBreakStmt>(label);
}

AstRef<AstContinueStmt> Parser::parse_continue_stmt() {
	if (!is_keyword(KeywordKind::CONTINUE)) {
		error("Expected 'continue'");
		return {};
	}
	eat(); // Eat 'continue'
	StringView label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = parse_ident();
	}
	return ast_.create<AstContinueStmt>(label);
}

AstRef<AstFallthroughStmt> Parser::parse_fallthrough_stmt() {
	if (!is_keyword(KeywordKind::FALLTHROUGH)) {
		error("Expected 'fallthrough'");
		return {};
	}
	eat(); // Eat 'fallthrough'
	return ast_.create<AstFallthroughStmt>();
}

AstRef<AstDeferStmt> Parser::parse_defer_stmt() {
	if (!is_keyword(KeywordKind::DEFER)) {
		error("Expected 'defer'");
		return {};
	}
	eat(); // Eat 'defer'
	auto stmt = parse_stmt();
	if (!stmt) {
		return {};
	}
	auto& s = ast_[stmt];
	if (s.is_stmt<AstEmptyStmt>()) {
		error("Empty statement after defer (e.g. ';')");
		return {};
	} else if (s.is_stmt<AstDeferStmt>()) {
		error("Cannot defer a defer statement");
		return {};
	}
	return ast_.create<AstDeferStmt>(stmt);
}

AstRef<AstIdentExpr> Parser::parse_ident_expr() {
	if (!is_kind(TokenKind::IDENTIFIER)) {
		error("Expected identifier");
		return {};
	}
	const auto ident = lexer_.string(token_);
	eat(); // Eat <ident>
	return ast_.create<AstIdentExpr>(ident);
}

AstRef<AstUndefExpr> Parser::parse_undef_expr() {
	if (!is_kind(TokenKind::UNDEFINED)) {
		error("Expected '---'");
		return {};
	}
	eat(); // Eat '---'
	return ast_.create<AstUndefExpr>();
}

AstRef<AstContextExpr> Parser::parse_context_expr() {
	if (!is_keyword(KeywordKind::CONTEXT)) {
		error("Expected 'context'");
		return {};
	}
	eat(); // Eat 'context'
	return ast_.create<AstContextExpr>();
}

AstRef<AstExpr> Parser::parse_expr(Bool lhs) {
	return parse_bin_expr(lhs, 1);
}

AstRef<AstExpr> Parser::parse_bin_expr(Bool lhs, Uint32 prec) {
	static constexpr const Uint32 PREC[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) PREC,
		#include "lexer.inl"
	};
	auto expr = parse_unary_expr(lhs);
	for (;;) {
		if (!is_kind(TokenKind::OPERATOR)) {
			error("Expected operator in binary expression");
			return {};
		}
		if (PREC[Uint32(token_.as_operator)] < prec) {
			// Stop climbing, found the correct precedence.
			break;
		}
		if (is_operator(OperatorKind::QUESTION)) {
			auto on_true = parse_expr(lhs);
			if (!is_operator(OperatorKind::COLON)) {
				error("Expected ':' after ternary condition");
				return {};
			}
			eat(); // Eat ':'
			auto on_false = parse_expr(lhs);
			expr = ast_.create<AstTernaryExpr>(expr, on_true, on_false);
		} else {
			auto rhs = parse_bin_expr(false, prec+1);
			if (!rhs) {
				error("Expected expression on right-hand side of binary operator");
				return {};
			}
			expr = ast_.create<AstBinExpr>(expr, rhs, token_.as_operator);
		}
		lhs = false;
	}
	return expr;
}

AstRef<AstExpr> Parser::parse_unary_expr(Bool lhs) {
	if (is_operator(OperatorKind::TRANSMUTE) ||
	    is_operator(OperatorKind::CAST))
	{
		eat(); // Eat 'transmute' or 'cast'
		if (!is_operator(OperatorKind::LPAREN)) {
			error("Expected '(' after cast");
			return {};
		}
		eat(); // Eat '('
		// auto type = parse_type();
		if (!is_operator(OperatorKind::RPAREN)) {
			error("Expected ')' after cast");
			return {};
		}
		eat(); // Eat ')'
		if (auto expr = parse_unary_expr(lhs)) {
			// return ast_.create<AstExplicitCastExpr>(type, expr);
		}
	} else if (is_operator(OperatorKind::AUTO_CAST)) {
		eat(); // Eat 'auto_cast'
		if (auto expr = parse_unary_expr(lhs)) {
			// return ast_.create<AstAutoCastExpr>(expr);
		}
	} else if (is_operator(OperatorKind::ADD)  ||
	           is_operator(OperatorKind::SUB)  ||
	           is_operator(OperatorKind::XOR)  ||
	           is_operator(OperatorKind::BAND) ||
	           is_operator(OperatorKind::LNOT) ||
	           is_operator(OperatorKind::MUL))
	{
		const auto op = token_.as_operator;
		eat(); // Eat op
		if (auto expr = parse_unary_expr(lhs)) {
			return ast_.create<AstUnaryExpr>(expr, op);
		}
	} else if (is_operator(OperatorKind::PERIOD)) {
		eat(); // Eat '.'
		// if (auto ident = parse_ident()) {
		// 	//return ast_.create<AstImplicitSelectorExpr>(ident);
		// }
	}
	return {};
}

AstRef<AstExpr> Parser::parse_operand(Bool lhs) {
	(void)lhs;
	if (is_kind(TokenKind::IDENTIFIER)) {
		return parse_ident_expr();
	} else if (is_kind(TokenKind::UNDEFINED)) {
		return parse_undef_expr();
	} else if (is_keyword(KeywordKind::CONTEXT)) {
		return parse_context_expr();
	}
	return {};
}

} // namespace Thor
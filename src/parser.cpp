#include <stdlib.h>
#include <stdio.h>

#include "parser.h"
#include "util/allocator.h"

namespace Thor {

struct Debug {
	Debug(System& sys, StringView name, Sint32 line) {
		ScratchAllocator<1024> scratch{sys.allocator};
		StringBuilder builder{scratch};
		builder.rep(s_depth*2, ' ');
		builder.put(name);
		builder.put(':');
		builder.put(line);
		builder.put('\n');
		if (auto result = builder.result()) {
			sys.console.write(sys, *result);
		}
		s_depth++;
	}
	~Debug() {
		s_depth--;
	}
	static inline int s_depth = 0;
};

// #define TRACE()
#define TRACE() \
	auto debug_ ## __LINE__ = Debug{sys_, __func__, __LINE__}

void Parser::eat() {
	do {
		token_ = lexer_.next();
	} while (is_kind(TokenKind::COMMENT));
}

Maybe<Parser> Parser::open(System& sys, StringView filename) {
	auto lexer = Lexer::open(sys, filename);
	if (!lexer) {
		// Could not open filename
		return {};
	}
	auto file = AstFile::create(sys.allocator, filename);
	if (!file) {
		// Could not create astfile
		return {};
	}
	return Parser { sys, move(*lexer), move(*file) };
}

Parser::Parser(System& sys, Lexer&& lexer, AstFile&& ast)
	: sys_{sys}
	, temporary_{sys.allocator}
	, ast_{move(ast)}
	, lexer_{move(lexer)}
	, token_{TokenKind::INVALID, 0, 0}
{
	eat();
}

StringView Parser::parse_ident() {
	const auto result = lexer_.string(token_);
	eat(); // Eat <ident>
	return result;
}

Maybe<Array<AstRef<AstExpr>>> Parser::parse_expr_list(Bool lhs) {
	TRACE();
	Array<AstRef<AstExpr>> exprs{temporary_};
	for (;;) {
		auto expr = parse_expr(lhs);
		if (!expr || !exprs.push_back(expr)) {
			return {};
		}
		if (!is_kind(TokenKind::COMMA) || is_kind(TokenKind::ENDOF)) {
			break;
		}
	}
	return exprs;
}

AstRef<AstProcExpr> Parser::parse_proc_expr() {
	TRACE();
	AstRef<AstProcType> type; // = parse_proc_type() TODO(dweiler):
	if (!type) {
		return {};
	}
	auto block = parse_block_stmt();
	if (!block) {
		return {};
	}
	return ast_.create<AstProcExpr>(type, block);
}

/*
// Decl := Ident (',' Ident)* (':' Type)? (('=' | ':') Expr (',' Expr)*)? (',')?
//      |= Type (',' Type)* (',')?
AstRef<AstDecl> Parser::parse_decl() {
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	Array<AstRef<AstType>> types{temporary_};
	while (is_kind(TokenKind::IDENTIFIER) && !is_kind(TokenKind::ENDOF)) {
		auto type = parse_type();
		if (!type) {
			return {};
		}
		if (!types.push_back(type)) {
			return {};
		}
		if (is_kind(TokenKind::COMMA)) {
			eat(); // Eat ','
		} else {
			break;
		}
	}
	Array<AstStringRef> names{temporary_};
	if (is_kind(TokenKind::COLON)) {
		// The types are actually identifiers. Ensure they are all AstNamedType and
		// convert them to names.
		for (auto type_ref : types) {
			auto& type = ast_[type_ref];
			if (auto named = type.to_type<AstNamedType>()) {
				if (!names.push_back(named.name)) {
					return {};
				}
			} else {
				return error("Unexpected type");
			}
		}
		types.clear();
		eat(); // Eat ':'
	}
	AstRef<AstType> type;
	if (!is_kind(TokenKind::COLON) && !is_assignment(AssignKind::EQ)) {
		type = parse_type();
		if (!type) {
			return {};
		}
	}
	Bool immutable = false;
	Bool values = false;
	if (is_kind(TokenKind::COLON)) {
		eat(); // Eat ':'
		immutable = true;
		values = true;
	} else if (is_assignment(AssignKind::EQ)) {
		eat(); // Eat '='
		immutable = false;
		values = true;
	}
	Array<AstRef<AstExpr>> exprs{temporary_};
	if (values) do {
		if (is_kind(TokenKind::SEMICOLON)) {
			break;
		}
		auto expr = parse_expr();
		if (!expr) {
			return {};
		}
		if (!exprs.push_back(expr)) {
			return {};
		}
	} while (is_kind(TokenKind::COMMA) && !is_kind(TokenKind::ENDOF));
}
*/

AstRef<AstStmt> Parser::parse_stmt() {
	TRACE();
	AstRef<AstStmt> stmt;
	if (is_kind(TokenKind::SEMICOLON)) {
		return parse_empty_stmt();
	} else if (is_kind(TokenKind::LBRACE)) {
		return parse_block_stmt();
	} else if (is_kind(TokenKind::ATTRIBUTE)) {
		// TODO
	} else if (is_kind(TokenKind::DIRECTIVE)) {
		// TODO
	} else if (is_keyword(KeywordKind::PACKAGE)) {
		stmt = parse_package_stmt();
	} else if (is_keyword(KeywordKind::IMPORT)) {
		stmt = parse_import_stmt();
	} else if (is_keyword(KeywordKind::DEFER)) {
		stmt = parse_defer_stmt();
	} else if (is_keyword(KeywordKind::RETURN)) {
		stmt = parse_return_stmt();
	} else if (is_keyword(KeywordKind::BREAK)) {
		stmt = parse_break_stmt();
	} else if (is_keyword(KeywordKind::CONTINUE)) {
		stmt = parse_continue_stmt();
	} else if (is_keyword(KeywordKind::FALLTHROUGH)) {
		stmt = parse_fallthrough_stmt();
	} else if (is_keyword(KeywordKind::FOREIGN)) {
		// TODO
	} else if (is_keyword(KeywordKind::IF)) {
		stmt = parse_if_stmt();
	} else if (is_keyword(KeywordKind::WHEN)) {
		stmt = parse_when_stmt();
	} else if (is_keyword(KeywordKind::FOR)) {
		// TODO
	} else if (is_keyword(KeywordKind::SWITCH)) {
		// TODO
	} else if (is_keyword(KeywordKind::USING)) {
		// TODO
	} else {
		// stmt = parse_decl_stmt();
	}
	if (!stmt) {
		return {};
	}
	if (is_kind(TokenKind::SEMICOLON)) {
		eat(); // Eat ';'
		return stmt;
	}
	if (!is_kind(TokenKind::ENDOF)) {
		return error("Expected ';' or newline after statement");
	}
	return {};
}

AstRef<AstEmptyStmt> Parser::parse_empty_stmt() {
	TRACE();
	if (!is_kind(TokenKind::SEMICOLON)) {
		return error("Expected ';' (or newline)");
	}
	eat(); // Eat ';'
	return ast_.create<AstEmptyStmt>();
}

AstRef<AstBlockStmt> Parser::parse_block_stmt() {
	TRACE();
	if (!is_kind(TokenKind::LBRACE)) {
		error("Expected '{'");
		return {};
	}
	eat(); // Eat '{'
	Array<AstRef<AstStmt>> stmts{temporary_};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto stmt = parse_stmt();
		if (!stmt || !stmts.push_back(stmt)) {
			return {};
		}
	}
	if (!is_kind(TokenKind::RBRACE)) {
		return error("Expected '}'");
	}
	eat(); // Eat '}'
	auto refs = ast_.insert(move(stmts));
	return ast_.create<AstBlockStmt>(refs);
}

AstRef<AstPackageStmt> Parser::parse_package_stmt() {
	TRACE();
	eat(); // Eat 'package'
	if (is_kind(TokenKind::IDENTIFIER)) {
		const auto ident = lexer_.string(token_);
		eat(); // Eat <ident>
		if (auto ref = ast_.insert(ident)) {
			return ast_.create<AstPackageStmt>(ref);
		} else {
			// Out of memory.
			return {};
		}
	}
	return error("Expected identifier for package");
}

AstRef<AstImportStmt> Parser::parse_import_stmt() {
	TRACE();
	eat(); // Eat 'import'
	if (is_literal(LiteralKind::STRING)) {
		const auto path = lexer_.string(token_);
		eat(); // Eat ""
		if (auto ref = ast_.insert(path)) {
			return ast_.create<AstImportStmt>(ref);
		} else {
			return {};
		}
	}
	return error("Expected string literal for import path");
}

AstRef<AstBreakStmt> Parser::parse_break_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::BREAK)) {
		return error("Expected 'break'");
	}
	eat(); // Eat 'break'
	StringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = ast_.insert(parse_ident());
	}
	return ast_.create<AstBreakStmt>(label);
}

AstRef<AstContinueStmt> Parser::parse_continue_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTINUE)) {
		return error("Expected 'continue'");
	}
	eat(); // Eat 'continue'
	StringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = ast_.insert(parse_ident());
	}
	return ast_.create<AstContinueStmt>(label);
}

AstRef<AstFallthroughStmt> Parser::parse_fallthrough_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::FALLTHROUGH)) {
		return error("Expected 'fallthrough'");
	}
	eat(); // Eat 'fallthrough'
	return ast_.create<AstFallthroughStmt>();
}

AstRef<AstIfStmt> Parser::parse_if_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::IF)) {
		return error("Expected 'if'");
	}
	eat(); // Eat 'if'
	Maybe<AstRef<AstStmt>> init;
	AstRef<AstExpr>        cond;
	AstRef<AstStmt>        on_true;
	Maybe<AstRef<AstStmt>> on_false;

	auto prev_level = this->expr_level_;
	this->expr_level_ = -1;

	auto prev_allow_in_expr = this->allow_in_expr_;
	this->allow_in_expr_ = true;

	if (is_kind(TokenKind::SEMICOLON)) {
		cond = parse_expr(false);
	} else {
		init.reset(); // parse_decl_stmt();
		if (is_kind(TokenKind::SEMICOLON)) {
			cond = parse_expr(false);
		} else {
			if (init && ast_[*init].is_stmt<AstExprStmt>()) {
				cond = static_cast<AstExprStmt const &>(ast_[*init]).expr;
			} else {
				error("Expected a boolean expression");
			}
			init.reset();
		}
	}

	this->expr_level_    = prev_level;
	this->allow_in_expr_ = prev_allow_in_expr;

	if (!cond.is_valid()) {
		return error("Expected a condition for if statement");
	}

	if (is_keyword(KeywordKind::DO)) {
		eat(); // Eat 'do'
		// TODO(bill): enforce same line behaviour
		on_true = parse_stmt();
	} else {
		on_true = parse_block_stmt();
	}

	skip_possible_newline_for_literal();
	if (is_keyword(KeywordKind::ELSE)) {
		// Token else_tok = token_;
		eat();
		if (is_keyword(KeywordKind::IF)) {
			on_false = parse_if_stmt();
		} else if (is_kind(TokenKind::LBRACE)) {
			on_false = parse_block_stmt();
		} else if (is_keyword(KeywordKind::DO)) {
			eat(); // Eat 'do'
			// TODO(bill): enforce same line behaviour
			on_false = parse_stmt();
		} else {
			return error("Expected if statement block statement");
		}
	}

	return ast_.create<AstIfStmt>(move(init), cond, on_true, move(on_false));
}

AstRef<AstExprStmt> Parser::parse_expr_stmt() {
	auto expr = parse_expr(false);
	if (!expr) {
		return {};
	}
	return ast_.create<AstExprStmt>(expr);
}

AstRef<AstWhenStmt> Parser::parse_when_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::WHEN)) {
		return error("Expected 'when'");
	}
	eat(); // Eat 'when'
	auto cond = parse_expr(false);
	if (!cond) {
		return {};
	}
	AstRef<AstBlockStmt> body;
	if (is_keyword(KeywordKind::DO)) {
		eat(); // Eat 'do'
		auto stmt = parse_expr_stmt();
		if (!stmt) {
			return {};
		}
		Array<AstRef<AstStmt>> stmts{temporary_};
		if (!stmts.push_back(stmt)) {
			return {};
		}
		auto refs = ast_.insert(move(stmts));
		body = ast_.create<AstBlockStmt>(refs);
	} else {
		body = parse_block_stmt();
	}
	if (!body) {
		return {};
	}
	return ast_.create<AstWhenStmt>(cond, body);
}

Bool Parser::skip_possible_newline_for_literal() {
	if (is_kind(TokenKind::SEMICOLON) && lexer_.string(token_) == "\n") {
		// peek
		eat();
		// TODO
	}
	return false;
}


AstRef<AstDeferStmt> Parser::parse_defer_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::DEFER)) {
		return error("Expected 'defer'");
	}
	eat(); // Eat 'defer'
	auto stmt = parse_stmt();
	if (!stmt) {
		return {};
	}
	auto& s = ast_[stmt];
	if (s.is_stmt<AstEmptyStmt>()) {
		return error("Empty statement after defer (e.g. ';')");
	} else if (s.is_stmt<AstDeferStmt>()) {
		return error("Cannot defer a defer statement");
	}
	return ast_.create<AstDeferStmt>(stmt);
}

AstRef<AstReturnStmt> Parser::parse_return_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::RETURN)) {
		return error("Expected 'return'");
	}
	eat(); // Eat 'return'
	Array<AstRef<AstExpr>> exprs{temporary_};
	for (;;) {
		auto expr = parse_expr(false);
		if (!expr) {
			return {};
		}
		if (!exprs.push_back(expr)) {
			return {};
		}
		if (is_kind(TokenKind::COMMA)) {
			eat(); // Eat ','
		} else {
			break;
		}
	}
	auto refs = ast_.insert(move(exprs));
	return ast_.create<AstReturnStmt>(refs);
}

AstRef<AstIdentExpr> Parser::parse_ident_expr() {
	TRACE();
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	const auto ident = ast_.insert(parse_ident());
	return ast_.create<AstIdentExpr>(ident);
}

AstRef<AstUndefExpr> Parser::parse_undef_expr() {
	TRACE();
	if (!is_kind(TokenKind::UNDEFINED)) {
		return error("Expected '---'");
	}
	eat(); // Eat '---'
	return ast_.create<AstUndefExpr>();
}

AstRef<AstContextExpr> Parser::parse_context_expr() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTEXT)) {
		return error("Expected 'context'");
	}
	eat(); // Eat 'context'
	return ast_.create<AstContextExpr>();
}

AstRef<AstExpr> Parser::parse_expr(Bool lhs) {
	TRACE();
	return parse_bin_expr(lhs, 1);
}

AstRef<AstExpr> Parser::parse_bin_expr(Bool lhs, Uint32 prec) {
	TRACE();
	static constexpr const Uint32 PREC[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) PREC,
		#include "lexer.inl"
	};
	auto expr = parse_unary_expr(lhs);
	if (!expr) {
		return error("Could not parse unary prefix");
	}
	for (;;) {
		if (!is_kind(TokenKind::OPERATOR)) {
			// error("Expected operator in binary expression");
			break;
		}
		if (PREC[Uint32(token_.as_operator)] < prec) {
			// Stop climbing, found the correct precedence.
			break;
		}
		if (is_operator(OperatorKind::QUESTION)) {
			eat(); // Eat '?'
			auto on_true = parse_expr(lhs);
			if (!on_true) {
				return {};
			}
			if (!is_operator(OperatorKind::COLON)) {
				return error("Expected ':' after ternary condition");
			}
			eat(); // Eat ':'
			auto on_false = parse_expr(lhs);
			if (!on_false) {
				return {};
			}
			expr = ast_.create<AstTernaryExpr>(expr, on_true, on_false);
		} else {
			auto kind = token_.as_operator;
			eat(); // Eat operator
			auto rhs = parse_bin_expr(false, prec+1);
			if (!rhs) {
				return error("Expected expression on right-hand side of binary operator");
			}
			expr = ast_.create<AstBinExpr>(expr, rhs, kind);
		}
		lhs = false;
	}
	return expr;
}

AstRef<AstExpr> Parser::parse_unary_atom(AstRef<AstExpr> operand, Bool lhs) {
	TRACE();
	if (!operand) {
		// printf("No operand\n");
		// error("Expected an operand");
		return {};
	}
	for (;;) {
		if (is_operator(OperatorKind::LPAREN)) {
			// operand(...)
		} else if (is_operator(OperatorKind::PERIOD)) {
			// operand.expr(...)
		} else if (is_operator(OperatorKind::ARROW)) {
			// operand->expr()
		} else if (is_operator(OperatorKind::LBRACKET)) {
			// operand[a]
			// operand[:]
			// operand[a:]
			// operand[:a]
			// operand[a:b]
			// operand[a,b]
			// operand[a..=b]
			// operand[a..<b]
			// operand[...]
			// operand[?]
		} else if (is_operator(OperatorKind::POINTER)) {
			// operand^
		} else if (is_operator(OperatorKind::OR_RETURN)) {
			// operand or_return
		} else if (is_operator(OperatorKind::OR_BREAK)) {
			// operand or_break
		} else if (is_operator(OperatorKind::OR_CONTINUE)) {
			// operand or_continue
		} else if (is_kind(TokenKind::LBRACE)) {
			// operand {
			if (lhs) {
				break;
			}
		} else {
			break;
		}
		lhs = false;
	}
	return operand;
}

AstRef<AstExpr> Parser::parse_unary_expr(Bool lhs) {
	TRACE();
	if (is_operator(OperatorKind::TRANSMUTE) ||
	    is_operator(OperatorKind::CAST))
	{
		eat(); // Eat 'transmute' or 'cast'
		if (!is_operator(OperatorKind::LPAREN)) {
			return error("Expected '(' after cast");
		}
		eat(); // Eat '('
		auto type = parse_type();
		if (!type) {
			return {};
		}
		if (!is_operator(OperatorKind::RPAREN)) {
			return error("Expected ')' after cast");
		}
		eat(); // Eat ')'
		if (auto expr = parse_unary_expr(lhs)) {
			return ast_.create<AstCastExpr>(type, expr);
		}
	} else if (is_operator(OperatorKind::AUTO_CAST)) {
		eat(); // Eat 'auto_cast'
		if (auto expr = parse_unary_expr(lhs)) {
			return ast_.create<AstCastExpr>(AstRef<AstType>{}, expr);
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
	auto operand = parse_operand(lhs);
	return parse_unary_atom(operand, lhs);
}

AstRef<AstExpr> Parser::parse_value_literal() {
	TRACE();
	InlineAllocator<1024> allocator;
	auto string = lexer_.string(token_);
	auto terminated = allocator.allocate<char>(string.length() + 1, false);
	memcpy(terminated, string.data(), string.length());
	terminated[string.length()] = '\0';
	if (is_literal(LiteralKind::INTEGER)) {
		eat(); // Eat literal
		Uint64 value = strtoul(terminated, nullptr, 10);
		return ast_.create<AstIntExpr>(value);
	} else if (is_literal(LiteralKind::FLOAT)) {
		eat(); // Eat literal
		Float64 value = strtod(terminated, nullptr);
		return ast_.create<AstFloatExpr>(value);
	}
	return {};
}

AstRef<AstExpr> Parser::parse_operand(Bool lhs) {
	TRACE();
	(void)lhs;
	if (is_kind(TokenKind::LITERAL)) {
		return parse_value_literal();
	} else if (is_kind(TokenKind::IDENTIFIER)) {
		return parse_ident_expr();
	} else if (is_kind(TokenKind::UNDEFINED)) {
		return parse_undef_expr();
	} else if (is_keyword(KeywordKind::CONTEXT)) {
		return parse_context_expr();
	} else if (is_keyword(KeywordKind::PROC)) {
		return parse_proc_expr();
	}
	return {};
}

AstRef<AstUnionType> Parser::parse_union_type() {
	TRACE();
	if (!is_keyword(KeywordKind::UNION)) {
		return error("Expected 'union'");
	}
	eat(); // Eat 'union'
	if (!is_kind(TokenKind::LBRACE)) {
		return error("Expected '{'");
	}
	Array<AstRef<AstType>> types{temporary_};
	eat(); // Eat '}'
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto type = parse_type();
		if (!type || !types.push_back(type)) {
			return {};
		}
		if (!is_kind(TokenKind::COMMA)) {
			break;
		}
		eat(); // Eat ','
	}
	if (!is_kind(TokenKind::RBRACE)) {
		return error("Expected '}'");
	}
	eat(); // '}'
	auto refs = ast_.insert(move(types));
	return ast_.create<AstUnionType>(refs);
}

AstRef<AstEnumType> Parser::parse_enum_type() {
	TRACE();
	if (!is_keyword(KeywordKind::ENUM)) {
		return error("Expected 'enum'");
	}
	eat(); // Eat 'enum'
	AstRef<AstType> base;
	if (!is_kind(TokenKind::LBRACE)) {
		if (!(base = parse_type())) {
			return {};
		}
	}
	if (!is_kind(TokenKind::LBRACE)) {
		return error("Expected '{'");
	}
	eat(); // Eat '{'
	Array<AstRef<AstEnum>> enums{temporary_};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto e = parse_enum();
		if (!e) {
			break;
		}
		if (!enums.push_back(e)) {
			return {};
		}
		if (is_kind(TokenKind::COMMA)) {
			eat(); // Eat ','
		} else {
			break;
		}
	}
	if (!is_kind(TokenKind::RBRACE)) {
		return error("Expected '}' to terminate enum");
	}
	eat(); // Eat '}'
	auto refs = ast_.insert(move(enums));
	return ast_.create<AstEnumType>(base, refs);
}

AstRef<AstPtrType> Parser::parse_ptr_type() {
	TRACE();
	if (!is_operator(OperatorKind::POINTER)) {
		return error("Expected '^'");
	}
	eat(); // Eat '^'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstPtrType>(base);
}

AstRef<AstMultiPtrType> Parser::parse_multiptr_type() {
	TRACE();
	if (!is_operator(OperatorKind::POINTER)) {
		return error("Expected '^'");
	}
	eat(); // Eat '^'
	if (!is_operator(OperatorKind::RBRACKET)) {
		return error("Expected ']'");
	}
	eat(); // Eat ']'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstMultiPtrType>(base);
}

AstRef<AstSliceType> Parser::parse_slice_type() {
	TRACE();
	if (!is_operator(OperatorKind::RBRACKET)) {
		return error("Expected ']'");
	}
	eat(); // Eat ']'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstSliceType>(base);
}

AstRef<AstArrayType> Parser::parse_array_type() {
	TRACE();
	AstRef<AstExpr> size;
	if (is_operator(OperatorKind::QUESTION)) {
		eat(); // Eat '?'
	} else {
		size = parse_expr(false);
		if (!size) {
			return {};
		}
	}
	if (!is_operator(OperatorKind::RBRACKET)) {
		StringBuilder builder{temporary_};
		builder.put(Uint32(token_.kind));
		builder.put('\n');
		builder.put(Uint32(token_.as_operator));
		// error("Expected ']' (here)");
		// error(*builder.result())
		sys_.console.write(sys_, *builder.result());
		return {};
	}
	eat(); // Eat ']'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstArrayType>(size, base);
}

AstRef<AstNamedType> Parser::parse_named_type() {
	TRACE();
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	const auto name = lexer_.string(token_);
	StringRef name_ref = ast_.insert(name);
	StringRef pkg_ref;
	if (!name_ref) {
		return {};
	}
	eat(); // Eat ident
	if (is_operator(OperatorKind::PERIOD)) {
		eat(); // Eat '.'
		if (!is_kind(TokenKind::IDENTIFIER)) {
			return error("Expected identifier after '.'");
		}
		const auto name = lexer_.string(token_);
		// Exchange name_ref with pkg_ref since:
		//   a.b -> a is the package, b is the name
		pkg_ref = name_ref;
		name_ref = ast_.insert(name);
		if (!pkg_ref) {
			return {};
		}
		eat(); // Eat ident
	}
	return ast_.create<AstNamedType>(pkg_ref, name_ref);
}

AstRef<AstEnum> Parser::parse_enum() {
	TRACE();
	// Ident ('=' Expr)?
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	auto name = lexer_.string(token_);
	auto ref = ast_.insert(name);
	if (!ref) {
		return {};
	}
	eat(); // Eat Ident
	AstRef<AstExpr> expr;
	if (is_assignment(AssignKind::EQ)) {
		eat(); // Eat '='
		expr = parse_expr(false);
		if (!expr) {
			return {};
		}
	}
	return ast_.create<AstEnum>(ref, expr);
}

AstRef<AstParenType> Parser::parse_paren_type() {
	TRACE();
	if (!is_operator(OperatorKind::LPAREN)) {
		return error("Expoected '('");
	}
	eat(); // Eat '('
	auto type = parse_type();
	if (!type) {
		return {};
	}
	if (!is_operator(OperatorKind::RPAREN)) {
		return error("Expected ')'");
	}
	eat(); // Eat ')'
	return ast_.create<AstParenType>(type);
}

AstRef<AstType> Parser::parse_type() {
	TRACE();
	if (is_keyword(KeywordKind::UNION)) {
		return parse_union_type();
	} else if (is_keyword(KeywordKind::ENUM)) {
		return parse_enum_type();
	} else if (is_operator(OperatorKind::POINTER)) {
		return parse_ptr_type();
	} else if (is_operator(OperatorKind::LBRACKET)) {
		eat(); // Eat '['
		if (is_operator(OperatorKind::POINTER)) {
			return parse_multiptr_type(); // assuming '[' has already been consumed
		} else if (is_operator(OperatorKind::RBRACKET)) {
			return parse_slice_type(); // assuming '[' has already been consumed
		} else {
			return parse_array_type(); // assuming '[' has already been consumed
		}
	} else if (is_operator(OperatorKind::LPAREN)) {
		return parse_paren_type();
	} else if (is_kind(TokenKind::IDENTIFIER)) {
		auto named = parse_named_type();
		if (!named) {
			return {};
		}
		if (is_operator(OperatorKind::LPAREN)) {
			eat(); // Eat '('
			Array<AstRef<AstExpr>> exprs{temporary_};
			while (!is_operator(OperatorKind::RPAREN) && !is_kind(TokenKind::ENDOF)) {
				auto expr = parse_expr(false);
				if (!expr || !exprs.push_back(expr)) {
					return {};
				}
				if (is_kind(TokenKind::COMMA)) {
					eat(); // Eat ','
				} else {
					break;
				}
			}
			if (!is_operator(OperatorKind::RPAREN)) {
				return error("Expected ')'");
			}
			eat(); // Eat ')'
			auto refs = ast_.insert(move(exprs));
			return ast_.create<AstParamType>(named, refs);
		} else {
			return named;
		}
	}
	return error("Unexpected token while parsing type");
}

} // namespace Thor

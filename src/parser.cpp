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

AstStringRef Parser::parse_ident() {
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	auto str = lexer_.string(token_);
	eat(); // Eat <ident>
	return ast_.insert(str);
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
// Decl := Ident (',' Ident)* ':' Type (('=' | ':') Expr (',' Expr)*)? (',')?
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

// Stmt := EmptyStmt ';'
//       | BlockStmt ';'
//       | PackageStmt ';'
//       | ImportStmt ';'
//       | DeferStmt ';'
//       | ReturnStmt ';'
//       | BreakStmt ';'
//       | ContinueStmt ';'
//       | FallthroughStmt ';'
//       | ForeignStmt ';'
//       | IfStmt ';'
//       | WhenStmt ';'
//       | ForStmt ';'
//       | SwitchStmt ';'
//       | UsingStmt ';'
//       | DeclStmt ';'
AstRef<AstStmt> Parser::parse_stmt(Bool use, DirectiveList&& directives, AttributeList&& attributes) {
	TRACE();
	AstRef<AstStmt> stmt;
	if (is_kind(TokenKind::SEMICOLON)) {
		return parse_empty_stmt();
	} else if (is_kind(TokenKind::LBRACE)) {
		return parse_block_stmt();
	} else if (is_kind(TokenKind::ATTRIBUTE)) {
		// Encountered an attribute of the form:
		// 	'@' 'attribute'
		// 	'@' '(' 'attribute' ('=' Expr)? (',' 'attribute' ('=' Expr)?)* ')'
		// In the statement scope. The attribute applies to the statement that will
		// proceed it. Just pass it along recursively.
		if (auto attributes = parse_attributes()) {
			return parse_stmt(false, {}, move(attributes));
		} else {
			return {};
		}
	} else if (is_kind(TokenKind::DIRECTIVE)) {
		// Encountered a directive of the form:
		//	'#' Ident
		//	'#' Ident '(' Expr (',' Expr)* ')'
		// In the statement scope. The directive applies to the statement that will
		// proceed it. Just pass it along recursively.
		if (auto directives = parse_directives()) {
			return parse_stmt(false, move(directives), {});
		} else {
			return {};
		}
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
		eat(); // Eat 'foreign'
		if (is_keyword(KeywordKind::IMPORT)) {
			return parse_foreign_import_stmt();
		} else {
			// TODO(dweiler): foreign bindings
		}
	} else if (is_keyword(KeywordKind::IF)) {
		stmt = parse_if_stmt();
	} else if (is_keyword(KeywordKind::WHEN)) {
		stmt = parse_when_stmt();
	} else if (is_keyword(KeywordKind::FOR)) {
		// TODO
	} else if (is_keyword(KeywordKind::SWITCH)) {
		// TODO
	} else if (is_keyword(KeywordKind::USING)) {
		eat(); // Eat 'using'
		if (is_kind(TokenKind::IDENTIFIER)) {
			// This is a using statement like 'using fmt'
			stmt = parse_using_stmt();
		} else {
			// Otherwise it's a flag on a decl like
			//  using x: T
			return parse_stmt(true, {}, {});
		}
	} else {
		// DeclStmt := Ident (',' Ident)+ ':' Type? ((':'|'=') Expr (',' Expr)+)?
		//           | Type (',' Type)+
		// stmt = parse_decl_stmt();
		return error("Unexpected token");
	}
	if (!stmt) {
		return {};
	}
	if (!is_kind(TokenKind::SEMICOLON)) {
		return error("Expected ';' or newline after statement");
	}
	eat(); // Eat ';'
	return stmt;
}

// EmptyStmt := ';'
AstRef<AstEmptyStmt> Parser::parse_empty_stmt() {
	TRACE();
	if (!is_kind(TokenKind::SEMICOLON)) {
		return error("Expected ';' (or newline)");
	}
	eat(); // Eat ';'
	return ast_.create<AstEmptyStmt>();
}

// BlockStmt := '{' Stmt* '}'
AstRef<AstBlockStmt> Parser::parse_block_stmt() {
	TRACE();
	if (!is_kind(TokenKind::LBRACE)) {
		error("Expected '{'");
		return {};
	}
	eat(); // Eat '{'
	Array<AstRef<AstStmt>> stmts{temporary_};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto stmt = parse_stmt(false, {}, {});
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

// PackageStmt := 'package' Ident
AstRef<AstPackageStmt> Parser::parse_package_stmt() {
	TRACE();
	eat(); // Eat 'package'
	if (is_kind(TokenKind::IDENTIFIER)) {
		if (auto name = parse_ident()) {
			return ast_.create<AstPackageStmt>(name);
		} else {
			return {};
		}
	}
	return error("Expected identifier for package");
}

// ImportStmt := 'import' Ident? StringLit
AstRef<AstImportStmt> Parser::parse_import_stmt() {
	TRACE();
	eat(); // Eat 'import'
	AstStringRef alias;
	if (is_kind(TokenKind::IDENTIFIER)) {
		alias = parse_ident();
	}
	auto expr = parse_string_expr();
	if (!expr) {
		return {};
	}
	return ast_.create<AstImportStmt>(alias, expr);
}

// BreakStmt := 'break' Ident?
AstRef<AstBreakStmt> Parser::parse_break_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::BREAK)) {
		return error("Expected 'break'");
	}
	eat(); // Eat 'break'
	AstStringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = parse_ident();
	}
	return ast_.create<AstBreakStmt>(label);
}

// ContinueStmt := 'continue' Ident?
AstRef<AstContinueStmt> Parser::parse_continue_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTINUE)) {
		return error("Expected 'continue'");
	}
	eat(); // Eat 'continue'
	AstStringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = parse_ident();
	}
	return ast_.create<AstContinueStmt>(label);
}

// FallthroughStmt := 'fallthrough'
AstRef<AstFallthroughStmt> Parser::parse_fallthrough_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::FALLTHROUGH)) {
		return error("Expected 'fallthrough'");
	}
	eat(); // Eat 'fallthrough'
	return ast_.create<AstFallthroughStmt>();
}

// ForeignImportStmt := 'foreign' 'import' Ident? StringLit
//                    | 'foreign' 'import' Ident? '{' (Expr ',')+ '}'
AstRef<AstForeignImportStmt> Parser::parse_foreign_import_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::IMPORT)) {
		return error("Expected 'import'");
	}
	eat(); // Eat 'import'
	AstStringRef ident;
	if (is_kind(TokenKind::IDENTIFIER)) {
		ident = parse_ident();
		if (!ident) {
			return {};
		}
	}
	Array<AstRef<AstExpr>> exprs{temporary_};
	if (is_kind(TokenKind::LBRACE)) {
		eat(); // Eat '{'
		while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
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
		if (!is_kind(TokenKind::RBRACE)) {
			return error("Expected '}'");
		}
		eat(); // Eat '}'
	} else {
		// We expect it to be a string literal
		auto expr = parse_string_expr();
		if (!expr || !exprs.push_back(expr)) {
			return {};
		}
	}

	auto refs = ast_.insert(move(exprs));

	return ast_.create<AstForeignImportStmt>(ident, refs);
}
// IfStmt := TODO(dweiler): EBNF
AstRef<AstIfStmt> Parser::parse_if_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::IF)) {
		return error("Expected 'if'");
	}
	eat(); // Eat 'if'
	AstRef<AstStmt> init;
	AstRef<AstExpr> cond;
	AstRef<AstStmt> on_true;
	AstRef<AstStmt> on_false;

	auto prev_level = this->expr_level_;
	this->expr_level_ = -1;

	auto prev_allow_in_expr = this->allow_in_expr_;
	this->allow_in_expr_ = true;

	if (is_kind(TokenKind::SEMICOLON)) {
		cond = parse_expr(false);
	} else {
		init = {}; // TODO parse_decl_stmt();
		if (is_kind(TokenKind::SEMICOLON)) {
			cond = parse_expr(false);
		} else {
			if (init && ast_[init].is_stmt<AstExprStmt>()) {
				cond = static_cast<const AstExprStmt &>(ast_[init]).expr;
			} else {
				error("Expected a boolean expression");
			}
			init = {};
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
		on_true = parse_stmt(false, {}, {});
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
			on_false = parse_stmt(false, {}, {});
		} else {
			return error("Expected if statement block statement");
		}
	}

	return ast_.create<AstIfStmt>(init, cond, on_true, on_false);
}

// ExprStmt := Expr
AstRef<AstExprStmt> Parser::parse_expr_stmt() {
	auto expr = parse_expr(false);
	if (!expr) {
		return {};
	}
	return ast_.create<AstExprStmt>(expr);
}

// WhenStmt := 'when' Expr BlockStmt ('else' BlockStmt)?
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
	auto on_true = parse_block_stmt();
	if (!on_true) {
		return {};
	}
	AstRef<AstBlockStmt> on_false;
	if (is_keyword(KeywordKind::ELSE)) {
		on_false = parse_block_stmt();
		if (!on_false) {
			return {};
		}
	}
	return ast_.create<AstWhenStmt>(cond, on_true, on_false);
}

Bool Parser::skip_possible_newline_for_literal() {
	if (is_kind(TokenKind::SEMICOLON) && lexer_.string(token_) == "\n") {
		// peek
		eat();
		// TODO
	}
	return false;
}

// DeferStmt := 'defer' Stmt
AstRef<AstDeferStmt> Parser::parse_defer_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::DEFER)) {
		return error("Expected 'defer'");
	}
	eat(); // Eat 'defer'
	auto stmt = parse_stmt(false, {}, {});
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

// ReturnStmt := 'return' (Expr (',' Expr)*)?
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

// UsingStmt := 'using' Expr
AstRef<AstUsingStmt> Parser::parse_using_stmt() {
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	auto expr = parse_expr(false);
	if (!expr) {
		return {};
	}
	return ast_.create<AstUsingStmt>(expr);
}

// IntExpr := IntLit
AstRef<AstIntExpr> Parser::parse_int_expr() {
	TRACE();
	if (!is_literal(LiteralKind::INTEGER)) {
		return error("Expected integer literal");
	}
	auto string = lexer_.string(token_);
	eat(); // Eat literal
	char buf[1024];
	memcpy(buf, string.data(), string.length());
	buf[string.length()] = '\0';
	char* end = nullptr;
	auto value = strtoul(buf, &end, 10);
	if (*end != '\0') {
		return error("Malformed integer literal");
	}
	return ast_.create<AstIntExpr>(value);
}

// FloatExpr := FloatLit
AstRef<AstFloatExpr> Parser::parse_float_expr() {
	TRACE();
	if (!is_literal(LiteralKind::FLOAT)) {
		return error("Expected floating-point literal");
	}
	auto string = lexer_.string(token_);
	eat(); // Eat literal
	char buf[1024];
	memcpy(buf, string.data(), string.length());
	buf[string.length()] = '\0';
	char* end = nullptr;
	auto value = strtod(buf, &end);
	if (*end != '\0') {
		return error("Malformed floating-point literal");
	}
	return ast_.create<AstFloatExpr>(value);
}

// StringExpr := StringLit
AstRef<AstStringExpr> Parser::parse_string_expr() {
	TRACE();
	if (!is_literal(LiteralKind::STRING)) {
		return error("Expected string literal");
	}
	auto strip = lexer_.string(token_);
	strip = strip.slice(1);                     // Remove '"' (or '`')
	strip = strip.truncate(strip.length() - 1); // Remove '"' (or '`')
	auto ref = ast_.insert(strip);
	if (!ref) {
		return {};
	}
	eat(); // Eat literal
	return ast_.create<AstStringExpr>(ref);
}

// IdentExpr := Ident
AstRef<AstIdentExpr> Parser::parse_ident_expr() {
	TRACE();
	if (auto ident = parse_ident()) {
		return ast_.create<AstIdentExpr>(ident);
	}
	return {};
}

// UndefExpr := '---'
AstRef<AstUndefExpr> Parser::parse_undef_expr() {
	TRACE();
	if (!is_kind(TokenKind::UNDEFINED)) {
		return error("Expected '---'");
	}
	eat(); // Eat '---'
	return ast_.create<AstUndefExpr>();
}

// ContextExpr := 'context'
AstRef<AstContextExpr> Parser::parse_context_expr() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTEXT)) {
		return error("Expected 'context'");
	}
	eat(); // Eat 'context'
	return ast_.create<AstContextExpr>();
}

// IfExpr := Expr 'if' Expr 'else' Expr
//         | Expr '?' Expr ':' Expr
AstRef<AstIfExpr> Parser::parse_if_expr(AstRef<AstExpr> expr) {
	AstRef<AstExpr> cond;
	AstRef<AstExpr> on_true;
	AstRef<AstExpr> on_false;
	if (is_keyword(KeywordKind::IF)) {
		on_true = expr;
		eat(); // Eat 'if'
		cond = parse_expr(false);
		if (!cond) {
			return {};
		}
		if (!is_keyword(KeywordKind::ELSE)) {
			return error("Expected 'else' in 'if' expression");
		}
		eat(); // Eat 'else'
		on_false = parse_expr(false);
		if (!on_false) {
			return {};
		}
	} else if (is_operator(OperatorKind::QUESTION)) {
		cond = expr;
		eat(); // Eat '?'
		on_true = parse_expr(false);
		if (!on_true) {
			return {};
		}
		if (!is_operator(OperatorKind::COLON)) {
			return error("Expected ':' after ternary condition");
		}
		eat(); // Eat ':'
		auto on_false = parse_expr(false);
		if (!on_false) {
			return {};
		}
	} else {
		return error("Expected 'if' or '?'");
	}
	return ast_.create<AstIfExpr>(cond, on_true, on_false);
}

// WhenExpr := Expr 'when' Expr 'else' Expr
AstRef<AstWhenExpr> Parser::parse_when_expr(AstRef<AstExpr> on_true) {
	if (!is_keyword(KeywordKind::WHEN)) {
		return error("Expected 'when'");
	}
	eat(); // Eat 'when'
	auto cond = parse_expr(false);
	if (!cond) {
		return {};
	}
	if (!is_keyword(KeywordKind::ELSE)) {
		return error("Expected 'else' in 'when' expression");
	}
	eat(); // Eat 'else'
	auto on_false = parse_expr(false);
	if (!on_false) {
		return {};
	}
	return ast_.create<AstWhenExpr>(cond, on_true, on_false);
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
			// Could be if or when ternary expressions
			if (is_keyword(KeywordKind::IF)) {
				expr = parse_if_expr(expr);
			} else if (is_keyword(KeywordKind::WHEN)) {
				expr = parse_when_expr(expr);
			}
			break;
		}
		if (PREC[Uint32(token_.as_operator)] < prec) {
			// Stop climbing, found the correct precedence.
			break;
		}
		if (is_operator(OperatorKind::QUESTION)) {
			expr = parse_if_expr(expr);
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

AstRef<AstExpr> Parser::parse_operand(Bool lhs) {
	TRACE();
	(void)lhs;
	if (is_literal(LiteralKind::INTEGER)) {
		return parse_int_expr();
	} else if (is_literal(LiteralKind::FLOAT)) {
		return parse_float_expr();
	} else if (is_literal(LiteralKind::STRING)) {
		return parse_string_expr();
	} else if (is_literal(LiteralKind::IMAGINARY)) {
		// TODO
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

// Type := TypeIDType
//       | StructType
//       | UnionType
//       | EnumType
//       | ProcType
//       | PtrType
//       | MultiPtrType
//       | SliceType
//       | ArrayType
//       | DynArrayType
//       | MapType
//       | MatrixType
//       | NamedType
//       | ParamType
//       | ParenType
//       | DistinctType
AstRef<AstType> Parser::parse_type() {
	TRACE();
	if (is_keyword(KeywordKind::TYPEID)) {
		return parse_typeid_type();
	} else if (is_keyword(KeywordKind::UNION)) {
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
		} else if (is_keyword(KeywordKind::DYNAMIC)) {
			return parse_dynarray_type();
		} else {
			return parse_array_type(); // assuming '[' has already been consumed
		}
	} else if (is_keyword(KeywordKind::MAP)) {
		return parse_map_type();
	} else if (is_keyword(KeywordKind::MATRIX)) {
		return parse_matrix_type();
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
	} else if (is_operator(OperatorKind::LPAREN)) {
		return parse_paren_type();
	} else if (is_keyword(KeywordKind::DISTINCT)) {
		return parse_distinct_type();
	}
	return error("Unexpected token while parsing type");
}

// TypeIDType := 'typeid'
AstRef<AstTypeIDType> Parser::parse_typeid_type() {
	TRACE();
	if (!is_keyword(KeywordKind::TYPEID)) {
		return error("Expected 'typeid'");
	}
	eat(); // Eat 'typeid'
	return ast_.create<AstTypeIDType>();
}

// UnionType := 'union' '{' (Type ',')* Type? '}'
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

// EnumType := 'enum' Type? '{' (Enum ',')* Enum? '}'
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

// Enum := Ident ('=' Expr)?
AstRef<AstEnum> Parser::parse_enum() {
	TRACE();
	auto name = parse_ident();
	if (!name) {
		return {};
	}
	AstRef<AstExpr> expr;
	if (is_assignment(AssignKind::EQ)) {
		eat(); // Eat '='
		expr = parse_expr(false);
		if (!expr) {
			return {};
		}
	}
	return ast_.create<AstEnum>(name, expr);
}

// PtrType := '^' Type
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

// MultiPtrType := '[' '^' ']' Type
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

// SliceType := '[' ']' Type
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

// ArrayType := '[' (Expr | '?') ']' Type
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
		return error("Expected ']'");
	}
	eat(); // Eat ']'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstArrayType>(size, base);
}

// DynArrayType := '[' 'dynamic' ']' Type
AstRef<AstDynArrayType> Parser::parse_dynarray_type() {
	TRACE();
	if (!is_keyword(KeywordKind::DYNAMIC)) {
		return error("Expected 'dynamic'");
	}
	eat(); // Eat 'dynamic'
	if (!is_operator(OperatorKind::RBRACKET)) {
		return error("Expected ']'");
	}
	eat(); // Eat ']'
	auto type = parse_type();
	if (!type) {
		return {};
	}
	return ast_.create<AstDynArrayType>(type);
}

// MapType := 'map' [' Type ']' Type
AstRef<AstMapType> Parser::parse_map_type() {
	TRACE();
	if (!is_keyword(KeywordKind::MAP)) {
		return error("Expected 'map'");
	}
	eat(); // Eat 'map'
	if (!is_operator(OperatorKind::LBRACKET)) {
		return error("Expected '[' after 'map'");
	}
	eat(); // Eat '['
	auto kt = parse_type();
	if (!kt) {
		return {};
	}
	if (!is_operator(OperatorKind::RBRACKET)) {
		return error("Expected ']'");
	}
	eat(); // Eat ']'
	auto vt = parse_type();
	if (!vt) {
		return {};
	}
	return ast_.create<AstMapType>(kt, vt);
}

// MatrixType := 'matrix' '[' Expr ',' Expr ']' Type
AstRef<AstMatrixType> Parser::parse_matrix_type() {
	TRACE();
	if (!is_keyword(KeywordKind::MATRIX)) {
		return error("Expected 'matrix'");
	}
	eat(); // Eat 'matrix'
	if (!is_operator(OperatorKind::LBRACKET)) {
		return error("Expected '[' after 'matrix'");
	}
	eat(); // Eat '['
	auto rows = parse_expr(false);
	if (!rows) {
		return {};
	}
	if (!is_kind(TokenKind::COMMA)) {
		return error("Expected ','");
	}
	eat(); // Eat ','
	auto cols = parse_expr(false);
	if (!cols) {
		return {};
	}
	if (!is_operator(OperatorKind::RBRACKET)) {
		return error("Expected ']'");
	}
	eat(); // Eat ']'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstMatrixType>(rows, cols, base);
}

// NamedType := Ident ('.' Ident)?
AstRef<AstNamedType> Parser::parse_named_type() {
	TRACE();
	AstStringRef name_ref = parse_ident();
	if (!name_ref) {
		return {};
	}
	AstStringRef pkg_ref;
	if (is_operator(OperatorKind::PERIOD)) {
		eat(); // Eat '.'
		auto ref = parse_ident();
		if (!pkg_ref) {
			return {};
		}
		pkg_ref = name_ref;
		name_ref = ref;
		if (!pkg_ref) {
			return {};
		}
	}
	return ast_.create<AstNamedType>(pkg_ref, name_ref);
}

// ParenType := '(' Type ')'
AstRef<AstParenType> Parser::parse_paren_type() {
	TRACE();
	if (!is_operator(OperatorKind::LPAREN)) {
		return error("Expected '('");
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

// DistinctType := 'distinct' Type
AstRef<AstDistinctType> Parser::parse_distinct_type() {
	TRACE();
	if (!is_keyword(KeywordKind::DISTINCT)) {
		return error("Expected 'distinct'");
	}
	eat(); // Eat 'distinct'
	auto type = parse_type();
	if (!type) {
		return {};
	}
	return ast_.create<AstDistinctType>(type);
}

Parser::AttributeList Parser::parse_attributes() {
	if (!is_kind(TokenKind::ATTRIBUTE)) {
		return error("Expected '@'");
	}
	eat(); // Eat '@'
	Array<AstRef<AstAttribute>> attrs{temporary_};
	if (is_operator(OperatorKind::LPAREN)) {
		eat(); // Eat '('
		while (!is_operator(OperatorKind::RPAREN) && !is_kind(TokenKind::ENDOF)) {
			auto attr = parse_attribute(true);
			if (!attr || !attrs.push_back(attr)) {
				return {};
			}
			if (is_kind(TokenKind::COMMA)) {
				eat(); // Eat ','
			} else {
				break;
			}
		}
		if (!is_operator(OperatorKind::RPAREN)) {
			error("Expected ')'");
		}
		eat(); // Eat ')'
	} else {
		auto attr = parse_attribute(false);
		if (!attr || !attrs.push_back(attr)) {
			return {};
		}
	}
	return attrs;
}

Parser::DirectiveList Parser::parse_directives() {
	Array<AstRef<AstDirective>> directives{temporary_};
	while (is_kind(TokenKind::DIRECTIVE) && !is_kind(TokenKind::ENDOF)) {
		auto directive = parse_directive();
		if (!directive || !directives.push_back(directive)) {
			return {};
		}
	}
	return directives;
}

// Attribute := Ident ('=' Expr)?
AstRef<AstAttribute> Parser::parse_attribute(Bool allow_expr) {
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	auto ident = parse_ident();
	if (!ident) {
		return {};
	}
	AstRef<AstExpr> expr;
	if (is_assignment(AssignKind::EQ)) {
		if (!allow_expr) {
			return error("Unexpected '='");
		}
		eat(); // Eat '='
		expr = parse_expr(false);
		if (!expr) {
			return error("Could not parse expression");
			return {};
		}
	}
	return ast_.create<AstAttribute>(ident, expr);
}

// Directive := '#' Ident '(' Expr (',' Expr)* ')'
AstRef<AstDirective> Parser::parse_directive() {
	if (!is_kind(TokenKind::DIRECTIVE)) {
		return error("Expected '#'");
	}
	eat(); // Eat '#'
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	auto ident = parse_ident();
	if (!ident) {
		return {};
	}
	AstRefArray<AstExpr> refs;
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
		refs = ast_.insert(move(exprs));
	}
	return ast_.create<AstDirective>(ident, refs);
}

} // namespace Thor

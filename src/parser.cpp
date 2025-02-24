#include <stdlib.h>
#include <string.h> // memcpy

#include "parser.h"
#include "ast.h"
#include "util/allocator.h"

namespace Thor {

struct Debug {
	Debug(System& sys, StringView name, StringView file, Sint32 line) {
		ScratchAllocator<1024> scratch{sys.allocator};
		StringBuilder builder{scratch};
		builder.rep(s_depth*2, ' ');
		builder.put(name);
		builder.put(' ');
		builder.put(file);
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
	auto debug_ ## __LINE__ = Debug{sys_, __func__, __FILE__, __LINE__}

Uint32 Parser::eat() {
	Uint32 offset = 0;
	do {
		offset = token_.offset;
		token_ = lexer_.next();
	} while (is_kind(TokenKind::COMMENT));
	return offset;
}

Maybe<Parser> Parser::open(System& sys, StringView filename) {
	auto lexer = Lexer::open(sys, filename);
	if (!lexer) {
		// Could not open filename
		return {};
	}
	auto file = AstFile::create(sys, filename);
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

AstStringRef Parser::parse_ident(Uint32* poffset) {
	TRACE();
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	auto str = lexer_.string(token_);
	auto offset = eat(); // Eat <ident>
	if (poffset) *poffset = offset;
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
	return ast_.create<AstProcExpr>(ast_[type].offset, type, block);
}

AstRef<AstExpr> Parser::parse_paren_expr() {
	TRACE();
	eat(); // Eat '('
	auto expr = parse_expr(false);
	if (!expr) {
		return {};
	}
	if (!is_operator(OperatorKind::RPAREN)) {
		return error("Expected ')'");
	}
	eat(); // Eat ')'
	return expr;
}

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
//       | AssignStmt ';'
AstRef<AstStmt> Parser::parse_stmt(Bool is_using,
                                   DirectiveList&& directives,
                                   AttributeList&& attributes)
{
	TRACE();
	AstRef<AstStmt> stmt;
	if (is_semi()) {
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
		// Try to parse an expression
		auto expr = parse_expr(false);
		if (!expr) {
			return {};
		}

		// If the expression is proceeded immediately by a newline or semicolon then
		// this is just an expression in a statement context, thus it's an ExprStmt.
		if (is_semi()) {
			return ast_.create<AstExprStmt>(ast_[expr].offset, expr);
		}

		Array<AstRef<AstExpr>> lhs{temporary_};
		Array<AstRef<AstExpr>> rhs{temporary_};
		AstRef<AstType> type; // Optional type
		if (!lhs.push_back(expr)) {
			return {};
		}

		// There could be multiple values on the left-hand side of a DeclStmt.
		while (is_kind(TokenKind::COMMA) && !is_kind(TokenKind::ENDOF)) {
			eat(); // Eat ','
			auto ref = parse_expr(false);
			if (!ref || !lhs.push_back(ref)) {
				return {};
			}
		}

		// When the left-hand side is proceeded by a colon then everything on the
		// left-hand side is an IdentExpr and an optional Type is expected after
		// the colon.
		enum class DeclKind : Uint8 {
			NONE,
			MUTABLE,
			IMMUTABLE
		} decl = DeclKind::NONE;
		if (is_operator(OperatorKind::COLON)) {
			eat(); // Eat ':'
			decl = DeclKind::MUTABLE;
		}

		if (!is_operator(OperatorKind::COLON) && !is_kind(TokenKind::ASSIGNMENT)) {
			type = parse_type();
			if (!type) {
				return {};
			}
		}

		// When proceeded by a ':' or '=' then we're parsing an initializer. The
		// initializer can also be a list of initializers (comma separated).
		if (is_operator(OperatorKind::COLON)) {
			decl = DeclKind::IMMUTABLE;
		}
		AssignKind assign = AssignKind::EQ;
		if (is_kind(TokenKind::ASSIGNMENT)) {
			assign = token_.as_assign;
		}
		if (decl == DeclKind::IMMUTABLE || is_kind(TokenKind::ASSIGNMENT)) {
			eat(); // Eat ':' or '='
			auto init = parse_expr(false);
			if (!init || !rhs.push_back(init)) {
				return {};
			}
			while (is_kind(TokenKind::COMMA) && !is_kind(TokenKind::ENDOF)) {
				eat(); // Eat ','
				auto ref = parse_expr(false); // assignment
				if (!ref || !rhs.push_back(ref)) {
					return {};
				}
			}
		}

		AstRefArray<AstExpr> lhs_refs = ast_.insert(move(lhs));
		AstRefArray<AstExpr> rhs_refs;
		if (!rhs.is_empty()) {
			rhs_refs = ast_.insert(move(rhs));
		}
		const auto offset = ast_[expr].offset;
		if (decl == DeclKind::NONE) {
			stmt = ast_.create<AstAssignStmt>(offset, lhs_refs, rhs_refs, assign);
		} else {
			AstRefArray<AstDirective> d_refs;
			AstRefArray<AstAttribute> a_refs;
			if (directives) d_refs = ast_.insert(move(*directives));
			if (attributes) a_refs = ast_.insert(move(*attributes));
			stmt = ast_.create<AstDeclStmt>(offset,
			                                decl == DeclKind::IMMUTABLE,
			                                is_using,
			                                lhs_refs,
			                                type,
			                                rhs_refs,
			                                d_refs,
			                                a_refs);
		}
	}
	if (!stmt) {
		return {};
	}
	if (!is_semi()) {
		return error("Expected ';' or newline after statement");
	}
	eat(); // Eat ';'
	return stmt;
}

// EmptyStmt := ';'
AstRef<AstEmptyStmt> Parser::parse_empty_stmt() {
	TRACE();
	if (!is_semi()) {
		return error("Expected ';' (or newline)");
	}
	auto offset = eat(); // Eat ';'
	return ast_.create<AstEmptyStmt>(offset);
}

// BlockStmt := '{' Stmt* '}'
AstRef<AstBlockStmt> Parser::parse_block_stmt() {
	TRACE();
	if (!is_kind(TokenKind::LBRACE)) {
		error("Expected '{'");
		return {};
	}
	auto offset = eat(); // Eat '{'
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
	return ast_.create<AstBlockStmt>(offset, refs);
}

// PackageStmt := 'package' Ident
AstRef<AstPackageStmt> Parser::parse_package_stmt() {
	TRACE();
	auto offset = eat(); // Eat 'package'
	if (is_kind(TokenKind::IDENTIFIER)) {
		if (auto name = parse_ident()) {
			return ast_.create<AstPackageStmt>(offset, name);
		} else {
			return {};
		}
	}
	return error("Expected identifier for package");
}

// ImportStmt := 'import' Ident? StringLit
AstRef<AstImportStmt> Parser::parse_import_stmt() {
	TRACE();
	auto offset = eat(); // Eat 'import'
	AstStringRef alias;
	if (is_kind(TokenKind::IDENTIFIER)) {
		alias = parse_ident();
	}
	auto expr = parse_string_expr();
	if (!expr) {
		return {};
	}
	return ast_.create<AstImportStmt>(offset, alias, expr);
}

// BreakStmt := 'break' Ident?
AstRef<AstBreakStmt> Parser::parse_break_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::BREAK)) {
		return error("Expected 'break'");
	}
	auto offset = eat(); // Eat 'break'
	AstStringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = parse_ident();
	}
	return ast_.create<AstBreakStmt>(offset, label);
}

// ContinueStmt := 'continue' Ident?
AstRef<AstContinueStmt> Parser::parse_continue_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTINUE)) {
		return error("Expected 'continue'");
	}
	auto offset = eat(); // Eat 'continue'
	AstStringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = parse_ident();
	}
	return ast_.create<AstContinueStmt>(offset, label);
}

// FallthroughStmt := 'fallthrough'
AstRef<AstFallthroughStmt> Parser::parse_fallthrough_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::FALLTHROUGH)) {
		return error("Expected 'fallthrough'");
	}
	auto offset = eat(); // Eat 'fallthrough'
	return ast_.create<AstFallthroughStmt>(offset);
}

// ForeignImportStmt := 'foreign' 'import' Ident? StringLit
//                    | 'foreign' 'import' Ident? '{' (Expr ',')+ '}'
AstRef<AstForeignImportStmt> Parser::parse_foreign_import_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::IMPORT)) {
		return error("Expected 'import'");
	}
	auto offset = eat(); // Eat 'import'
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

	return ast_.create<AstForeignImportStmt>(offset, ident, refs);
}

// IfStmt := 'if' ';' Expr?         (BlockStmt | DoStmt) ('else' (BlockStmt | DoStmt))?
//         | 'if' DeclStmt ';' Expr (BlockStmt | DoStmt) ('else' (BlockStmt | DoStmt))?
//         | 'if' ExprStmt          (BlockStmt | DoStmt) ('else' (BlockStmt | DoStmt))?
AstRef<AstIfStmt> Parser::parse_if_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::IF)) {
		return error("Expected 'if'");
	}

	auto offset = eat(); // Eat 'if'

	AstRef<AstStmt> init;
	AstRef<AstExpr> cond;
	AstRef<AstStmt> on_true;
	AstRef<AstStmt> on_false;

	auto prev_level = this->expr_level_;
	this->expr_level_ = -1;

	auto prev_allow_in_expr = this->allow_in_expr_;
	this->allow_in_expr_ = true;

	if (is_kind(TokenKind::EXPLICITSEMI)) {
		cond = parse_expr(false);
	} else {
		init = parse_stmt(false, {}, {});
		if (!ast_[init].is_stmt<AstDeclStmt>()) {
			return error("Expected a declaration for 'init' statement in 'if'");
		}
		if (is_kind(TokenKind::EXPLICITSEMI)) {
			cond = parse_expr(false);
		} else if (init) {
			auto& node = ast_[init];
			if (node.is_stmt<AstExprStmt>()) {
				cond = static_cast<const AstExprStmt &>(node).expr;
			} else {
				*(volatile int *)0 = 0;
				return error("Expected expression statement");
			}
			init = {};
		} else {
			return error("Expected a boolean expression");
		}
	}

	this->expr_level_    = prev_level;
	this->allow_in_expr_ = prev_allow_in_expr;

	if (!cond.is_valid()) {
		return error("Expected a condition for if statement");
	}

	if (is_keyword(KeywordKind::DO)) {
		eat(); // Eat 'do'
		on_true = parse_stmt(false, {}, {});
	} else {
		on_true = parse_block_stmt();
	}
	if (!on_true) {
		return {};
	}

	if (is_kind(TokenKind::IMPLICITSEMI)) {
		eat();
	}

	if (is_keyword(KeywordKind::ELSE)) {
		// Token else_tok = token_;
		eat();
		if (is_keyword(KeywordKind::IF)) {
			on_false = parse_if_stmt();
		} else if (is_kind(TokenKind::LBRACE)) {
			on_false = parse_block_stmt();
		} else if (is_keyword(KeywordKind::DO)) {
			eat(); // Eat 'do'
			on_false = parse_stmt(false, {}, {});
		} else {
			return error("Expected if statement block statement");
		}
		if (!on_false) {
			return {};
		}
	}

	return ast_.create<AstIfStmt>(offset, init, cond, on_true, on_false);
}

// ExprStmt := Expr
AstRef<AstExprStmt> Parser::parse_expr_stmt() {
	TRACE();
	auto expr = parse_expr(false);
	if (!expr) {
		return {};
	}
	return ast_.create<AstExprStmt>(ast_[expr].offset, expr);
}

// WhenStmt := 'when' Expr BlockStmt ('else' BlockStmt)?
AstRef<AstWhenStmt> Parser::parse_when_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::WHEN)) {
		return error("Expected 'when'");
	}
	auto offset = eat(); // Eat 'when'
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
	return ast_.create<AstWhenStmt>(offset, cond, on_true, on_false);
}

// DeferStmt := 'defer' Stmt
AstRef<AstDeferStmt> Parser::parse_defer_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::DEFER)) {
		return error("Expected 'defer'");
	}
	auto offset = eat(); // Eat 'defer'
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
	return ast_.create<AstDeferStmt>(offset, stmt);
}

// ReturnStmt := 'return' (Expr (',' Expr)*)?
AstRef<AstReturnStmt> Parser::parse_return_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::RETURN)) {
		return error("Expected 'return'");
	}
	auto offset = eat(); // Eat 'return'
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
	return ast_.create<AstReturnStmt>(offset, refs);
}

// UsingStmt := 'using' Expr
AstRef<AstUsingStmt> Parser::parse_using_stmt() {
	TRACE();
	if (!is_kind(TokenKind::IDENTIFIER)) {
		return error("Expected identifier");
	}
	auto expr = parse_expr(false);
	if (!expr) {
		return {};
	}
	return ast_.create<AstUsingStmt>(ast_[expr].offset, expr);
}

// Expr := IfExpr
//       | WhenExpr
//       | IntExpr
//       | IdentExpr
//       | UndefExpr
//       | ContextExpr
//       | ProcExpr
//       | RangeExpr
//       | SliceRangeExpr
//       | IntExpr
//       | FloatExpr
//       | StringExpr
//       | CastExpr
//       | TypeExpr

// IntExpr := IntLit
AstRef<AstIntExpr> Parser::parse_int_expr() {
	TRACE();
	if (!is_literal(LiteralKind::INTEGER)) {
		return error("Expected integer literal");
	}
	auto string = lexer_.string(token_);
	char buf[1024];
	memcpy(buf, string.data(), string.length());
	buf[string.length()] = '\0';
	char* end = nullptr;
	auto value = strtoul(buf, &end, 10);
	if (*end != '\0') {
		return error("Malformed integer literal");
	}
	auto offset = eat(); // Eat literal
	return ast_.create<AstIntExpr>(offset, value);
}

// FloatExpr := FloatLit
AstRef<AstFloatExpr> Parser::parse_float_expr() {
	TRACE();
	if (!is_literal(LiteralKind::FLOAT)) {
		return error("Expected floating-point literal");
	}
	auto string = lexer_.string(token_);
	char buf[1024];
	memcpy(buf, string.data(), string.length());
	buf[string.length()] = '\0';
	char* end = nullptr;
	auto value = strtod(buf, &end);
	if (*end != '\0') {
		return error("Malformed floating-point literal");
	}
	auto offset = eat(); // Eat literal
	return ast_.create<AstFloatExpr>(offset, value);
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
	auto offset = eat(); // Eat literal
	return ast_.create<AstStringExpr>(offset, ref);
}

// ImaginaryExpr := ImaginaryLit
AstRef<AstImaginaryExpr> Parser::parse_imaginary_expr() {
	TRACE();
	if (!is_literal(LiteralKind::IMAGINARY)) {
		return error("Expected imaginary literal");
	}
	auto strip = lexer_.string(token_);
	strip = strip.truncate(strip.length() - 1); // Remove trailing 'i', 'j', or 'k'
	char buf[1024];
	memcpy(buf, strip.data(), strip.length());
	buf[strip.length()] = '\0';
	char* end = nullptr;
	auto value = strtod(buf, &end);
	if (*end != '\0') {
		return error("Malformed imaginary literal");
	}
	auto offset = eat(); // Eat literal
	return ast_.create<AstImaginaryExpr>(offset, value);
}

// CompoundExpr := '{' Field (',' Field)* '}'
AstRef<AstCompoundExpr> Parser::parse_compound_expr() {
	TRACE();
	auto offset = eat(); // Eat '{'
	if (is_kind(TokenKind::IMPLICITSEMI)) {
		eat(); // Eat ';'
	}
	Array<AstRef<AstField>> fields{temporary_};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto field = parse_field(true);
		if (!field || !fields.push_back(field)) {
			return {};
		}
		if (is_kind(TokenKind::COMMA)) {
			eat(); // Eat ','
		} else {
			break;
		}
	}
	if (is_kind(TokenKind::IMPLICITSEMI)) {
		eat();
	}
	if (!is_kind(TokenKind::RBRACE)) {
		return error("Expected ',' or '}'");
	}
	eat(); // Eat '}'
	auto refs = ast_.insert(move(fields));
	return ast_.create<AstCompoundExpr>(offset, refs);
}

// IdentExpr := Ident
AstRef<AstIdentExpr> Parser::parse_ident_expr() {
	TRACE();
	Uint32 offset = 0;
	if (auto ident = parse_ident(&offset)) {
		return ast_.create<AstIdentExpr>(offset, ident);
	}
	return {};
}

// UndefExpr := '---'
AstRef<AstUndefExpr> Parser::parse_undef_expr() {
	TRACE();
	if (!is_kind(TokenKind::UNDEFINED)) {
		return error("Expected '---'");
	}
	auto offset = eat(); // Eat '---'
	return ast_.create<AstUndefExpr>(offset);
}

// ContextExpr := 'context'
AstRef<AstContextExpr> Parser::parse_context_expr() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTEXT)) {
		return error("Expected 'context'");
	}
	auto offset = eat(); // Eat 'context'
	return ast_.create<AstContextExpr>(offset);
}

// IfExpr := Expr 'if' Expr 'else' Expr
//         | Expr '?' Expr ':' Expr
AstRef<AstIfExpr> Parser::parse_if_expr(AstRef<AstExpr> expr) {
	TRACE();
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
	return ast_.create<AstIfExpr>(ast_[expr].offset, cond, on_true, on_false);
}

// WhenExpr := Expr 'when' Expr 'else' Expr
AstRef<AstWhenExpr> Parser::parse_when_expr(AstRef<AstExpr> on_true) {
	TRACE();
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
	return ast_.create<AstWhenExpr>(ast_[on_true].offset, cond, on_true, on_false);
}

AstRef<AstExpr> Parser::parse_expr(Bool lhs) {
	TRACE();
	auto expr = parse_bin_expr(lhs, 1);
	if (!expr) {
		return {};
	}
	return expr;
}

AstRef<AstExpr> Parser::parse_value(Bool lhs) {
	TRACE();
	if (is_kind(TokenKind::LBRACE)) {
		return parse_compound_expr();
	}
	return parse_expr(lhs);
}

AstRef<AstExpr> Parser::parse_bin_expr(Bool lhs, Uint32 prec) {
	TRACE();
	static constexpr const Uint32 PREC[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) PREC,
		#include "lexer.inl"
	};
	auto expr = parse_unary_expr(lhs);
	if (!expr) {
		return {};
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
			expr = ast_.create<AstBinExpr>(ast_[expr].offset, expr, rhs, kind);
		}
		lhs = false;
	}
	return expr;
}

// DerefExpr := Expr '^'
AstRef<AstDerefExpr> Parser::parse_deref_expr(AstRef<AstExpr> operand) {
	if (!is_operator(OperatorKind::POINTER)) {
		return error("Expected '^'");
	}
	eat(); // Eat '^'
	return ast_.create<AstDerefExpr>(ast_[operand].offset, operand);
}

// OrReturnExpr := Expr 'or_return'
AstRef<AstOrReturnExpr> Parser::parse_or_return_expr(AstRef<AstExpr> operand) {
	if (!is_operator(OperatorKind::OR_RETURN)) {
		return error("Expected 'or_return'");
	}
	eat(); // Eat 'or_return'
	return ast_.create<AstOrReturnExpr>(ast_[operand].offset, operand);
}

// OrBreakExpr := Expr 'or_break'
AstRef<AstOrBreakExpr> Parser::parse_or_break_expr(AstRef<AstExpr> operand) {
	if (!is_operator(OperatorKind::OR_BREAK)) {
		return error("Expected 'or_break'");
	}
	eat(); // Eat 'or_break'
	return ast_.create<AstOrBreakExpr>(ast_[operand].offset, operand);
}

// OrContinueExpr := Expr 'or_continue'
AstRef<AstOrContinueExpr> Parser::parse_or_continue_expr(AstRef<AstExpr> operand) {
	if (!is_operator(OperatorKind::OR_CONTINUE)) {
		return error("Expected 'or_continue'");
	}
	eat(); // Eat 'or_continue'
	return ast_.create<AstOrContinueExpr>(ast_[operand].offset, operand);
}

// CallExpr := Expr '(' (',' Field)* ')'
AstRef<AstCallExpr> Parser::parse_call_expr(AstRef<AstExpr> operand) {
	TRACE();
	if (!is_operator(OperatorKind::LPAREN)) {
		return error("Expected '('");
	}
	eat(); // Eat '('
	Array<AstRef<AstField>> args{temporary_};
	while (!is_operator(OperatorKind::RPAREN) && !is_kind(TokenKind::ENDOF)) {
		auto field = parse_field(true);
		if (!field) {
			return {};
		}
		const auto& node = ast_[field];
		if (node.expr && !ast_[node.operand].is_expr<AstIdentExpr>()) {
			return error(ast_[node.operand].offset, "Expected identifier when assigning parameter by name");
		}
		if (!args.push_back(field)) {
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
	auto refs = ast_.insert(move(args));
	return ast_.create<AstCallExpr>(ast_[operand].offset, operand, refs);
}

AstRef<AstExpr> Parser::parse_unary_atom(AstRef<AstExpr> operand, Bool is_lhs) {
	TRACE();
	if (!operand) {
		return {};
	}
	for (;;) {
		if (is_operator(OperatorKind::LPAREN)) {
			operand = parse_call_expr(operand);
		} else if (is_operator(OperatorKind::PERIOD)) {
			eat(); // Eat '.'
			if (is_kind(TokenKind::IDENTIFIER)) {
				auto name = parse_ident();
				if (!name) {
					return {};
				}
				operand = ast_.create<AstAccessExpr>(ast_[operand].offset, operand, name, false);
			} else if (is_operator(OperatorKind::LPAREN)) {
				eat(); // Eat '('
				auto type = parse_type();
				if (!type) {
					return {};
				}
				if (!is_operator(OperatorKind::RPAREN)) {
					return error("Expected ')'");
				}
				eat(); // Eat ')'
				operand = ast_.create<AstAssertExpr>(ast_[operand].offset, operand, type);
			} else if (is_operator(OperatorKind::QUESTION)) {
				eat(); // Eat '?'
				operand = ast_.create<AstAssertExpr>(ast_[operand].offset, operand, AstRef<AstType>{});
			} else {
				return error("Unexpected token after '.'");
			}
		} else if (is_operator(OperatorKind::ARROW)) {
			eat(); // Eat '->'
			auto ident = parse_ident();
			if (!ident) {
				return {};
			}
			operand = ast_.create<AstAccessExpr>(ast_[operand].offset, operand, ident, true);
		} else if (is_operator(OperatorKind::LBRACKET)) {
			eat(); // Eat '['
			if (is_operator(OperatorKind::RBRACKET)) {
				return error("Expected expression in '[]'");
			}
			// We don't know if we need the expression yet
			AstRef<AstExpr> lhs = parse_expr(is_lhs);
			AstRef<AstExpr> rhs;
			Bool is_slice = is_operator(OperatorKind::COLON);
			if (is_slice || is_kind(TokenKind::COMMA)) {
				eat(); // Eat ':' or ','
				rhs = parse_expr(false);
			}
			if (!is_operator(OperatorKind::RBRACKET)) {
				return error("Expected ']'");
			}
			eat(); // Eat ']'
			if (is_slice) {
				operand = ast_.create<AstSliceExpr>(ast_[operand].offset, operand, lhs, rhs);
			} else {
				operand = ast_.create<AstIndexExpr>(ast_[operand].offset, operand, lhs, rhs);
			}
		} else if (is_operator(OperatorKind::POINTER)) {
			return parse_deref_expr(operand);
		} else if (is_operator(OperatorKind::OR_RETURN)) {
			return parse_or_return_expr(operand);
		} else if (is_operator(OperatorKind::OR_BREAK)) {
			return parse_or_break_expr(operand);
		} else if (is_operator(OperatorKind::OR_CONTINUE)) {
			return parse_or_continue_expr(operand);
		} else {
			break;
		}
		is_lhs = false;
	}
	return operand;
}

AstRef<AstExpr> Parser::parse_unary_expr(Bool lhs) {
	TRACE();
	if (is_operator(OperatorKind::TRANSMUTE) ||
	    is_operator(OperatorKind::CAST))
	{
		auto offset = eat(); // Eat 'transmute' or 'cast'
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
			return ast_.create<AstCastExpr>(offset, type, expr);
		}
	} else if (is_operator(OperatorKind::AUTO_CAST)) {
		auto offset = eat(); // Eat 'auto_cast'
		if (auto expr = parse_unary_expr(lhs)) {
			return ast_.create<AstCastExpr>(offset, AstRef<AstType>{}, expr);
		}
	} else if (is_operator(OperatorKind::ADD)  ||
	           is_operator(OperatorKind::SUB)  ||
	           is_operator(OperatorKind::XOR)  ||
	           is_operator(OperatorKind::BAND) ||
	           is_operator(OperatorKind::LNOT) ||
	           is_operator(OperatorKind::MUL))
	{
		const auto op = token_.as_operator;
		auto offset = eat(); // Eat op
		if (auto expr = parse_unary_expr(lhs)) {
			return ast_.create<AstUnaryExpr>(offset, expr, op);
		}
	} else if (is_operator(OperatorKind::PERIOD)) {
		auto offset = eat(); // Eat '.'
		if (auto ident = parse_ident()) {
			return ast_.create<AstSelectorExpr>(offset, ident);
		} else {
			return error("Expected identifier after '.'");
		}
	}
	auto operand = parse_operand(lhs);
	return parse_unary_atom(operand, lhs);
}

AstRef<AstExpr> Parser::parse_operand(Bool is_lhs) {
	TRACE();
	if (is_literal(LiteralKind::INTEGER)) {
		return parse_int_expr();
	} else if (is_literal(LiteralKind::FLOAT)) {
		return parse_float_expr();
	} else if (is_literal(LiteralKind::STRING)) {
		return parse_string_expr();
	} else if (is_literal(LiteralKind::IMAGINARY)) {
		return parse_imaginary_expr();
	} else if (is_kind(TokenKind::IDENTIFIER)) {
		return parse_ident_expr();
	} else if (is_kind(TokenKind::UNDEFINED)) {
		return parse_undef_expr();
	} else if (is_keyword(KeywordKind::CONTEXT)) {
		return parse_context_expr();
	} else if (is_keyword(KeywordKind::PROC)) {
		return parse_proc_expr();
	} else if (is_operator(OperatorKind::LPAREN)) {
		return parse_paren_expr();
	} else if (is_kind(TokenKind::LBRACE)) {
		if (!is_lhs) {
			return parse_compound_expr();
		}
	}
	auto type = parse_type();
	if (!type) {
		return {};
	}
	return ast_.create<AstTypeExpr>(ast_[type].offset, type);
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
//       | BitsetType
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
		auto offset = eat(); // Eat '['
		if (is_operator(OperatorKind::POINTER)) {
			return parse_multiptr_type(offset); // assuming '[' has already been consumed
		} else if (is_operator(OperatorKind::RBRACKET)) {
			return parse_slice_type(offset); // assuming '[' has already been consumed
		} else if (is_keyword(KeywordKind::DYNAMIC)) {
			return parse_dynarray_type(offset);
		} else {
			return parse_array_type(offset); // assuming '[' has already been consumed
		}
	} else if (is_keyword(KeywordKind::MAP)) {
		return parse_map_type();
	} else if (is_keyword(KeywordKind::MATRIX)) {
		return parse_matrix_type();
	} else if (is_keyword(KeywordKind::BITSET)) {
		return parse_bitset_type();
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
			return ast_.create<AstParamType>(ast_[named].offset, named, refs);
		} else {
			return named;
		}
	} else if (is_operator(OperatorKind::LPAREN)) {
		return parse_paren_type();
	} else if (is_keyword(KeywordKind::DISTINCT)) {
		return parse_distinct_type();
	}
	return {};
}

// TypeIDType := 'typeid'
AstRef<AstTypeIDType> Parser::parse_typeid_type() {
	TRACE();
	if (!is_keyword(KeywordKind::TYPEID)) {
		return error("Expected 'typeid'");
	}
	auto offset = eat(); // Eat 'typeid'
	return ast_.create<AstTypeIDType>(offset);
}

// UnionType := 'union' '{' (Type ',')* Type? '}'
AstRef<AstUnionType> Parser::parse_union_type() {
	TRACE();
	if (!is_keyword(KeywordKind::UNION)) {
		return error("Expected 'union'");
	}
	auto offset = eat(); // Eat 'union'
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
	return ast_.create<AstUnionType>(offset, refs);
}

// EnumType := 'enum' Type? '{' (Enum ',')* Enum? '}'
AstRef<AstEnumType> Parser::parse_enum_type() {
	TRACE();
	if (!is_keyword(KeywordKind::ENUM)) {
		return error("Expected 'enum'");
	}
	auto offset = eat(); // Eat 'enum'
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
	Array<AstRef<AstField>> enums{temporary_};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto field = parse_field(true);
		if (!field) {
			break;
		}
		if (!enums.push_back(field)) {
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
	return ast_.create<AstEnumType>(offset, base, refs);
}

// Field := Expr ('=' Expr)?
AstRef<AstField> Parser::parse_field(Bool allow_assignment) {
	TRACE();
	auto operand = parse_value(false);
	if (!operand) {
		return {};
	}
	AstRef<AstExpr> expr;
	if (is_assignment(AssignKind::EQ)) {
		if (!allow_assignment) {
			return error("Unexpected '='");
		}
		eat(); // Eat '='
		expr = parse_value(false);
		if (!expr) {
			return error("Could not parse expression");
			return {};
		}
	}
	return ast_.create<AstField>(ast_[operand].offset, operand, expr);
}

// PtrType := '^' Type
AstRef<AstPtrType> Parser::parse_ptr_type() {
	TRACE();
	if (!is_operator(OperatorKind::POINTER)) {
		return error("Expected '^'");
	}
	auto offset = eat(); // Eat '^'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstPtrType>(offset, base);
}

// MultiPtrType := '[' '^' ']' Type
AstRef<AstMultiPtrType> Parser::parse_multiptr_type(Uint32 offset) {
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
	return ast_.create<AstMultiPtrType>(offset, base);
}

// SliceType := '[' ']' Type
AstRef<AstSliceType> Parser::parse_slice_type(Uint32 offset) {
	TRACE();
	if (!is_operator(OperatorKind::RBRACKET)) {
		return error("Expected ']'");
	}
	eat(); // Eat ']'
	auto base = parse_type();
	if (!base) {
		return {};
	}
	return ast_.create<AstSliceType>(offset, base);
}

// ArrayType := '[' (Expr | '?') ']' Type
AstRef<AstArrayType> Parser::parse_array_type(Uint32 offset) {
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
	return ast_.create<AstArrayType>(offset, size, base);
}

// DynArrayType := '[' 'dynamic' ']' Type
AstRef<AstDynArrayType> Parser::parse_dynarray_type(Uint32 offset) {
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
	return ast_.create<AstDynArrayType>(offset, type);
}

// MapType := 'map' [' Type ']' Type
AstRef<AstMapType> Parser::parse_map_type() {
	TRACE();
	if (!is_keyword(KeywordKind::MAP)) {
		return error("Expected 'map'");
	}
	auto offset = eat(); // Eat 'map'
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
	return ast_.create<AstMapType>(offset, kt, vt);
}

// MatrixType := 'matrix' '[' Expr ',' Expr ']' Type
AstRef<AstMatrixType> Parser::parse_matrix_type() {
	TRACE();
	if (!is_keyword(KeywordKind::MATRIX)) {
		return error("Expected 'matrix'");
	}
	auto offset = eat(); // Eat 'matrix'
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
	return ast_.create<AstMatrixType>(offset, rows, cols, base);
}

// BitsetType := 'bit_set' '[' Expr (';' Type)? ']'
AstRef<AstBitsetType> Parser::parse_bitset_type() {
	if (!is_keyword(KeywordKind::BITSET)) {
		return error("Expected 'bitset'");
	}
	auto offset = eat(); // Eat 'bitset'
	if (!is_operator(OperatorKind::LBRACKET)) {
		return error("Expected '[' after 'bitset'");
	}
	eat(); // Eat '['
	auto expr = parse_expr(false);
	if (!expr) {
		return {};
	}
	AstRef<AstType> type;
	if (is_kind(TokenKind::EXPLICITSEMI)) {
		eat(); // Eat ';'
		type = parse_type();
		if (!type) {
			return {};
		}
	}
	return ast_.create<AstBitsetType>(offset, expr, type);
}

// NamedType := Ident ('.' Ident)?
AstRef<AstNamedType> Parser::parse_named_type() {
	TRACE();
	Uint32 offset = 0;
	AstStringRef name_ref = parse_ident(&offset);
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
	return ast_.create<AstNamedType>(offset, pkg_ref, name_ref);
}

// ParenType := '(' Type ')'
AstRef<AstParenType> Parser::parse_paren_type() {
	TRACE();
	if (!is_operator(OperatorKind::LPAREN)) {
		return error("Expected '('");
	}
	auto offset = eat(); // Eat '('
	auto type = parse_type();
	if (!type) {
		return {};
	}
	if (!is_operator(OperatorKind::RPAREN)) {
		return error("Expected ')'");
	}
	eat(); // Eat ')'
	return ast_.create<AstParenType>(offset, type);
}

// DistinctType := 'distinct' Type
AstRef<AstDistinctType> Parser::parse_distinct_type() {
	TRACE();
	if (!is_keyword(KeywordKind::DISTINCT)) {
		return error("Expected 'distinct'");
	}
	auto offset = eat(); // Eat 'distinct'
	auto type = parse_type();
	if (!type) {
		return {};
	}
	return ast_.create<AstDistinctType>(offset, type);
}

Parser::AttributeList Parser::parse_attributes() {
	TRACE();
	if (!is_kind(TokenKind::ATTRIBUTE)) {
		return error("Expected '@'");
	}
	eat(); // Eat '@'
	Array<AstRef<AstField>> attrs{temporary_};
	if (is_operator(OperatorKind::LPAREN)) {
		eat(); // Eat '('
		while (!is_operator(OperatorKind::RPAREN) && !is_kind(TokenKind::ENDOF)) {
			auto field = parse_field(true);
			if (!field || !attrs.push_back(field)) {
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
		auto field = parse_field(false);
		if (!field || !attrs.push_back(field)) {
			return {};
		}
	}
	return attrs;
}

Parser::DirectiveList Parser::parse_directives() {
	TRACE();
	Array<AstRef<AstDirective>> directives{temporary_};
	while (is_kind(TokenKind::DIRECTIVE) && !is_kind(TokenKind::ENDOF)) {
		auto directive = parse_directive();
		if (!directive || !directives.push_back(directive)) {
			return {};
		}
	}
	return directives;
}

// Directive := '#' Ident '(' Expr (',' Expr)* ')'
AstRef<AstDirective> Parser::parse_directive() {
	TRACE();
	if (!is_kind(TokenKind::DIRECTIVE)) {
		return error("Expected '#'");
	}
	auto offset = eat(); // Eat '#'
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
	return ast_.create<AstDirective>(offset, ident, refs);
}

} // namespace Thor

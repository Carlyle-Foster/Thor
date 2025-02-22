#ifndef THOR_PARSER_H
#define THOR_PARSER_H
#include "lexer.h"
#include "ast.h"
#include "util/system.h"

namespace Thor {

struct Parser {
	static Maybe<Parser> open(System& sys, StringView file);
	AstStringRef parse_ident();

	using DirectiveList = Maybe<Array<AstRef<AstDirective>>>;
	using AttributeList = Maybe<Array<AstRef<AstAttribute>>>;

	// Expression parsers
	AstRef<AstExpr>       parse_expr(Bool lhs);
	AstRef<AstExpr>       parse_operand();
	AstRef<AstExpr>       parse_bin_expr(Bool lhs, Uint32 prec);
	AstRef<AstExpr>       parse_unary_expr(Bool lhs);
	AstRef<AstExpr>       parse_operand(Bool lhs); // Operand parser for AstBinExpr or AstUnaryExpr

	AstRef<AstIntExpr> parse_int_expr();
	AstRef<AstFloatExpr> parse_float_expr();
	AstRef<AstStringExpr> parse_string_expr();
	AstRef<AstProcExpr> parse_proc_expr();
	AstRef<AstIdentExpr> parse_ident_expr();
	AstRef<AstUndefExpr> parse_undef_expr();
	AstRef<AstContextExpr> parse_context_expr();
	AstRef<AstIfExpr> parse_if_expr(AstRef<AstExpr> expr);
	AstRef<AstWhenExpr> parse_when_expr(AstRef<AstExpr> on_true);

	// Statement parsers
	AstRef<AstStmt> parse_stmt(Bool use, DirectiveList&& directives, AttributeList&& attributes);
	AstRef<AstExprStmt> parse_expr_stmt();
	AstRef<AstEmptyStmt> parse_empty_stmt();
	AstRef<AstBlockStmt> parse_block_stmt();
	AstRef<AstPackageStmt> parse_package_stmt();
	AstRef<AstImportStmt> parse_import_stmt();
	AstRef<AstBreakStmt> parse_break_stmt();
	AstRef<AstContinueStmt> parse_continue_stmt();
	AstRef<AstFallthroughStmt> parse_fallthrough_stmt();
	AstRef<AstForeignImportStmt> parse_foreign_import_stmt();
	AstRef<AstIfStmt> parse_if_stmt();
	AstRef<AstWhenStmt> parse_when_stmt();
	AstRef<AstDeferStmt> parse_defer_stmt();
	AstRef<AstReturnStmt> parse_return_stmt();
	AstRef<AstUsingStmt> parse_using_stmt();

	// Type parsers
	AstRef<AstType> parse_type();
	AstRef<AstTypeIDType> parse_typeid_type();
	AstRef<AstUnionType> parse_union_type();
	AstRef<AstEnumType> parse_enum_type();
	AstRef<AstPtrType> parse_ptr_type();
	AstRef<AstMultiPtrType> parse_multiptr_type();
	AstRef<AstSliceType> parse_slice_type();
	AstRef<AstArrayType> parse_array_type();
	AstRef<AstDynArrayType> parse_dynarray_type();
	AstRef<AstMapType> parse_map_type();
	AstRef<AstMatrixType> parse_matrix_type();
	AstRef<AstNamedType> parse_named_type();
	AstRef<AstParenType> parse_paren_type();
	AstRef<AstDistinctType> parse_distinct_type();

	// Misc parsers
	AttributeList parse_attributes();
	DirectiveList parse_directives();

	[[nodiscard]] constexpr AstFile& ast() { return ast_; }
	[[nodiscard]] constexpr const AstFile& ast() const { return ast_; }
private:
	AstRef<AstExpr> parse_unary_atom(AstRef<AstExpr> operand, Bool lhs);
	AstRef<AstEnum> parse_enum();

	AstRef<AstAttribute> parse_attribute(Bool parse_expr);
	AstRef<AstDirective> parse_directive();

	Bool skip_possible_newline_for_literal();

	Parser(System& sys, Lexer&& lexer, AstFile&& ast);

	template<Ulen E, typename... Ts>
	Unit error(const char (&msg)[E], Ts&&...) {
		ScratchAllocator<1024> scratch{sys_.allocator};
		StringBuilder builder{scratch};
		auto position = lexer_.position(token_);
		builder.put(ast_.filename());
		builder.put(':');
		builder.put(position.line);
		builder.put(':');
		builder.put(position.column);
		builder.put(':');
		builder.put(' ');
		builder.put("error");
		builder.put(':');
		builder.put(' ');
		builder.put(StringView { msg });
		builder.put('\n');
		if (auto result = builder.result()) {
			sys_.console.write(sys_, *result);
		} else {
			sys_.console.write(sys_, StringView { "Out of memory" });
		}
		return {};
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
	constexpr Bool is_assignment(AssignKind kind) const {
		return is_kind(TokenKind::ASSIGNMENT) && token_.as_assign == kind;
	}
	void eat();
	System&            sys_;
	TemporaryAllocator temporary_;
	AstFile            ast_;
	Lexer              lexer_;
	Token              token_;
	// >= 0: In Expression
	// <  0: In Control Clause
	Sint32             expr_level_ = 0;
	Bool               allow_in_expr_ = false;
};

} // namespace Thor

#endif // THOR_PARSER_H

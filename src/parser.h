#ifndef THOR_PARSER_H
#define THOR_PARSER_H
#include "lexer.h"
#include "ast.h"
#include "util/system.h"

namespace Thor {

struct Parser {
	static Maybe<Parser> open(System& sys, StringView file);
	AstStringRef parse_ident(Uint32* poffset = nullptr);

	using DirectiveList = Maybe<Array<AstRef<AstDirective>>>;
	using AttributeList = Maybe<Array<AstRef<AstField>>>;

	// Expression parsers
	AstRef<AstExpr>       parse_expr(Bool lhs);
	AstRef<AstExpr>       parse_value(Bool lhs);

	AstRef<AstExpr>       parse_operand();
	AstRef<AstExpr>       parse_bin_expr(Bool lhs, Uint32 prec);
	AstRef<AstExpr>       parse_unary_expr(Bool lhs);
	AstRef<AstExpr>       parse_operand(Bool lhs); // Operand parser for AstBinExpr or AstUnaryExpr

	AstRef<AstIntExpr> parse_int_expr();
	AstRef<AstFloatExpr> parse_float_expr();
	AstRef<AstStringExpr> parse_string_expr();
	AstRef<AstImaginaryExpr> parse_imaginary_expr();
	AstRef<AstCompoundExpr> parse_compound_expr();
	AstRef<AstProcExpr> parse_proc_expr();
	AstRef<AstExpr> parse_paren_expr();
	AstRef<AstIdentExpr> parse_ident_expr();
	AstRef<AstUndefExpr> parse_undef_expr();
	AstRef<AstContextExpr> parse_context_expr();
	AstRef<AstIfExpr> parse_if_expr(AstRef<AstExpr> expr);
	AstRef<AstWhenExpr> parse_when_expr(AstRef<AstExpr> on_true);
	AstRef<AstDerefExpr> parse_deref_expr(AstRef<AstExpr> operand);
	AstRef<AstOrReturnExpr> parse_or_return_expr(AstRef<AstExpr> operand);
	AstRef<AstOrBreakExpr> parse_or_break_expr(AstRef<AstExpr> operand);
	AstRef<AstOrContinueExpr> parse_or_continue_expr(AstRef<AstExpr> operand);
	AstRef<AstCallExpr> parse_call_expr(AstRef<AstExpr> operand);

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
	AstRef<AstMultiPtrType> parse_multiptr_type(Uint32 offset);
	AstRef<AstSliceType> parse_slice_type(Uint32 offset);
	AstRef<AstArrayType> parse_array_type(Uint32 offset);
	AstRef<AstDynArrayType> parse_dynarray_type(Uint32 offset);
	AstRef<AstMapType> parse_map_type();
	AstRef<AstBitsetType> parse_bitset_type();
	AstRef<AstMatrixType> parse_matrix_type();
	AstRef<AstNamedType> parse_named_type();
	AstRef<AstParenType> parse_paren_type();
	AstRef<AstDistinctType> parse_distinct_type();

	// Misc parsers
	AttributeList parse_attributes();
	DirectiveList parse_directives();

	[[nodiscard]] THOR_FORCEINLINE constexpr AstFile& ast() { return ast_; }
	[[nodiscard]] THOR_FORCEINLINE constexpr const AstFile& ast() const { return ast_; }
private:
	AstRef<AstExpr> parse_unary_atom(AstRef<AstExpr> operand, Bool lhs);
	AstRef<AstField> parse_field(Bool allow_assignment);

	AstRef<AstDirective> parse_directive();

	Parser(System& sys, Lexer&& lexer, AstFile&& ast);

	template<Ulen E, typename... Ts>
	Unit error(Uint32 offset, const char (&msg)[E], Ts&&...) {
		ScratchAllocator<1024> scratch{sys_.allocator};
		StringBuilder builder{scratch};
		auto position = lexer_.position(offset);
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
	template<Ulen E, typename... Ts>
	Unit error(const char (&msg)[E], Ts&&... args) {
		return error(token_.offset, msg, forward<Ts>(args)...);
	}
	THOR_FORCEINLINE constexpr Bool is_kind(TokenKind kind) const {
		return token_.kind == kind;
	}
	THOR_FORCEINLINE constexpr Bool is_semi() const {
		return is_kind(TokenKind::EXPLICITSEMI) || is_kind(TokenKind::IMPLICITSEMI);
	}
	THOR_FORCEINLINE constexpr Bool is_keyword(KeywordKind kind) const {
		return is_kind(TokenKind::KEYWORD) && token_.as_keyword == kind;
	}
	THOR_FORCEINLINE constexpr Bool is_operator(OperatorKind kind) const {
		return is_kind(TokenKind::OPERATOR) && token_.as_operator == kind;
	}
	THOR_FORCEINLINE constexpr Bool is_literal(LiteralKind kind) const {
		return is_kind(TokenKind::LITERAL) && token_.as_literal == kind;
	}
	THOR_FORCEINLINE constexpr Bool is_assignment(AssignKind kind) const {
		return is_kind(TokenKind::ASSIGNMENT) && token_.as_assign == kind;
	}

	// Eat the current token, advancing the lexer and return the byte position of
	// the previous token.
	Uint32 eat();

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

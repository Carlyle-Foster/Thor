#include "util/system.h"
#include "util/file.h"
#include "util/map.h"
#include "util/stream.h"

#include "parser.h"

namespace Thor {
	extern const Filesystem STD_FILESYSTEM;
	extern const Heap       STD_HEAP;
	extern const Console    STD_CONSOLE;
}

using namespace Thor;

#include <stdio.h>

int main(int, char **) {
	System sys {
		STD_FILESYSTEM,
		STD_HEAP,
		STD_CONSOLE
	};

	auto parser = Parser::open(sys, "test/ks.odin");
	if (!parser) {
		return 1;
	}
	auto& ast = parser->ast();

	StringBuilder builder{sys.allocator};
	Array<AstRef<AstExpr>> exprs{sys.allocator};
	for (;;) {
		auto expr = parser->parse_expr(false);
		if (!expr) {
			break;
		}
		if (!exprs.push_back(move(expr))) {
			break;
		}
	}
	for (auto expr : exprs) {
		ast[expr].dump(ast, builder);
	}

	/*
	auto type = parser->parse_type();
	if (!type) {
		return 1;
	}
	ast[type].dump(ast, builder);
	*/

	if (auto result = builder.result()) {
		sys.console.write(sys, *result);
		sys.console.write(sys, StringView { "\n" });
	}
	return 0;
}

#include "util/system.h"
#include "util/file.h"
#include "util/map.h"

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
	Array<AstRef<AstStmt>> stmts{sys.allocator};
	for (;;) {
		auto stmt = parser->parse_stmt();
		if (!stmt || !stmts.push_back(move(stmt))) {
			break;
		}
	}
	for (auto stmt : stmts) {
		ast[stmt].dump(ast, builder, 0);
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

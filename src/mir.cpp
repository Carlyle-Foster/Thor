#include "mir.h"

namespace Thor {

void MirVal::dump(const Mir&, StringBuilder& builder) const {
	builder.put("val");
}

MirRef<MirVal> MirBuilder::build_arith(MirRef<MirVal> lhs, MirRef<MirVal> rhs) {
	auto inst = mir_.create<MirArithInst>(lhs, rhs);
	mir_[blocks_.last()].append(inst);
	return {};
}

void MirBlock::dump(const Mir& mir, StringBuilder& builder) const {
	const auto n_insts = insts.length();
	builder.put("block");
	builder.put(':');
	builder.put('\n');
	for (Ulen i = 0; i < n_insts; i++) {
		builder.rep(2, ' ');
		mir[insts[i]].dump(mir, builder);
		builder.put('\n');
	}
}

void MirArithInst::dump(const Mir& mir, StringBuilder& builder) const {
	builder.put("arith");
	builder.put(' ');
	mir[lhs].dump(mir, builder);
	builder.put(',');
	builder.put(' ');
	mir[rhs].dump(mir, builder);
}

} // namespace Thor
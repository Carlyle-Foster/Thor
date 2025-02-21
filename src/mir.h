#ifndef THOR_MIR_H
#define THOR_MIR_H

namespace Thor {

template<typename T>
struct MirRef {
};

struct MirNode {};

struct MirAttr : MirNode {};

struct MirType : MirNode {};
struct MirUnitType : MirType {}; // unit
struct MirIntType : MirType {}; // u8, u16, u32, u64
struct MirPtrType : MirType {}; // ptr
struct MirFloatType : MirType {}; // f16, f32, f64
struct MirStructType : MirType {}; // struct
struct MirArrayType : MirType {}; // []Type
struct MirFuncType : MirType {}; // proc()

struct MirBlock : MirNode {};

struct MirConst : MirNode {};
struct MirConstSeq : MirConst {};
struct MirConstArray : MirConstSeq {
	MirRef<MirArrayType>    type;
	Array<MirRef<MirConst>> values;
};
struct MirConstStruct : MirConstSeq {
	MirRef<MirStructType>   type;
	Array<MirRef<MirConst>> values;
};

struct MirGlobalValue : MirConst {
	enum class Linkage {
		External, // "extern" in C parlance
		Static,   // "static" in C parlance
	};
	Linkage linkage;
};

struct MirGlobalObject : MirGlobalValue {
	MirRef<MirType>  type;
	Uint32           align;
	StringRef        section;
};

struct MirGlobalVar : MirGlobalValue {
	MirRef<MirConst> init;
};

struct MirFunction : MirGlobalObject {
	MirRef<MirFuncType> type;
	MirRef<MirBlock>    entry;
};

struct Mir {

};

} // namespace Thor

#endif // THOR_MIR_H
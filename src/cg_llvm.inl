#ifndef FN
#define FN(...)
#endif

//
// Analysis.h
//
FN(Bool,                  VerifyModule,                ModuleRef, VerifierFailureAction, char**)

//
// Core.h
//

// Global
FN(void,                  Shutdown,                    void)
FN(void,                  GetVersion,                  unsigned*, unsigned*, unsigned*)
FN(void,                  DisposeMessage,              char*)
// Context
FN(ContextRef,            ContextCreate,               void)
FN(void,                  ContextDispose,              ContextRef)
FN(unsigned,              GetEnumAttributeKindForName, const char*, Ulen)
FN(AttributeRef,          CreateEnumAttribute,         ContextRef, unsigned, Uint64)
// Types
FN(TypeRef,               Int1TypeInContext,           ContextRef)
FN(TypeRef,               Int8TypeInContext,           ContextRef)
FN(TypeRef,               Int16TypeInContext,          ContextRef)
FN(TypeRef,               Int32TypeInContext,          ContextRef)
FN(TypeRef,               Int64TypeInContext,          ContextRef)
FN(TypeRef,               FloatTypeInContext,          ContextRef)
FN(TypeRef,               DoubleTypeInContext,         ContextRef)
FN(TypeRef,               PointerTypeInContext,        ContextRef, unsigned)
FN(TypeRef,               VoidTypeInContext,           ContextRef)
FN(TypeRef,               StructTypeInContext,         ContextRef, TypeRef*, unsigned, Bool)
FN(TypeRef,               FunctionType,                TypeRef, TypeRef*, unsigned, Bool)
FN(TypeRef,               StructCreateNamed,           ContextRef, const char*)
FN(TypeRef,               ArrayType2,                  TypeRef, Uint64)
FN(TypeRef,               GetTypeByName2,              ContextRef, const char*)
// Values
FN(ValueRef,              ConstNull,                   ValueRef, const char*, Ulen)
FN(ValueRef,              ConstPointerNull,            TypeRef)
FN(ValueRef,              ConstInt,                    TypeRef, unsigned long long, Bool)
FN(ValueRef,              ConstReal,                   TypeRef, double)
FN(ValueRef,              ConstStructInContext,        ContextRef, ValueRef*, unsigned, Bool)
FN(ValueRef,              ConstArray2,                 TypeRef, ValueRef*, Uint64)
FN(ValueRef,              ConstNamedStruct,            TypeRef, ValueRef*, unsigned)
FN(ValueRef,              AddGlobal,                   ModuleRef, TypeRef, const char*)
FN(ValueRef,              GetParam,                    ValueRef, unsigned)
FN(ValueRef,              GetBasicBlockParent,         BasicBlockRef)
FN(ValueRef,              GetBasicBlockTerminator,     BasicBlockRef)
FN(ValueRef,              BuildRetVoid,                BuilderRef)
FN(ValueRef,              BuildRet,                    BuilderRef, ValueRef)
FN(ValueRef,              BuildBr,                     BuilderRef, BasicBlockRef)
FN(ValueRef,              BuildCondBr,                 BuilderRef, ValueRef, BasicBlockRef, BasicBlockRef)
FN(ValueRef,              BuildAdd,                    BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFAdd,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildSub,                    BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFSub,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildMul,                    BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFMul,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildUDiv,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildSDiv,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildURem,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildSRem,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFRem,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildShl,                    BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildLShr,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildAShr,                   BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildAnd,                    BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildOr,                     BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildXor,                    BuilderRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildNeg,                    BuilderRef, ValueRef, const char*)
FN(ValueRef,              BuildFNeg,                   BuilderRef, ValueRef, const char*)
FN(ValueRef,              BuildNot,                    BuilderRef, ValueRef, const char*)
FN(ValueRef,              BuildMemCpy,                 BuilderRef, ValueRef, unsigned, ValueRef, unsigned, ValueRef)
FN(ValueRef,              BuildMemSet,                 BuilderRef, ValueRef, ValueRef, ValueRef, unsigned)
FN(ValueRef,              BuildAlloca,                 BuilderRef, TypeRef, const char*)
FN(ValueRef,              BuildLoad2,                  BuilderRef, TypeRef, ValueRef, const char*)
FN(ValueRef,              BuildStore,                  BuilderRef, ValueRef, ValueRef)
FN(ValueRef,              BuildGEP2,                   BuilderRef, TypeRef, ValueRef, ValueRef*, unsigned, const char*)
FN(ValueRef,              BuildGlobalString,           BuilderRef, const char*, const char*)
FN(ValueRef,              BuildCast,                   BuilderRef, Opcode, ValueRef, TypeRef, const char*)
FN(ValueRef,              BuildICmp,                   BuilderRef, IntPredicate, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildFCmp,                   BuilderRef, IntPredicate, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildPhi,                    BuilderRef, TypeRef, const char*)
FN(ValueRef,              BuildCall2,                  BuilderRef, TypeRef, ValueRef, ValueRef*, unsigned, const char*)
FN(ValueRef,              BuildSelect,                 BuilderRef, ValueRef, ValueRef, ValueRef, const char*)
FN(ValueRef,              BuildExtractValue,           BuilderRef, ValueRef, unsigned, const char*)
// Builder
FN(BasicBlockRef,         CreateBasicBlockInContext,   ContextRef, const char*)
FN(void,                  AppendExistingBasicBlock,    ValueRef, BasicBlockRef)
FN(BuilderRef,            CreateBuilderInContext,      ContextRef)
FN(void,                  PositionBuilderAtEnd,        BuilderRef, BasicBlockRef)
FN(BasicBlockRef,         GetInsertBlock,              BuilderRef)
FN(void,                  DisposeBuilder,              BuilderRef)


//
// Error.h
//
FN(void,                  ConsumeError,                ErrorRef)

//
// Target.h
//
FN(void,                  InitializeX86TargetInfo,     void)
FN(void,                  InitializeX86Target,         void)
FN(void,                  InitializeX86TargetMC,       void)
FN(void,                  InitializeX86AsmPrinter,     void)
FN(void,                  InitializeX86AsmParser,      void)
FN(void,                  InitializeAArch64TargetInfo, void)
FN(void,                  InitializeAArch64Target,     void)
FN(void,                  InitializeAArch64TargetMC,   void)
FN(void,                  InitializeAArch64AsmPrinter, void)
FN(void,                  InitializeAArch64AsmParser,  void)

//
// TargetMachine.h
//
FN(Bool,                  GetTargetFromTriple,         const char*, TargetRef*, char**)
FN(TargetMachineRef,      CreateTargetMachine,         TargetRef, const char*, const char*, const char*, CodeGenOptLevel, RelocMode, CodeModel)
FN(void,                  DisposeTargetMachine,        TargetMachineRef)
FN(Bool,                  TargetMachineEmitToFile,     TargetMachineRef, ModuleRef, const char*, CodeGenFileType, char**)

//
// PassBuilder.h
//
FN(ErrorRef,              RunPasses,                   ModuleRef, const char*, TargetMachineRef, PassBuilderOptionsRef)
FN(PassBuilderOptionsRef, CreatePassBuilderOptions,    void)
FN(void,                  DisposePassBuilderOptions,   PassBuilderOptionsRef)

#undef FN
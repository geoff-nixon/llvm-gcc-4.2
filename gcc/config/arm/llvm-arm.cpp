/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* High-level LLVM backend interface 
Copyright (C) 2008, 2009 Apple Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

//===----------------------------------------------------------------------===//
// This is a C++ source file that implements specific llvm ARM ABI.
//===----------------------------------------------------------------------===//

#include "llvm-abi.h"
#include "llvm-internal.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Instructions.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"

extern "C" {
#include "insn-codes.h"
#include "toplev.h"
#include "rtl.h"
#include "insn-config.h"
#include "recog.h"

enum neon_itype { neon_itype_dummy };
extern enum insn_code locate_neon_builtin_icode
  (int fcode, neon_itype *itype, enum neon_builtins *neon_code);
}

/// UnexpectedError - Report errors about unexpected uses of builtins.  The
/// msg argument should begin with a "%H" so that the location of the
/// expression is printed in the error message.
static bool UnexpectedError(const char *msg, tree exp, Value *&Result) {
  error(msg, &EXPR_LOCATION(exp));

  // Set the Result to an undefined value.
  const Type *ResTy = ConvertType(TREE_TYPE(exp));
  if (ResTy->isSingleValueType())
    Result = getGlobalContext().getUndef(ResTy);

  // Return true, which can be propagated as the return value of
  // TargetIntrinsicLower, to indicate that no further error message
  // is needed.
  return true;
}

static bool NonImmediateError(tree exp, Value *&Result) {
  return UnexpectedError("%Hlast builtin argument must be an immediate",
                         exp, Result);
}

static bool BadImmediateError(tree exp, Value *&Result) {
  return UnexpectedError("%Hunexpected immediate argument for builtin",
                         exp, Result);
}

static bool BadModeError(tree exp, Value *&Result) {
  return UnexpectedError("%Hunexpected mode for builtin argument",
                         exp, Result);
}

enum neon_datatype {
  neon_datatype_unspecified,
  neon_datatype_signed,
  neon_datatype_unsigned,
  neon_datatype_float,
  neon_datatype_polynomial
};

/// GetBuiltinExtraInfo - Decipher the extra integer immediate argument
/// used with many of GCC's builtins for NEON to distinguish variants of an
/// operation.  The following values for that argument are used:
///   - bit0: For integer types (i.e., bit2 == 0), 0 = unsigned, 1 = signed;
///           otherwise, 0 = polynomial, 1 = float.
///   - bit1: The operation rounds its results.
///   - bit2: 0 = integer datatypes, 1 = floating-point or polynomial.
///   .
/// Returns false if the extra argument is not an integer immediate.
static bool GetBuiltinExtraInfo(const Value *extra_arg,
                                neon_datatype &datatype, bool &isRounded) {
  const ConstantInt *arg = dyn_cast<ConstantInt>(extra_arg);
  if (!arg)
    return false;

  int argval = arg->getZExtValue();
  isRounded = ((argval & 2) != 0);
  if ((argval & 4) == 0) {
    if ((argval & 1) == 0)
      datatype = neon_datatype_unsigned;
    else
      datatype = neon_datatype_signed;
  } else {
    if ((argval & 1) == 0)
      datatype = neon_datatype_polynomial;
    else
      datatype = neon_datatype_float;
  }
  return true;
}

/// BuildConstantSplatVector - Create a ConstantVector with the same value
/// replicated in each element.
static Value *BuildConstantSplatVector(unsigned NumElements, ConstantInt *Val) {
  std::vector<Constant*> CstOps;
  for (unsigned i = 0; i != NumElements; ++i)
    CstOps.push_back(Val);
  return ConstantVector::get(CstOps);
}

/// BuildDup - Build a splat operation to duplicate a value into every
/// element of a vector.
static Value *BuildDup(const Type *ResultType, Value *Val,
                       LLVMBuilder &Builder) {
  LLVMContext &Context = getGlobalContext();

  // GCC may promote the scalar argument; cast it back.
  const VectorType *VTy = dyn_cast<const VectorType>(ResultType);
  assert(VTy && "expected a vector type");
  const Type *ElTy = VTy->getElementType();
  if (Val->getType() != ElTy) {
    assert(!ElTy->isFloatingPoint() &&
           "only integer types expected to be promoted");
    Val = Builder.CreateTrunc(Val, ElTy);
  }

  // Insert the value into lane 0 of an undef vector.
  Value *Undef = Context.getUndef(ResultType);
  Value *Result =
    Builder.CreateInsertElement(Undef, Val,
                                ConstantInt::get(Type::Int32Ty, 0));

  // Use a shuffle to move the value into the other lanes.
  unsigned NUnits = VTy->getNumElements();
  if (NUnits > 1) {
    std::vector<Constant*> Idxs;
    for (unsigned i = 0; i != NUnits; ++i)
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, 0));
    Result = Builder.CreateShuffleVector(Result, Undef,
                                         ConstantVector::get(Idxs));
  }
  return Result;
}

/// BuildDupLane - Build a splat operation to take a value from one element
/// of a vector and splat it into another vector.
static Value *BuildDupLane(Value *Vec, unsigned LaneVal, unsigned NUnits,
                           LLVMBuilder &Builder) {
  // Translate this to a vector shuffle.
  std::vector<Constant*> Idxs;
  LLVMContext &Context = getGlobalContext();
  for (unsigned i = 0; i != NUnits; ++i)
    Idxs.push_back(ConstantInt::get(Type::Int32Ty, LaneVal));
  return Builder.CreateShuffleVector(Vec, Context.getUndef(Vec->getType()),
                                     ConstantVector::get(Idxs));
}

// NEON vector shift counts must be in the range 0..ElemBits-1 for left shifts
// or 1..ElemBits for right shifts.  For narrowing shifts, compare against the
// destination element size.  For widening shifts, the upper bound can be
// equal to the element size.  Define separate functions to check these
// constraints, so that the rest of the code for handling vector shift counts
// can be shared.

typedef bool (*ShiftCountChecker)(int Cnt, int ElemBits);

static bool CheckLeftShiftCount(int Cnt, int ElemBits) {
  return (Cnt >= 0 && Cnt < ElemBits);
}

static bool CheckLongLeftShiftCount(int Cnt, int ElemBits) {
  return (Cnt >= 0 && Cnt <= ElemBits);
}

static bool CheckRightShiftCount(int Cnt, int ElemBits) {
  return (Cnt >= 1 && Cnt <= ElemBits);
}

static bool CheckNarrowRightShiftCount(int Cnt, int ElemBits) {
  return (Cnt >= 1 && Cnt <= ElemBits / 2);
}

/// BuildShiftCountVector - Check that the shift count argument to a constant
/// shift builtin is a constant in the appropriate range for the shift
/// operation.  It expands the shift count into a vector, optionally with the
/// count negated for right shifts.  Returns true on success.
static bool BuildShiftCountVector(Value *&Op, enum machine_mode Mode,
                                  ShiftCountChecker CheckCount,
                                  bool NegateRightShift) {
  ConstantInt *Cnt = dyn_cast<ConstantInt>(Op);
  if (!Cnt)
    return false;
  int CntVal = Cnt->getSExtValue();

  assert (VECTOR_MODE_P (Mode) && "expected vector mode for shift");
  unsigned ElemBits = GET_MODE_BITSIZE (GET_MODE_INNER (Mode));
  if (!CheckCount(CntVal, ElemBits))
    return false;

  // Right shifts are represented in NEON intrinsics by a negative shift count.
  LLVMContext &Context = getGlobalContext();
  Cnt = ConstantInt::get(IntegerType::get(ElemBits),
                               NegateRightShift ? -CntVal : CntVal);
  Op = BuildConstantSplatVector(GET_MODE_NUNITS(Mode), Cnt);
  return true;
}

/// isValidLane - Check if the lane operand for a vector intrinsic is a
/// ConstantInt in the range 0..NUnits.  If pLaneVal is not null, store
/// the lane value to it.
static bool isValidLane(Value *LnOp, int NUnits, unsigned *pLaneVal = 0) {
  ConstantInt *Lane = dyn_cast<ConstantInt>(LnOp);
  if (!Lane)
    return false;

  int LaneVal = Lane->getSExtValue();
  if (LaneVal < 0 || LaneVal >= NUnits)
    return false;

  if (pLaneVal)
    *pLaneVal = LaneVal;
  return true;
}

/// GetVldstType - Get the vector type of a NEON vector load/store instruction.
/// For NEON vector structs used in vldN/vstN instruction (2 <= N <= 4), GCC
/// treats NEON vector structs as scalars, but LLVM uses wide vector types
/// that combine all the vectors in a struct.  For example, int8x8x4 (4 int8x8
/// vectors) is treated as a single vector of 32 i8 elements.  Since none of
/// the instruction operands identify the vector mode, get the element type
/// from the pointer type of the first argument and the total size from the
/// result mode.
static const VectorType *
GetVldstType(tree exp, enum machine_mode ResultMode) {
  tree FnDecl = get_callee_fndecl(exp);
  tree ArgTy = TREE_VALUE(TYPE_ARG_TYPES(TREE_TYPE(FnDecl)));
  assert(ArgTy && POINTER_TYPE_P (ArgTy) && "Expected a pointer type!");
  enum machine_mode ElemMode = TYPE_MODE(TREE_TYPE(ArgTy));
  // Note: Because of a field size limitation in GCC, the NEON XI mode is
  // defined as 511 bits instead of 512.  Add one below to adjust for this.
  unsigned NumElems =
    (GET_MODE_BITSIZE(ResultMode) + 1) / GET_MODE_BITSIZE(ElemMode);
  const Type *ElemType = ConvertType(TREE_TYPE(ArgTy));
  return VectorType::get(ElemType, NumElems);
}

/// TargetIntrinsicLower - To handle builtins, we want to expand the
/// invocation into normal LLVM code.  If the target can handle the builtin,
/// this function should emit the expanded code and return true.
bool TreeToLLVM::TargetIntrinsicLower(tree exp,
                                      unsigned FnCode,
                                      const MemRef *DestLoc,
                                      Value *&Result,
                                      const Type *ResultType,
                                      std::vector<Value*> &Ops) {
  neon_datatype datatype = neon_datatype_unspecified;
  bool isRounded = false;
  Intrinsic::ID intID;
  Function *intFn;
  const Type* intOpTypes[2];

  if (FnCode < ARM_BUILTIN_NEON_BASE)
    return false;

  LLVMContext &Context = getGlobalContext();

  neon_builtins neon_code;
  enum insn_code icode = locate_neon_builtin_icode (FnCode, 0, &neon_code);

  // Read the extra immediate argument to the builtin.
  switch (neon_code) {
  default:
    return false;
  case NEON_BUILTIN_vpaddl:
  case NEON_BUILTIN_vneg:
  case NEON_BUILTIN_vqneg:
  case NEON_BUILTIN_vabs:
  case NEON_BUILTIN_vqabs:
  case NEON_BUILTIN_vcls:
  case NEON_BUILTIN_vclz:
  case NEON_BUILTIN_vcnt:
  case NEON_BUILTIN_vrecpe:
  case NEON_BUILTIN_vrsqrte:
  case NEON_BUILTIN_vmvn:
  case NEON_BUILTIN_vcvt:
  case NEON_BUILTIN_vmovn:
  case NEON_BUILTIN_vqmovn:
  case NEON_BUILTIN_vqmovun:
  case NEON_BUILTIN_vmovl:
  case NEON_BUILTIN_vrev64:
  case NEON_BUILTIN_vrev32:
  case NEON_BUILTIN_vrev16:
    if (!GetBuiltinExtraInfo(Ops[1], datatype, isRounded))
      return NonImmediateError(exp, Result);
    break;
  case NEON_BUILTIN_vadd:
  case NEON_BUILTIN_vaddl:
  case NEON_BUILTIN_vaddw:
  case NEON_BUILTIN_vhadd:
  case NEON_BUILTIN_vqadd:
  case NEON_BUILTIN_vaddhn:
  case NEON_BUILTIN_vmul:
  case NEON_BUILTIN_vqdmulh:
  case NEON_BUILTIN_vmull:
  case NEON_BUILTIN_vqdmull:
  case NEON_BUILTIN_vsub:
  case NEON_BUILTIN_vsubl:
  case NEON_BUILTIN_vsubw:
  case NEON_BUILTIN_vqsub:
  case NEON_BUILTIN_vhsub:
  case NEON_BUILTIN_vsubhn:
  case NEON_BUILTIN_vceq:
  case NEON_BUILTIN_vcge:
  case NEON_BUILTIN_vcgt:
  case NEON_BUILTIN_vcage:
  case NEON_BUILTIN_vcagt:
  case NEON_BUILTIN_vtst:
  case NEON_BUILTIN_vabd:
  case NEON_BUILTIN_vabdl:
  case NEON_BUILTIN_vmax:
  case NEON_BUILTIN_vmin:
  case NEON_BUILTIN_vpadd:
  case NEON_BUILTIN_vpadal:
  case NEON_BUILTIN_vpmax:
  case NEON_BUILTIN_vpmin:
  case NEON_BUILTIN_vrecps:
  case NEON_BUILTIN_vrsqrts:
  case NEON_BUILTIN_vshl:
  case NEON_BUILTIN_vqshl:
  case NEON_BUILTIN_vshr_n:
  case NEON_BUILTIN_vshrn_n:
  case NEON_BUILTIN_vqshrn_n:
  case NEON_BUILTIN_vqshrun_n:
  case NEON_BUILTIN_vshl_n:
  case NEON_BUILTIN_vqshl_n:
  case NEON_BUILTIN_vqshlu_n:
  case NEON_BUILTIN_vshll_n:
  case NEON_BUILTIN_vget_lane:
  case NEON_BUILTIN_vcvt_n:
  case NEON_BUILTIN_vmul_n:
  case NEON_BUILTIN_vmull_n:
  case NEON_BUILTIN_vqdmull_n:
  case NEON_BUILTIN_vqdmulh_n:
  case NEON_BUILTIN_vand:
  case NEON_BUILTIN_vorr:
  case NEON_BUILTIN_veor:
  case NEON_BUILTIN_vbic:
  case NEON_BUILTIN_vorn:
    if (!GetBuiltinExtraInfo(Ops[2], datatype, isRounded))
      return NonImmediateError(exp, Result);
    break;
  case NEON_BUILTIN_vmla:
  case NEON_BUILTIN_vmls:
  case NEON_BUILTIN_vmlal:
  case NEON_BUILTIN_vmlsl:
  case NEON_BUILTIN_vqdmlal:
  case NEON_BUILTIN_vqdmlsl:
  case NEON_BUILTIN_vaba:
  case NEON_BUILTIN_vabal:
  case NEON_BUILTIN_vsra_n:
  case NEON_BUILTIN_vmul_lane:
  case NEON_BUILTIN_vmull_lane:
  case NEON_BUILTIN_vqdmull_lane:
  case NEON_BUILTIN_vqdmulh_lane:
  case NEON_BUILTIN_vmla_n:
  case NEON_BUILTIN_vmlal_n:
  case NEON_BUILTIN_vqdmlal_n:
  case NEON_BUILTIN_vmls_n:
  case NEON_BUILTIN_vmlsl_n:
  case NEON_BUILTIN_vqdmlsl_n:
    if (!GetBuiltinExtraInfo(Ops[3], datatype, isRounded))
      return NonImmediateError(exp, Result);
    break;
  case NEON_BUILTIN_vmla_lane:
  case NEON_BUILTIN_vmlal_lane:
  case NEON_BUILTIN_vqdmlal_lane:
  case NEON_BUILTIN_vmls_lane:
  case NEON_BUILTIN_vmlsl_lane:
  case NEON_BUILTIN_vqdmlsl_lane:
    if (!GetBuiltinExtraInfo(Ops[4], datatype, isRounded))
      return NonImmediateError(exp, Result);
    break;
  case NEON_BUILTIN_vsri_n:
  case NEON_BUILTIN_vsli_n:
  case NEON_BUILTIN_vset_lane:
  case NEON_BUILTIN_vcreate:
  case NEON_BUILTIN_vdup_n:
  case NEON_BUILTIN_vdup_lane:
  case NEON_BUILTIN_vcombine:
  case NEON_BUILTIN_vget_high:
  case NEON_BUILTIN_vget_low:
  case NEON_BUILTIN_vtbl1:
  case NEON_BUILTIN_vtbl2:
  case NEON_BUILTIN_vtbl3:
  case NEON_BUILTIN_vtbl4:
  case NEON_BUILTIN_vtbx1:
  case NEON_BUILTIN_vtbx2:
  case NEON_BUILTIN_vtbx3:
  case NEON_BUILTIN_vtbx4:
  case NEON_BUILTIN_vext:
  case NEON_BUILTIN_vbsl:
  case NEON_BUILTIN_vtrn:
  case NEON_BUILTIN_vzip:
  case NEON_BUILTIN_vuzp:
  case NEON_BUILTIN_vld1:
  case NEON_BUILTIN_vld2:
  case NEON_BUILTIN_vld3:
  case NEON_BUILTIN_vld4:
  case NEON_BUILTIN_vld1_lane:
  case NEON_BUILTIN_vld2_lane:
  case NEON_BUILTIN_vld3_lane:
  case NEON_BUILTIN_vld4_lane:
  case NEON_BUILTIN_vld1_dup:
  case NEON_BUILTIN_vld2_dup:
  case NEON_BUILTIN_vld3_dup:
  case NEON_BUILTIN_vld4_dup:
  case NEON_BUILTIN_vst1:
  case NEON_BUILTIN_vst2:
  case NEON_BUILTIN_vst3:
  case NEON_BUILTIN_vst4:
  case NEON_BUILTIN_vst1_lane:
  case NEON_BUILTIN_vst2_lane:
  case NEON_BUILTIN_vst3_lane:
  case NEON_BUILTIN_vst4_lane:
  case NEON_BUILTIN_vreinterpretv8qi:
  case NEON_BUILTIN_vreinterpretv4hi:
  case NEON_BUILTIN_vreinterpretv2si:
  case NEON_BUILTIN_vreinterpretv2sf:
  case NEON_BUILTIN_vreinterpretdi:
  case NEON_BUILTIN_vreinterpretv16qi:
  case NEON_BUILTIN_vreinterpretv8hi:
  case NEON_BUILTIN_vreinterpretv4si:
  case NEON_BUILTIN_vreinterpretv4sf:
  case NEON_BUILTIN_vreinterpretv2di:
    // No extra argument used here.
    break;
  }

  // Check that the isRounded flag is only set when it is supported.
  if (isRounded) {
    switch (neon_code) {
    case NEON_BUILTIN_vhadd:
    case NEON_BUILTIN_vaddhn:
    case NEON_BUILTIN_vqdmulh:
    case NEON_BUILTIN_vsubhn:
    case NEON_BUILTIN_vshl:
    case NEON_BUILTIN_vqshl:
    case NEON_BUILTIN_vshr_n:
    case NEON_BUILTIN_vshrn_n:
    case NEON_BUILTIN_vqshrn_n:
    case NEON_BUILTIN_vqshrun_n:
    case NEON_BUILTIN_vsra_n:
    case NEON_BUILTIN_vqdmulh_lane:
    case NEON_BUILTIN_vqdmulh_n:
      // These all support a rounded variant.
      break;
    default:
      return BadImmediateError(exp, Result);
    }
  }

  // Check for supported vector modes.

  // Set defaults for mode checking.
  int modeCheckOpnd = 1;
  bool allow_64bit_modes = true;
  bool allow_128bit_modes = true;
  bool allow_8bit_elements = true;
  bool allow_16bit_elements = true;
  bool allow_32bit_elements = true;
  bool allow_64bit_elements = false;
  bool allow_16bit_polynomials = false;

  switch (neon_code) {
  default:
    assert(0 && "unexpected builtin");
    break;

  case NEON_BUILTIN_vadd:
  case NEON_BUILTIN_vsub:
  case NEON_BUILTIN_vqadd:
  case NEON_BUILTIN_vqsub:
  case NEON_BUILTIN_vshl:
  case NEON_BUILTIN_vqshl:
  case NEON_BUILTIN_vshr_n:
  case NEON_BUILTIN_vshl_n:
  case NEON_BUILTIN_vqshl_n:
  case NEON_BUILTIN_vqshlu_n:
  case NEON_BUILTIN_vsra_n:
  case NEON_BUILTIN_vsri_n:
  case NEON_BUILTIN_vsli_n:
  case NEON_BUILTIN_vmvn:
  case NEON_BUILTIN_vext:
  case NEON_BUILTIN_vbsl:
  case NEON_BUILTIN_vand:
  case NEON_BUILTIN_vorr:
  case NEON_BUILTIN_veor:
  case NEON_BUILTIN_vbic:
  case NEON_BUILTIN_vorn:
  case NEON_BUILTIN_vdup_lane:
    allow_64bit_elements = true;
    break;

  case NEON_BUILTIN_vhadd:
  case NEON_BUILTIN_vhsub:
  case NEON_BUILTIN_vmul:
  case NEON_BUILTIN_vceq:
  case NEON_BUILTIN_vcge:
  case NEON_BUILTIN_vcgt:
  case NEON_BUILTIN_vcage:
  case NEON_BUILTIN_vcagt:
  case NEON_BUILTIN_vtst:
  case NEON_BUILTIN_vabd:
  case NEON_BUILTIN_vabdl:
  case NEON_BUILTIN_vaba:
  case NEON_BUILTIN_vabal:
  case NEON_BUILTIN_vmax:
  case NEON_BUILTIN_vmin:
  case NEON_BUILTIN_vpaddl:
  case NEON_BUILTIN_vrecps:
  case NEON_BUILTIN_vrsqrts:
  case NEON_BUILTIN_vneg:
  case NEON_BUILTIN_vqneg:
  case NEON_BUILTIN_vabs:
  case NEON_BUILTIN_vqabs:
  case NEON_BUILTIN_vcls:
  case NEON_BUILTIN_vclz:
  case NEON_BUILTIN_vtrn:
  case NEON_BUILTIN_vzip:
  case NEON_BUILTIN_vuzp:
    break;

  case NEON_BUILTIN_vmla:
  case NEON_BUILTIN_vmls:
  case NEON_BUILTIN_vpadal:
    modeCheckOpnd = 2;
    break;

  case NEON_BUILTIN_vaddhn:
  case NEON_BUILTIN_vsubhn:
  case NEON_BUILTIN_vshrn_n:
  case NEON_BUILTIN_vqshrn_n:
  case NEON_BUILTIN_vqshrun_n:
  case NEON_BUILTIN_vmovn:
  case NEON_BUILTIN_vqmovn:
  case NEON_BUILTIN_vqmovun:
    allow_64bit_modes = false;
    allow_8bit_elements = false;
    allow_64bit_elements = true;
    break;

  case NEON_BUILTIN_vqdmulh:
  case NEON_BUILTIN_vqdmulh_lane:
  case NEON_BUILTIN_vqdmulh_n:
  case NEON_BUILTIN_vmul_lane:
  case NEON_BUILTIN_vmul_n:
  case NEON_BUILTIN_vmla_lane:
  case NEON_BUILTIN_vmla_n:
  case NEON_BUILTIN_vmls_lane:
  case NEON_BUILTIN_vmls_n:
    allow_8bit_elements = false;
    break;

  case NEON_BUILTIN_vqdmull:
  case NEON_BUILTIN_vqdmull_lane:
  case NEON_BUILTIN_vqdmull_n:
  case NEON_BUILTIN_vmull_lane:
  case NEON_BUILTIN_vmull_n:
    allow_128bit_modes = false;
    allow_8bit_elements = false;
    break;

  case NEON_BUILTIN_vqdmlal:
  case NEON_BUILTIN_vqdmlal_lane:
  case NEON_BUILTIN_vqdmlal_n:
  case NEON_BUILTIN_vqdmlsl:
  case NEON_BUILTIN_vmlal_lane:
  case NEON_BUILTIN_vmlal_n:
  case NEON_BUILTIN_vmlsl_lane:
  case NEON_BUILTIN_vmlsl_n:
  case NEON_BUILTIN_vqdmlsl_lane:
  case NEON_BUILTIN_vqdmlsl_n:
    modeCheckOpnd = 2;
    allow_128bit_modes = false;
    allow_8bit_elements = false;
    break;

  case NEON_BUILTIN_vaddw:
  case NEON_BUILTIN_vmlal:
  case NEON_BUILTIN_vmlsl:
  case NEON_BUILTIN_vsubw:
    modeCheckOpnd = 2;
    allow_128bit_modes = false;
    break;

  case NEON_BUILTIN_vaddl:
  case NEON_BUILTIN_vmull:
  case NEON_BUILTIN_vsubl:
  case NEON_BUILTIN_vpadd:
  case NEON_BUILTIN_vpmax:
  case NEON_BUILTIN_vpmin:
  case NEON_BUILTIN_vshll_n:
  case NEON_BUILTIN_vmovl:
    allow_128bit_modes = false;
    break;

  case NEON_BUILTIN_vcnt:
    allow_16bit_elements = false;
    allow_32bit_elements = false;
    break;

  case NEON_BUILTIN_vtbl1:
  case NEON_BUILTIN_vtbl2:
  case NEON_BUILTIN_vtbl3:
  case NEON_BUILTIN_vtbl4:
  case NEON_BUILTIN_vtbx1:
  case NEON_BUILTIN_vtbx2:
  case NEON_BUILTIN_vtbx3:
  case NEON_BUILTIN_vtbx4:
    allow_16bit_elements = false;
    allow_32bit_elements = false;
    allow_128bit_modes = false;
    modeCheckOpnd = 0;
    break;

  case NEON_BUILTIN_vrecpe:
  case NEON_BUILTIN_vrsqrte:
  case NEON_BUILTIN_vcvt:
  case NEON_BUILTIN_vcvt_n:
    allow_8bit_elements = false;
    allow_16bit_elements = false;
    break;

  case NEON_BUILTIN_vget_lane:
    allow_64bit_elements = true;
    allow_16bit_polynomials = true;
    break;

  case NEON_BUILTIN_vset_lane:
    allow_64bit_elements = true;
    allow_16bit_polynomials = true;
    modeCheckOpnd = 2;
    break;

  case NEON_BUILTIN_vrev64:
    allow_16bit_polynomials = true;
    break;

  case NEON_BUILTIN_vrev32:
    allow_16bit_polynomials = true;
    allow_32bit_elements = false;
    break;

  case NEON_BUILTIN_vrev16:
    allow_16bit_elements = false;
    allow_32bit_elements = false;
    break;

  case NEON_BUILTIN_vcreate:
    modeCheckOpnd = 0;
    allow_128bit_modes = false;
    allow_64bit_elements = true;
    break;

  case NEON_BUILTIN_vdup_n:
    modeCheckOpnd = 0;
    allow_64bit_elements = true;
    break;

  case NEON_BUILTIN_vcombine:
  case NEON_BUILTIN_vreinterpretv8qi:
  case NEON_BUILTIN_vreinterpretv4hi:
  case NEON_BUILTIN_vreinterpretv2si:
  case NEON_BUILTIN_vreinterpretv2sf:
  case NEON_BUILTIN_vreinterpretdi:
    allow_128bit_modes = false;
    allow_64bit_elements = true;
    break;

  case NEON_BUILTIN_vget_high:
  case NEON_BUILTIN_vget_low:
  case NEON_BUILTIN_vreinterpretv16qi:
  case NEON_BUILTIN_vreinterpretv8hi:
  case NEON_BUILTIN_vreinterpretv4si:
  case NEON_BUILTIN_vreinterpretv4sf:
  case NEON_BUILTIN_vreinterpretv2di:
    allow_64bit_modes = false;
    allow_64bit_elements = true;
    break;

  case NEON_BUILTIN_vld1:
  case NEON_BUILTIN_vld2:
  case NEON_BUILTIN_vld3:
  case NEON_BUILTIN_vld4:
  case NEON_BUILTIN_vld1_lane:
  case NEON_BUILTIN_vld2_lane:
  case NEON_BUILTIN_vld3_lane:
  case NEON_BUILTIN_vld4_lane:
  case NEON_BUILTIN_vld1_dup:
  case NEON_BUILTIN_vld2_dup:
  case NEON_BUILTIN_vld3_dup:
  case NEON_BUILTIN_vld4_dup:
  case NEON_BUILTIN_vst1:
  case NEON_BUILTIN_vst2:
  case NEON_BUILTIN_vst3:
  case NEON_BUILTIN_vst4:
  case NEON_BUILTIN_vst1_lane:
  case NEON_BUILTIN_vst2_lane:
  case NEON_BUILTIN_vst3_lane:
  case NEON_BUILTIN_vst4_lane:
    // Most of the load/store builtins do not have operands with the mode of
    // the operation.  Skip the mode check, since there is no extra operand
    // to check against the mode anyway.
    modeCheckOpnd = -1;
    break;
  }

  if (modeCheckOpnd >= 0) {

    switch (insn_data[icode].operand[modeCheckOpnd].mode) {
    case V8QImode: case V4HImode: case V2SImode: case DImode: case V2SFmode:
      if (!allow_64bit_modes)
        return BadModeError(exp, Result);
      break;
    case V16QImode: case V8HImode: case V4SImode: case V2DImode: case V4SFmode:
      if (!allow_128bit_modes)
        return BadModeError(exp, Result);
      break;
    default:
      return BadModeError(exp, Result);
    }

    if (datatype == neon_datatype_polynomial) {

      switch (insn_data[icode].operand[modeCheckOpnd].mode) {
      case V8QImode: case V16QImode:
        break;
      case V4HImode: case V8HImode:
        if (!allow_16bit_polynomials)
          return BadModeError(exp, Result);
        break;
      default:
        return BadModeError(exp, Result);
      }

    } else if (datatype == neon_datatype_float) {

      switch (insn_data[icode].operand[modeCheckOpnd].mode) {
      case V2SFmode: case V4SFmode:
        break;
      default:
        return BadModeError(exp, Result);
      }

    } else {

      switch (insn_data[icode].operand[modeCheckOpnd].mode) {
      case V8QImode: case V16QImode:
        if (!allow_8bit_elements)
          return BadModeError(exp, Result);
        break;
      case V4HImode: case V8HImode:
        if (!allow_16bit_elements)
          return BadModeError(exp, Result);
        break;
      case V2SImode: case V4SImode:
      case V2SFmode: case V4SFmode:
        if (!allow_32bit_elements)
          return BadModeError(exp, Result);
        break;
      case DImode: case V2DImode:
        if (!allow_64bit_elements)
          return BadModeError(exp, Result);
        break;
      default:
        return BadModeError(exp, Result);
      }
    }
  }

  // Now translate the builtin to LLVM.

  switch (neon_code) {
  default:
    assert(0 && "unimplemented builtin");
    break;

  case NEON_BUILTIN_vadd:
    if (datatype == neon_datatype_polynomial)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateAdd(Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vaddl:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vaddls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vaddlu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vaddw:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vaddws;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vaddwu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vhadd:
    if (datatype == neon_datatype_signed)
      intID = (isRounded ?
               Intrinsic::arm_neon_vrhadds :
               Intrinsic::arm_neon_vhadds);
    else if (datatype == neon_datatype_unsigned)
      intID = (isRounded ?
               Intrinsic::arm_neon_vrhaddu :
               Intrinsic::arm_neon_vhaddu);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqadd:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vqadds;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vqaddu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vaddhn:
    if (datatype == neon_datatype_signed ||
        datatype == neon_datatype_unsigned)
      intID = (isRounded ?
               Intrinsic::arm_neon_vraddhn :
               Intrinsic::arm_neon_vaddhn);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vmul_lane:
  case NEON_BUILTIN_vmul_n:
    if (datatype == neon_datatype_polynomial)
      return BadImmediateError(exp, Result);
    // fall through....
  case NEON_BUILTIN_vmul:
    if (neon_code == NEON_BUILTIN_vmul_n) {
      Ops[1] = BuildDup(Ops[0]->getType(), Ops[1], Builder);
    } else if (neon_code == NEON_BUILTIN_vmul_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
      if (!isValidLane(Ops[2], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[1] = BuildDupLane(Ops[1], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_polynomial) {
      intID = Intrinsic::arm_neon_vmulp;
      intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
      Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    } else
      Result = Builder.CreateMul(Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vmla_lane:
  case NEON_BUILTIN_vmla_n:
  case NEON_BUILTIN_vmla:
    if (neon_code == NEON_BUILTIN_vmla_n) {
      Ops[2] = BuildDup(Ops[1]->getType(), Ops[2], Builder);
    } else if (neon_code == NEON_BUILTIN_vmla_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[2].mode);
      if (!isValidLane(Ops[3], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[2] = BuildDupLane(Ops[2], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_polynomial)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateAdd(Ops[0], Builder.CreateMul(Ops[1], Ops[2]));
    break;

  case NEON_BUILTIN_vmls_lane:
  case NEON_BUILTIN_vmls_n:
  case NEON_BUILTIN_vmls:
    if (neon_code == NEON_BUILTIN_vmls_n) {
      Ops[2] = BuildDup(Ops[1]->getType(), Ops[2], Builder);
    } else if (neon_code == NEON_BUILTIN_vmls_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[2].mode);
      if (!isValidLane(Ops[3], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[2] = BuildDupLane(Ops[2], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_polynomial)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateSub(Ops[0], Builder.CreateMul(Ops[1], Ops[2]));
    break;

  case NEON_BUILTIN_vmlal_lane:
  case NEON_BUILTIN_vmlal_n:
  case NEON_BUILTIN_vmlal:
    if (neon_code == NEON_BUILTIN_vmlal_n) {
      Ops[2] = BuildDup(Ops[1]->getType(), Ops[2], Builder);
    } else if (neon_code == NEON_BUILTIN_vmlal_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[2].mode);
      if (!isValidLane(Ops[3], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[2] = BuildDupLane(Ops[2], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vmlals;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vmlalu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vmlsl_lane:
  case NEON_BUILTIN_vmlsl_n:
  case NEON_BUILTIN_vmlsl:
    if (neon_code == NEON_BUILTIN_vmlsl_n) {
      Ops[2] = BuildDup(Ops[1]->getType(), Ops[2], Builder);
    } else if (neon_code == NEON_BUILTIN_vmlsl_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[2].mode);
      if (!isValidLane(Ops[3], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[2] = BuildDupLane(Ops[2], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vmlsls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vmlslu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vqdmulh_lane:
  case NEON_BUILTIN_vqdmulh_n:
  case NEON_BUILTIN_vqdmulh:
    if (neon_code == NEON_BUILTIN_vqdmulh_n) {
      Ops[1] = BuildDup(Ops[0]->getType(), Ops[1], Builder);
    } else if (neon_code == NEON_BUILTIN_vqdmulh_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
      if (!isValidLane(Ops[2], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[1] = BuildDupLane(Ops[1], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_signed)
      intID = (isRounded ?
               Intrinsic::arm_neon_vqrdmulh :
               Intrinsic::arm_neon_vqdmulh);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqdmlal_lane:
  case NEON_BUILTIN_vqdmlal_n:
  case NEON_BUILTIN_vqdmlal:
    if (neon_code == NEON_BUILTIN_vqdmlal_n) {
      Ops[2] = BuildDup(Ops[1]->getType(), Ops[2], Builder);
    } else if (neon_code == NEON_BUILTIN_vqdmlal_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[2].mode);
      if (!isValidLane(Ops[3], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[2] = BuildDupLane(Ops[2], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vqdmlal;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vqdmlsl_lane:
  case NEON_BUILTIN_vqdmlsl_n:
  case NEON_BUILTIN_vqdmlsl:
    if (neon_code == NEON_BUILTIN_vqdmlsl_n) {
      Ops[2] = BuildDup(Ops[1]->getType(), Ops[2], Builder);
    } else if (neon_code == NEON_BUILTIN_vqdmlsl_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[2].mode);
      if (!isValidLane(Ops[3], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[2] = BuildDupLane(Ops[2], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vqdmlsl;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vmull_lane:
  case NEON_BUILTIN_vmull_n:
  case NEON_BUILTIN_vmull:
    if (neon_code == NEON_BUILTIN_vmull_n) {
      Ops[1] = BuildDup(Ops[0]->getType(), Ops[1], Builder);
    } else if (neon_code == NEON_BUILTIN_vmull_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
      if (!isValidLane(Ops[2], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[1] = BuildDupLane(Ops[1], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_polynomial)
      intID = Intrinsic::arm_neon_vmullp;
    else if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vmulls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vmullu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqdmull_n:
  case NEON_BUILTIN_vqdmull_lane:
  case NEON_BUILTIN_vqdmull:
    if (neon_code == NEON_BUILTIN_vqdmull_n) {
      Ops[1] = BuildDup(Ops[0]->getType(), Ops[1], Builder);
    } else if (neon_code == NEON_BUILTIN_vqdmull_lane) {
      unsigned LaneVal;
      unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
      if (!isValidLane(Ops[2], NUnits, &LaneVal))
        return UnexpectedError("%Hinvalid lane number", exp, Result);
      Ops[1] = BuildDupLane(Ops[1], LaneVal, NUnits, Builder);
    }
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vqdmull;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vshl_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckLeftShiftCount, false))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (datatype == neon_datatype_signed ||
        datatype == neon_datatype_unsigned)
      Result = Builder.CreateShl(Ops[0], Ops[1]);
    else
      return BadImmediateError(exp, Result);
    break;

  case NEON_BUILTIN_vshr_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckRightShiftCount, isRounded))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (!isRounded) {
      if (datatype == neon_datatype_signed)
        Result = Builder.CreateAShr(Ops[0], Ops[1]);
      else if (datatype == neon_datatype_unsigned)
        Result = Builder.CreateLShr(Ops[0], Ops[1]);
      else
        return BadImmediateError(exp, Result);
      break;
    }
    // fall through....
  case NEON_BUILTIN_vshl:
    if (datatype == neon_datatype_signed)
      intID = (isRounded ?
               Intrinsic::arm_neon_vrshifts :
               Intrinsic::arm_neon_vshifts);
    else if (datatype == neon_datatype_unsigned)
      intID = (isRounded ?
               Intrinsic::arm_neon_vrshiftu :
               Intrinsic::arm_neon_vshiftu);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vshrn_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckNarrowRightShiftCount, true))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (datatype == neon_datatype_signed ||
        datatype == neon_datatype_unsigned)
      intID = (isRounded ?
               Intrinsic::arm_neon_vrshiftn :
               Intrinsic::arm_neon_vshiftn);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqshl_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckLeftShiftCount, false))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    // fall through....
  case NEON_BUILTIN_vqshl:
    if (datatype == neon_datatype_signed)
      intID = (isRounded ?
               Intrinsic::arm_neon_vqrshifts :
               Intrinsic::arm_neon_vqshifts);
    else if (datatype == neon_datatype_unsigned)
      intID = (isRounded ?
               Intrinsic::arm_neon_vqrshiftu :
               Intrinsic::arm_neon_vqshiftu);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqshlu_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckLeftShiftCount, false))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (datatype != neon_datatype_signed)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vqshiftsu;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqshrn_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckNarrowRightShiftCount, true))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (datatype == neon_datatype_signed)
      intID = (isRounded ?
               Intrinsic::arm_neon_vqrshiftns :
               Intrinsic::arm_neon_vqshiftns);
    else if (datatype == neon_datatype_unsigned)
      intID = (isRounded ?
               Intrinsic::arm_neon_vqrshiftnu :
               Intrinsic::arm_neon_vqshiftnu);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqshrun_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckNarrowRightShiftCount, true))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (datatype == neon_datatype_signed)
      intID = (isRounded ?
               Intrinsic::arm_neon_vqrshiftnsu :
               Intrinsic::arm_neon_vqshiftnsu);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vshll_n:
    if (!BuildShiftCountVector(Ops[1], insn_data[icode].operand[1].mode,
                               CheckLongLeftShiftCount, false))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vshiftls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vshiftlu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vsra_n:
    if (!BuildShiftCountVector(Ops[2], insn_data[icode].operand[1].mode,
                               CheckRightShiftCount, isRounded))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    if (!isRounded) {
      if (datatype == neon_datatype_signed)
        Result = Builder.CreateAShr(Ops[1], Ops[2]);
      else if (datatype == neon_datatype_unsigned)
        Result = Builder.CreateLShr(Ops[1], Ops[2]);
      else
        return BadImmediateError(exp, Result);
    } else {
      if (datatype == neon_datatype_signed)
        intID = (isRounded ?
                 Intrinsic::arm_neon_vrshifts :
                 Intrinsic::arm_neon_vshifts);
      else if (datatype == neon_datatype_unsigned)
        intID = (isRounded ?
                 Intrinsic::arm_neon_vrshiftu :
                 Intrinsic::arm_neon_vshiftu);
      else
        return BadImmediateError(exp, Result);

      intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
      Result = Builder.CreateCall2(intFn, Ops[1], Ops[2]);
    }
    Result = Builder.CreateAdd(Ops[0], Result);
    break;

  case NEON_BUILTIN_vsub:
    if (datatype == neon_datatype_polynomial)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateSub(Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vsubl:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vsubls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vsublu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vsubw:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vsubws;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vsubwu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vqsub:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vqsubs;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vqsubu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vhsub:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vhsubs;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vhsubu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vsubhn:
    if (datatype == neon_datatype_signed ||
        datatype == neon_datatype_unsigned)
      intID = (isRounded ?
               Intrinsic::arm_neon_vrsubhn :
               Intrinsic::arm_neon_vsubhn);
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vceq:
    if (datatype == neon_datatype_float)
      Result = Builder.CreateFCmp(FCmpInst::FCMP_OEQ, Ops[0], Ops[1]);
    else
      Result = Builder.CreateICmp(ICmpInst::ICMP_EQ, Ops[0], Ops[1]);
    Result = Builder.CreateSExt(Result, ResultType);
    break;

  case NEON_BUILTIN_vcge:
    if (datatype == neon_datatype_float)
      Result = Builder.CreateFCmp(FCmpInst::FCMP_OGE, Ops[0], Ops[1]);
    else if (datatype == neon_datatype_signed)
      Result = Builder.CreateICmp(ICmpInst::ICMP_SGE, Ops[0], Ops[1]);
    else if (datatype == neon_datatype_unsigned)
      Result = Builder.CreateICmp(ICmpInst::ICMP_UGE, Ops[0], Ops[1]);
    else
      return BadImmediateError(exp, Result);
    Result = Builder.CreateSExt(Result, ResultType);
    break;

  case NEON_BUILTIN_vcgt:
    if (datatype == neon_datatype_float)
      Result = Builder.CreateFCmp(FCmpInst::FCMP_OGT, Ops[0], Ops[1]);
    else if (datatype == neon_datatype_signed)
      Result = Builder.CreateICmp(ICmpInst::ICMP_SGT, Ops[0], Ops[1]);
    else if (datatype == neon_datatype_unsigned)
      Result = Builder.CreateICmp(ICmpInst::ICMP_UGT, Ops[0], Ops[1]);
    else
      return BadImmediateError(exp, Result);
    Result = Builder.CreateSExt(Result, ResultType);
    break;

  case NEON_BUILTIN_vcage:
    if (datatype != neon_datatype_float)
      return BadImmediateError(exp, Result);

    switch (insn_data[icode].operand[1].mode) {
    case V2SFmode:
      intID = Intrinsic::arm_neon_vacged;
      break;
    case V4SFmode:
      intID = Intrinsic::arm_neon_vacgeq;
      break;
    default:
      return BadModeError(exp, Result);
    }

    intFn = Intrinsic::getDeclaration(TheModule, intID);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vcagt:
    if (datatype != neon_datatype_float)
      return BadImmediateError(exp, Result);

    switch (insn_data[icode].operand[1].mode) {
    case V2SFmode:
      intID = Intrinsic::arm_neon_vacgtd;
      break;
    case V4SFmode:
      intID = Intrinsic::arm_neon_vacgtq;
      break;
    default:
      return BadModeError(exp, Result);
    }

    intFn = Intrinsic::getDeclaration(TheModule, intID);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vtst:
    if (datatype == neon_datatype_float)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateICmp(ICmpInst::ICMP_NE,
                                Builder.CreateAnd(Ops[0], Ops[1]),
                                Context.getConstantAggregateZero(ResultType));
    Result = Builder.CreateSExt(Result, ResultType);
    break;

  case NEON_BUILTIN_vabd:
    if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vabdf;
    else if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vabds;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vabdu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vabdl:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vabdls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vabdlu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vaba:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vabas;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vabau;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vabal:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vabals;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vabalu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vmax:
    if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vmaxf;
    else if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vmaxs;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vmaxu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vmin:
    if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vminf;
    else if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vmins;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vminu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vpadd:
    if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vpaddf;
    else if (datatype == neon_datatype_signed ||
             datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vpaddi;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vpaddl:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vpaddls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vpaddlu;
    else
      return BadImmediateError(exp, Result);

    intOpTypes[0] = ResultType;
    intOpTypes[1] = Ops[0]->getType();
    intFn = Intrinsic::getDeclaration(TheModule, intID, intOpTypes, 2);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vpadal:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vpadals;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vpadalu;
    else
      return BadImmediateError(exp, Result);

    intOpTypes[0] = ResultType;
    intOpTypes[1] = Ops[1]->getType();
    intFn = Intrinsic::getDeclaration(TheModule, intID, intOpTypes, 2);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vpmax:
    if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vpmaxf;
    else if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vpmaxs;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vpmaxu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vpmin:
    if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vpminf;
    else if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vpmins;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vpminu;
    else
      return BadImmediateError(exp, Result);

    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vrecps:
    if (datatype != neon_datatype_float)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vrecps;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vrsqrts:
    if (datatype != neon_datatype_float)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vrsqrts;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vsri_n:
    if (!BuildShiftCountVector(Ops[2], insn_data[icode].operand[1].mode,
                               CheckRightShiftCount, true))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    intID = Intrinsic::arm_neon_vshiftins;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vsli_n:
    if (!BuildShiftCountVector(Ops[2], insn_data[icode].operand[1].mode,
                               CheckLeftShiftCount, false))
      return UnexpectedError("%Hinvalid shift count", exp, Result);
    intID = Intrinsic::arm_neon_vshiftins;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;

  case NEON_BUILTIN_vabs:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vabs;
    else if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vabsf;
    else
      return BadImmediateError(exp, Result);
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vqabs:
    if (datatype != neon_datatype_signed)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vqabs;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vneg:
    if (datatype != neon_datatype_signed &&
        datatype != neon_datatype_float)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateNeg(Ops[0]);
    break;

  case NEON_BUILTIN_vqneg:
    if (datatype != neon_datatype_signed)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vqneg;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vcls:
    if (datatype != neon_datatype_signed)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vcls;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vclz:
    if (datatype != neon_datatype_signed &&
        datatype != neon_datatype_unsigned)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vclz;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vcnt:
    if (datatype == neon_datatype_float)
      return BadImmediateError(exp, Result);
    intID = Intrinsic::arm_neon_vcnt;
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vrecpe:
    if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vrecpe;
    else if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vrecpef;
    else
      return BadImmediateError(exp, Result);
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vrsqrte:
    if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vrsqrte;
    else if (datatype == neon_datatype_float)
      intID = Intrinsic::arm_neon_vrsqrtef;
    else
      return BadImmediateError(exp, Result);
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vmvn:
    if (datatype == neon_datatype_float)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateNot(Ops[0]);
    break;

  case NEON_BUILTIN_vget_lane: {
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
    if (!isValidLane(Ops[1], NUnits))
      return UnexpectedError("%Hinvalid lane number", exp, Result);
    Result = Builder.CreateExtractElement(Ops[0], Ops[1]);
    break;
  }

  case NEON_BUILTIN_vset_lane: {
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[2].mode);
    if (!isValidLane(Ops[2], NUnits))
      return UnexpectedError("%Hinvalid lane number", exp, Result);
    // GCC may promote the scalar argument; cast it back.
    const VectorType *VTy = dyn_cast<const VectorType>(Ops[1]->getType());
    assert(VTy && "expected a vector type for vset_lane vector operand");
    const Type *ElTy = VTy->getElementType();
    if (Ops[0]->getType() != ElTy) {
      assert(!ElTy->isFloatingPoint() &&
             "only integer types expected to be promoted");
      Ops[0] = Builder.CreateTrunc(Ops[0], ElTy);
    }
    Result = Builder.CreateInsertElement(Ops[1], Ops[0], Ops[2]);
    break;
  }

  case NEON_BUILTIN_vcreate:
    Result = Builder.CreateBitCast(Ops[0], ResultType);
    break;

  case NEON_BUILTIN_vdup_n:
    Result = BuildDup(ResultType, Ops[0], Builder);
    break;

  case NEON_BUILTIN_vdup_lane: {
    unsigned LaneVal;
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
    if (!isValidLane(Ops[1], NUnits, &LaneVal))
      return UnexpectedError("%Hinvalid lane number", exp, Result);
    unsigned DstUnits = GET_MODE_NUNITS(insn_data[icode].operand[0].mode);
    Result = BuildDupLane(Ops[0], LaneVal, DstUnits, Builder);
    break;
  }

  case NEON_BUILTIN_vcombine: {
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[0].mode);
    std::vector<Constant*> Idxs;
    for (unsigned i = 0; i != NUnits; ++i)
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, i));
    Result = Builder.CreateShuffleVector(Ops[0], Ops[1],
                                         ConstantVector::get(Idxs));
    break;
  }

  case NEON_BUILTIN_vget_high:
  case NEON_BUILTIN_vget_low: {
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[0].mode);
    std::vector<Constant*> Idxs;
    unsigned Idx = (neon_code == NEON_BUILTIN_vget_low ? 0 : NUnits);
    for (unsigned i = 0; i != NUnits; ++i)
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, Idx++));
    Result = Builder.CreateShuffleVector(Ops[0],
                                         Context.getUndef(Ops[0]->getType()),
                                         ConstantVector::get(Idxs));
    break;
  }

  case NEON_BUILTIN_vmovn:
    if (datatype == neon_datatype_signed ||
        datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vmovn;
    else
      return BadImmediateError(exp, Result);
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vqmovn:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vqmovns;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vqmovnu;
    else
      return BadImmediateError(exp, Result);
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vqmovun:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vqmovnsu;
    else
      return BadImmediateError(exp, Result);
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vmovl:
    if (datatype == neon_datatype_signed)
      intID = Intrinsic::arm_neon_vmovls;
    else if (datatype == neon_datatype_unsigned)
      intID = Intrinsic::arm_neon_vmovlu;
    else
      return BadImmediateError(exp, Result);
    intFn = Intrinsic::getDeclaration(TheModule, intID, &ResultType, 1);
    Result = Builder.CreateCall(intFn, Ops[0]);
    break;

  case NEON_BUILTIN_vext: {
    // Check if immediate operand is valid.
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
    ConstantInt *Imm = dyn_cast<ConstantInt>(Ops[2]);
    if (!Imm)
      return UnexpectedError("%Hinvalid immediate for vext", exp, Result);
    int ImmVal = Imm->getSExtValue();
    if (ImmVal < 0 || ImmVal >= (int)NUnits)
      return UnexpectedError("%Hout of range immediate for vext", exp, Result);
    if (ImmVal == 0) {
      Result = Ops[0];
      break;
    }
    // Translate to a vector shuffle.
    std::vector<Constant*> Idxs;
    for (unsigned i = 0; i != NUnits; ++i)
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, i + ImmVal));
    Result = Builder.CreateShuffleVector(Ops[0], Ops[1],
                                         ConstantVector::get(Idxs));
    break;
  }

  case NEON_BUILTIN_vrev64:
  case NEON_BUILTIN_vrev32:
  case NEON_BUILTIN_vrev16: {
    unsigned ChunkBits = 0;
    switch (neon_code) {
    case NEON_BUILTIN_vrev64: ChunkBits = 64; break;
    case NEON_BUILTIN_vrev32: ChunkBits = 32; break;
    case NEON_BUILTIN_vrev16: ChunkBits = 16; break;
    default: assert(false);
    }
    const VectorType *VTy = dyn_cast<const VectorType>(ResultType);
    assert(VTy && "expected a vector type");
    const Type *ElTy = VTy->getElementType();
    unsigned ChunkElts = ChunkBits / ElTy->getPrimitiveSizeInBits();

    // Translate to a vector shuffle.
    std::vector<Constant*> Idxs;
    unsigned NUnits = VTy->getNumElements();
    for (unsigned c = ChunkElts; c <= NUnits; c += ChunkElts) {
      for (unsigned i = 0; i != ChunkElts; ++i) {
        Idxs.push_back(ConstantInt::get(Type::Int32Ty, c - i - 1));
      }
    }
    Result = Builder.CreateShuffleVector(Ops[0], Context.getUndef(ResultType),
                                         ConstantVector::get(Idxs));
    break;
  }

  case NEON_BUILTIN_vcvt:
    if (FLOAT_MODE_P(insn_data[icode].operand[1].mode)) {
      if (datatype == neon_datatype_unsigned)
        Result = Builder.CreateFPToUI(Ops[0], ResultType);
      else if (datatype == neon_datatype_signed)
        Result = Builder.CreateFPToSI(Ops[0], ResultType);
      else
        return BadImmediateError(exp, Result);
    } else {
      if (datatype == neon_datatype_unsigned)
        Result = Builder.CreateUIToFP(Ops[0], ResultType);
      else if (datatype == neon_datatype_signed)
        Result = Builder.CreateSIToFP(Ops[0], ResultType);
      else
        return BadImmediateError(exp, Result);
    }
    break;

  case NEON_BUILTIN_vcvt_n: {
    // Check if the fractional bits argument is between 1 and 32.
    ConstantInt *FBits = dyn_cast<ConstantInt>(Ops[1]);
    if (!FBits)
      return UnexpectedError("%Hinvalid fractional bit count", exp, Result);
    int FBitsVal = FBits->getSExtValue();
    if (FBitsVal < 1 || FBitsVal > 32)
      return UnexpectedError("%Hinvalid fractional bit count", exp, Result);
    if (FLOAT_MODE_P(insn_data[icode].operand[1].mode)) {
      if (datatype == neon_datatype_unsigned)
        intID = Intrinsic::arm_neon_vcvtfp2fxu;
      else if (datatype == neon_datatype_signed)
        intID = Intrinsic::arm_neon_vcvtfp2fxs;
      else
        return BadImmediateError(exp, Result);
    } else {
      if (datatype == neon_datatype_unsigned)
        intID = Intrinsic::arm_neon_vcvtfxu2fp;
      else if (datatype == neon_datatype_signed)
        intID = Intrinsic::arm_neon_vcvtfxs2fp;
      else
        return BadImmediateError(exp, Result);
    }
    intOpTypes[0] = ResultType;
    intOpTypes[1] = Ops[0]->getType();
    intFn = Intrinsic::getDeclaration(TheModule, intID, intOpTypes, 2);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;
  }

  case NEON_BUILTIN_vbsl:
    Result = Builder.CreateOr(Builder.CreateAnd(Ops[1], Ops[0]),
                              Builder.CreateAnd(Ops[2],
                                                Builder.CreateNot(Ops[0])));
    break;

  case NEON_BUILTIN_vtbl1:
  case NEON_BUILTIN_vtbl2:
  case NEON_BUILTIN_vtbl3:
  case NEON_BUILTIN_vtbl4: {
    unsigned NUnits = Ops[0]->getType()->getPrimitiveSizeInBits() / 8;
    intOpTypes[0] = VectorType::get(IntegerType::get(8), NUnits);
    intID = Intrinsic::arm_neon_vtbl;
    intFn = Intrinsic::getDeclaration(TheModule, intID, intOpTypes, 1);
    Result = Builder.CreateCall2(intFn, Ops[0], Ops[1]);
    break;
  }

  case NEON_BUILTIN_vtbx1:
  case NEON_BUILTIN_vtbx2:
  case NEON_BUILTIN_vtbx3:
  case NEON_BUILTIN_vtbx4: {
    unsigned NUnits = Ops[1]->getType()->getPrimitiveSizeInBits() / 8;
    intOpTypes[0] = VectorType::get(IntegerType::get(8), NUnits);
    intID = Intrinsic::arm_neon_vtbx;
    intFn = Intrinsic::getDeclaration(TheModule, intID, intOpTypes, 1);
    Result = Builder.CreateCall3(intFn, Ops[0], Ops[1], Ops[2]);
    break;
  }

  case NEON_BUILTIN_vtrn: {
    // Translate this to a vector shuffle.
    std::vector<Constant*> Idxs;
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
    for (unsigned EvenOdd = 0; EvenOdd != 2; ++EvenOdd) {
      for (unsigned i = 0; i < NUnits; i += 2) {
        Idxs.push_back(ConstantInt::get(Type::Int32Ty, i + EvenOdd));
        Idxs.push_back(ConstantInt::get(Type::Int32Ty,
                                              i + NUnits + EvenOdd));
      }
    }
    Result = Builder.CreateShuffleVector(Ops[1], Ops[2],
                                         ConstantVector::get(Idxs));
    Type *PtrTy = Result->getType()->getPointerTo();
    Builder.CreateStore(Result, BitCastToType(Ops[0], PtrTy));
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vzip: {
    // Translate this to a vector shuffle.
    std::vector<Constant*> Idxs;
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
    for (unsigned i = 0; i != NUnits; ++i) {
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, i));
      Idxs.push_back(ConstantInt::get(Type::Int32Ty, i + NUnits));
    }
    Result = Builder.CreateShuffleVector(Ops[1], Ops[2],
                                         ConstantVector::get(Idxs));
    Type *PtrTy = Result->getType()->getPointerTo();
    Builder.CreateStore(Result, BitCastToType(Ops[0], PtrTy));
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vuzp: {
    // Translate this to a vector shuffle.
    std::vector<Constant*> Idxs;
    unsigned NUnits = GET_MODE_NUNITS(insn_data[icode].operand[1].mode);
    for (unsigned EvenOdd = 0; EvenOdd != 2; ++EvenOdd) {
      for (unsigned i = 0; i != NUnits; ++i)
        Idxs.push_back(ConstantInt::get(Type::Int32Ty, 2 * i + EvenOdd));
    }
    Result = Builder.CreateShuffleVector(Ops[1], Ops[2],
                                         ConstantVector::get(Idxs));
    Type *PtrTy = Result->getType()->getPointerTo();
    Builder.CreateStore(Result, BitCastToType(Ops[0], PtrTy));
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vreinterpretv8qi:
  case NEON_BUILTIN_vreinterpretv4hi:
  case NEON_BUILTIN_vreinterpretv2si:
  case NEON_BUILTIN_vreinterpretv2sf:
  case NEON_BUILTIN_vreinterpretdi:
  case NEON_BUILTIN_vreinterpretv16qi:
  case NEON_BUILTIN_vreinterpretv8hi:
  case NEON_BUILTIN_vreinterpretv4si:
  case NEON_BUILTIN_vreinterpretv4sf:
  case NEON_BUILTIN_vreinterpretv2di:
    Result = Builder.CreateBitCast(Ops[0], ResultType);
    break;

  case NEON_BUILTIN_vld1: {
    const VectorType *VTy = dyn_cast<const VectorType>(ResultType);
    assert(VTy && "expected a vector type");
    if (VTy->getElementType()->isFloatingPoint())
      intID = Intrinsic::arm_neon_vld1f;
    else
      intID = Intrinsic::arm_neon_vld1i;
    intFn = Intrinsic::getDeclaration(TheModule, intID, (const Type **)&VTy, 1);
    Type *VPTy = PointerType::getUnqual(Type::Int8Ty);
    Result = Builder.CreateCall(intFn, BitCastToType(Ops[0], VPTy));
    break;
  }

  case NEON_BUILTIN_vld2:
  case NEON_BUILTIN_vld3:
  case NEON_BUILTIN_vld4: {
    const StructType *STy = dyn_cast<const StructType>(ResultType);
    assert(STy && "expected a struct type");
    const VectorType *VTy = dyn_cast<const VectorType>(STy->getElementType(0));
    assert(VTy && "expected a vector type");
    if (VTy->getElementType()->isFloatingPoint()) {
      switch (neon_code) {
      case NEON_BUILTIN_vld2: intID = Intrinsic::arm_neon_vld2f; break;
      case NEON_BUILTIN_vld3: intID = Intrinsic::arm_neon_vld3f; break;
      case NEON_BUILTIN_vld4: intID = Intrinsic::arm_neon_vld4f; break;
      default: assert(false);
      }
    } else {
      switch (neon_code) {
      case NEON_BUILTIN_vld2: intID = Intrinsic::arm_neon_vld2i; break;
      case NEON_BUILTIN_vld3: intID = Intrinsic::arm_neon_vld3i; break;
      case NEON_BUILTIN_vld4: intID = Intrinsic::arm_neon_vld4i; break;
      default: assert(false);
      }
    }
    intFn = Intrinsic::getDeclaration(TheModule, intID, (const Type **)&VTy, 1);
    Type *VPTy = PointerType::getUnqual(Type::Int8Ty);
    Result = Builder.CreateCall(intFn, BitCastToType(Ops[0], VPTy));
    Builder.CreateStore(Result, DestLoc->Ptr);
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vld1_lane:
  case NEON_BUILTIN_vld2_lane:
  case NEON_BUILTIN_vld3_lane:
  case NEON_BUILTIN_vld4_lane: {
    const VectorType *VTy =
      GetVldstType(exp, insn_data[icode].operand[0].mode);
    unsigned LaneVal, NumVecs;
    switch (neon_code) {
    case NEON_BUILTIN_vld1_lane: NumVecs = 1; break;
    case NEON_BUILTIN_vld2_lane: NumVecs = 2; break;
    case NEON_BUILTIN_vld3_lane: NumVecs = 3; break;
    case NEON_BUILTIN_vld4_lane: NumVecs = 4; break;
    default: assert(false);
    }
    unsigned NUnits = VTy->getNumElements() / NumVecs;
    if (!isValidLane(Ops[2], NUnits, &LaneVal))
      return UnexpectedError("%Hinvalid lane number", exp, Result);
    Result = BitCastToType(Ops[1], VTy);
    for (unsigned n = 0; n != NumVecs; ++n) {
      Value *Addr = (n == 0) ? Ops[0] :
        Builder.CreateGEP(Ops[0], ConstantInt::get(Type::Int32Ty, n));
      Value *Elt = Builder.CreateLoad(Addr);
      Value *Ndx = ConstantInt::get(Type::Int32Ty,
                                          LaneVal + (n * NUnits));
      Result = Builder.CreateInsertElement(Result, Elt, Ndx);
    }
    Type *PtrToWideVec = PointerType::getUnqual(VTy);
    Builder.CreateStore(Result, BitCastToType(DestLoc->Ptr, PtrToWideVec));
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vld1_dup:
  case NEON_BUILTIN_vld2_dup:
  case NEON_BUILTIN_vld3_dup:
  case NEON_BUILTIN_vld4_dup: {
    const VectorType *VTy =
      GetVldstType(exp, insn_data[icode].operand[0].mode);
    unsigned NumVecs;
    switch (neon_code) {
    case NEON_BUILTIN_vld1_dup: NumVecs = 1; break;
    case NEON_BUILTIN_vld2_dup: NumVecs = 2; break;
    case NEON_BUILTIN_vld3_dup: NumVecs = 3; break;
    case NEON_BUILTIN_vld4_dup: NumVecs = 4; break;
    default: assert(false);
    }
    unsigned NUnits = VTy->getNumElements() / NumVecs;
    Result = Context.getUndef(VTy);
    for (unsigned n = 0; n != NumVecs; ++n) {
      Value *Addr = (n == 0) ? Ops[0] :
        Builder.CreateGEP(Ops[0], ConstantInt::get(Type::Int32Ty, n));
      Value *Elt = Builder.CreateLoad(Addr);
      // Insert the value into one lane of the result.
      Value *Ndx = ConstantInt::get(Type::Int32Ty, n * NUnits);
      Result = Builder.CreateInsertElement(Result, Elt, Ndx);
    }
    // Use a shuffle to move the value into the other lanes of the vector.
    if (NUnits > 1) {
      std::vector<Constant*> Idxs;
      for (unsigned n = 0; n != NumVecs; ++n) {
        for (unsigned i = 0; i != NUnits; ++i)
          Idxs.push_back(ConstantInt::get(Type::Int32Ty, n * NUnits));
      }
      Result = Builder.CreateShuffleVector(Result, Context.getUndef(VTy),
                                           ConstantVector::get(Idxs));
    }
    Type *PtrToWideVec = PointerType::getUnqual(VTy);
    Builder.CreateStore(Result, BitCastToType(DestLoc->Ptr, PtrToWideVec));
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vst1: {
    const VectorType *VTy = dyn_cast<const VectorType>(Ops[1]->getType());
    assert(VTy && "expected a vector type");
    if (VTy->getElementType()->isFloatingPoint())
      intID = Intrinsic::arm_neon_vst1f;
    else
      intID = Intrinsic::arm_neon_vst1i;
    intFn = Intrinsic::getDeclaration(TheModule, intID, (const Type **)&VTy, 1);
    Type *VPTy = PointerType::getUnqual(Type::Int8Ty);
    Builder.CreateCall2(intFn, BitCastToType(Ops[0], VPTy), Ops[1]);
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vst2:
  case NEON_BUILTIN_vst3:
  case NEON_BUILTIN_vst4: {
    const StructType *STy = dyn_cast<const StructType>(Ops[1]->getType());
    assert(STy && "expected a struct type");
    const VectorType *VTy = dyn_cast<const VectorType>(STy->getElementType(0));
    assert(VTy && "expected a vector type");
    if (VTy->getElementType()->isFloatingPoint()) {
      switch (neon_code) {
      case NEON_BUILTIN_vst2: intID = Intrinsic::arm_neon_vst2f; break;
      case NEON_BUILTIN_vst3: intID = Intrinsic::arm_neon_vst3f; break;
      case NEON_BUILTIN_vst4: intID = Intrinsic::arm_neon_vst4f; break;
      default: assert(false);
      }
    } else {
      switch (neon_code) {
      case NEON_BUILTIN_vst2: intID = Intrinsic::arm_neon_vst2i; break;
      case NEON_BUILTIN_vst3: intID = Intrinsic::arm_neon_vst3i; break;
      case NEON_BUILTIN_vst4: intID = Intrinsic::arm_neon_vst4i; break;
      default: assert(false);
      }
    }
    intFn = Intrinsic::getDeclaration(TheModule, intID, (const Type **)&VTy, 1);
    unsigned NumVecs = 0;
    switch (neon_code) {
    case NEON_BUILTIN_vst2: NumVecs = 2; break;
    case NEON_BUILTIN_vst3: NumVecs = 3; break;
    case NEON_BUILTIN_vst4: NumVecs = 4; break;
    default: assert(false);
    }
    std::vector<Value*> Args;
    Type *VPTy = PointerType::getUnqual(Type::Int8Ty);
    Args.push_back(BitCastToType(Ops[0], VPTy));
    for (unsigned n = 0; n < NumVecs; ++n) {
      Args.push_back(Builder.CreateExtractValue(Ops[1], n));
    }
    Builder.CreateCall(intFn, Args.begin(), Args.end());
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vst1_lane:
  case NEON_BUILTIN_vst2_lane:
  case NEON_BUILTIN_vst3_lane:
  case NEON_BUILTIN_vst4_lane: {
    const VectorType *VTy =
      GetVldstType(exp, insn_data[icode].operand[1].mode);
    unsigned LaneVal, NumVecs;
    switch (neon_code) {
    case NEON_BUILTIN_vst1_lane: NumVecs = 1; break;
    case NEON_BUILTIN_vst2_lane: NumVecs = 2; break;
    case NEON_BUILTIN_vst3_lane: NumVecs = 3; break;
    case NEON_BUILTIN_vst4_lane: NumVecs = 4; break;
    default: assert(false);
    }
    unsigned NUnits = VTy->getNumElements() / NumVecs;
    if (!isValidLane(Ops[2], NUnits, &LaneVal))
      return UnexpectedError("%Hinvalid lane number", exp, Result);
    Value *Tmp = CreateTemporary(VTy);
    Type *PtrToStruct = PointerType::getUnqual(Ops[1]->getType());
    Builder.CreateStore(Ops[1], BitCastToType(Tmp, PtrToStruct));
    Value *Vec = Builder.CreateLoad(Tmp);
    for (unsigned n = 0; n != NumVecs; ++n) {
      Value *Addr = (n == 0) ? Ops[0] :
        Builder.CreateGEP(Ops[0], ConstantInt::get(Type::Int32Ty, n));
      Value *Ndx = ConstantInt::get(Type::Int32Ty,
                                          LaneVal + (n * NUnits));
      Builder.CreateStore(Builder.CreateExtractElement(Vec, Ndx), Addr);
    }
    Result = 0;
    break;
  }

  case NEON_BUILTIN_vand:
    if (datatype != neon_datatype_signed &&
        datatype != neon_datatype_unsigned)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateAnd(Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vorr:
    if (datatype != neon_datatype_signed &&
        datatype != neon_datatype_unsigned)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateOr(Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_veor:
    if (datatype != neon_datatype_signed &&
        datatype != neon_datatype_unsigned)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateXor(Ops[0], Ops[1]);
    break;

  case NEON_BUILTIN_vbic:
    if (datatype != neon_datatype_signed &&
        datatype != neon_datatype_unsigned)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateAnd(Ops[0], Builder.CreateNot(Ops[1]));
    break;

  case NEON_BUILTIN_vorn:
    if (datatype != neon_datatype_signed &&
        datatype != neon_datatype_unsigned)
      return BadImmediateError(exp, Result);
    Result = Builder.CreateOr(Ops[0], Builder.CreateNot(Ops[1]));
    break;
  }

  return true;
}

/* LLVM LOCAL end (ENTIRE FILE!)  */

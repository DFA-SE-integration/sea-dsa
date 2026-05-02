#include "seadsa/PtrTypeUtils.hh"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"

namespace seadsa {

llvm::Type *recoverPointeeType(const llvm::Value *ptr) {
  if (!ptr) return nullptr;

  if (auto *AI = llvm::dyn_cast<llvm::AllocaInst>(ptr))
    return AI->getAllocatedType();
  if (auto *GV = llvm::dyn_cast<llvm::GlobalVariable>(ptr))
    return GV->getValueType();
  if (auto *GEP = llvm::dyn_cast<llvm::GEPOperator>(ptr))
    return GEP->getSourceElementType();
  if (auto *LI = llvm::dyn_cast<llvm::LoadInst>(ptr))
    return LI->getType();
  if (auto *BC = llvm::dyn_cast<llvm::BitCastOperator>(ptr))
    return recoverPointeeType(BC->getOperand(0));
  if (auto *ASC = llvm::dyn_cast<llvm::AddrSpaceCastOperator>(ptr))
    return recoverPointeeType(ASC->getOperand(0));
  if (auto *CE = llvm::dyn_cast<llvm::ConstantExpr>(ptr)) {
    if (CE->isCast()) return recoverPointeeType(CE->getOperand(0));
  }

  return nullptr;
}

llvm::Type *getLoadedType(const llvm::LoadInst &li) { return li.getType(); }

llvm::Type *getGEPSourceElementType(const llvm::GetElementPtrInst &gep) {
  return gep.getSourceElementType();
}

} // namespace seadsa

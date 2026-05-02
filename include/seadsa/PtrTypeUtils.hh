#pragma once

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Value.h"

namespace seadsa {

llvm::Type *recoverPointeeType(const llvm::Value *ptr);
llvm::Type *getLoadedType(const llvm::LoadInst &li);
llvm::Type *getGEPSourceElementType(const llvm::GetElementPtrInst &gep);

} // namespace seadsa

#include "seadsa/support/RemovePtrToInt.hh"

#include "seadsa/InitializePasses.hh"

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"

namespace seadsa {

using namespace llvm;

bool RemovePtrToInt::runOnFunction(Function &F) {
  (void)F;
  return false;
}

char seadsa::RemovePtrToInt::ID = 0;

void RemovePtrToInt::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<llvm::DominatorTreeWrapperPass>();
  AU.addRequired<llvm::TargetLibraryInfoWrapperPass>();
}

Pass *createRemovePtrToIntPass() { return new RemovePtrToInt(); }
} // namespace seadsa

using namespace seadsa;
INITIALIZE_PASS_BEGIN(RemovePtrToInt, "sea-remove-ptrtoint",
                      "Remove ptrtoint instructions", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(RemovePtrToInt, "sea-remove-ptrtoint",
                    "Remove ptrtoint instructions", false, false)

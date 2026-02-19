// ==- ValidateAliasTests.cc - Validate alias checks from Test-Suite ==//

#include "ValidateAliasTests.hh"
#include "seadsa/SeaDsaAliasAnalysis.hh"

#include "llvm/ADT/Optional.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <unordered_set>

using namespace llvm;
using namespace seadsa;

namespace {

const std::unordered_set<std::string> &getAliasCheckNames() {
  static const std::unordered_set<std::string> Names = {
      "MAYALIAS", "NOALIAS", "MUSTALIAS", "PARTIALALIAS",
      "_Z8MAYALIASPvS_", "_Z8MAYALIASPvS0_",
      "_Z7NOALIASPvS_",  "_Z7NOALIASPvS0_",
      "_Z9MUSTALIASPvS_", "_Z9MUSTALIASPvS0_",
      "_Z12PARTIALALIASPvS_", "_Z12PARTIALALIASPvS0_",
  };
  return Names;
}

std::string formatSourceLoc(const Instruction *I) {
  if (auto *DL = I->getDebugLoc().get()) {
    if (auto *File = DL->getFile()) {
      std::string filename = File->getFilename().str();
      unsigned line = DL->getLine();
      unsigned col = DL->getColumn();
      return filename + ":" + std::to_string(line) + ":" + std::to_string(col);
    }
  }
  return "(unknown)";
}

bool isAliasCheckCall(const Function *F) {
  if (!F)
    return false;
  return getAliasCheckNames().count(F->getName().str()) != 0;
}

enum class CheckKind { MAYALIAS, NOALIAS, MUSTALIAS, PARTIALALIAS };

llvm::Optional<CheckKind> getCheckKind(StringRef Name) {
  if (Name == "MAYALIAS" || Name.startswith("_Z8MAYALIAS"))
    return CheckKind::MAYALIAS;
  if (Name == "NOALIAS" || Name.startswith("_Z7NOALIAS"))
    return CheckKind::NOALIAS;
  if (Name == "MUSTALIAS" || Name.startswith("_Z9MUSTALIAS"))
    return CheckKind::MUSTALIAS;
  if (Name == "PARTIALALIAS" || Name.startswith("_Z12PARTIALALIAS"))
    return CheckKind::PARTIALALIAS;
  return llvm::None;
}

bool checkSucceeded(CheckKind Kind, AliasResult Result) {
  switch (Kind) {
  case CheckKind::MAYALIAS:
  case CheckKind::MUSTALIAS:
    return Result == AliasResult::MayAlias || Result == AliasResult::MustAlias;
  case CheckKind::NOALIAS:
    return Result == AliasResult::NoAlias;
  case CheckKind::PARTIALALIAS:
    return Result == AliasResult::MayAlias || Result == AliasResult::PartialAlias;
  }
  return false;
}

const char *checkKindStr(CheckKind Kind) {
  switch (Kind) {
  case CheckKind::MAYALIAS:    return "MAYALIAS";
  case CheckKind::NOALIAS:     return "NOALIAS";
  case CheckKind::MUSTALIAS:   return "MUSTALIAS";
  case CheckKind::PARTIALALIAS: return "PARTIALALIAS";
  }
  return "?";
}

} // namespace

bool seadsa::runValidateAliasTests(Module &M, SeaDsaAAResult &AA) {
  const DataLayout &DL = M.getDataLayout();
  AAQueryInfo AAQI(nullptr);
  bool anyFailure = false;

  for (Function &F : M) {
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        auto *CI = dyn_cast<CallBase>(&I);
        if (!CI)
          continue;
        const Function *Callee = CI->getCalledFunction();
        if (!Callee || !isAliasCheckCall(Callee))
          continue;
        if (CI->arg_size() < 2)
          continue;

        llvm::Optional<CheckKind> Kind = getCheckKind(Callee->getName());
        if (!Kind.hasValue())
          continue;

        Value *V1 = CI->getArgOperand(0);
        Value *V2 = CI->getArgOperand(1);

        // Create MemoryLocation for each argument
        MemoryLocation MemLoc1 = MemoryLocation::getBeforeOrAfter(V1);
        MemoryLocation MemLoc2 = MemoryLocation::getBeforeOrAfter(V2);

        AliasResult Result = AA.alias(MemLoc1, MemLoc2, AAQI);
        CheckKind KindVal = Kind.getValue();
        bool success = checkSucceeded(KindVal, Result);
        std::string Loc = formatSourceLoc(&I);

        if (success) {
          llvm::outs() << "\t SUCCESS :" << checkKindStr(KindVal)
                       << " check at (" << Loc << ")\n";
        } else {
          llvm::errs() << "\t FAILURE :" << checkKindStr(KindVal)
                       << " check at (" << Loc << ")\n";
          anyFailure = true;
        }
      }
    }
  }

  return !anyFailure;
}

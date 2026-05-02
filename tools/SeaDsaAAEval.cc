// ==- SeaDsaAAEval.cc - SeaDsa AA precision evaluator ==//

#include "SeaDsaAAEval.hh"
#include "seadsa/SeaDsaAliasAnalysis.hh"

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <utility>

using namespace llvm;
using namespace seadsa;

namespace {

void printPercent(int64_t Num, int64_t Sum) {
  if (Sum == 0) {
    errs() << "(0.0%)\n";
    return;
  }
  errs() << "(" << Num * 100LL / Sum << "." << ((Num * 1000LL / Sum) % 10)
         << "%)\n";
}

} // anonymous namespace

void seadsa::runSeaDsaAAEvaluator(Module &M, SeaDsaAAResult &AA) {
  const DataLayout &DL = M.getDataLayout();

  int64_t NoAliasCount = 0;
  int64_t MayAliasCount = 0;
  int64_t PartialAliasCount = 0;
  int64_t MustAliasCount = 0;

  for (Function &F : M) {
    if (F.isDeclaration()) continue;

    SetVector<std::pair<const Value *, Type *>> Pointers;
    for (Instruction &Inst : instructions(F)) {
      if (auto *LI = dyn_cast<LoadInst>(&Inst)) {
        Pointers.insert({LI->getPointerOperand(), LI->getType()});
      } else if (auto *SI = dyn_cast<StoreInst>(&Inst)) {
        Pointers.insert(
            {SI->getPointerOperand(), SI->getValueOperand()->getType()});
      }
    }

    // iterate over the worklist, and run the full (n^2)/2 disambiguations.
    for (auto I1 = Pointers.begin(), E = Pointers.end(); I1 != E; ++I1) {
      LocationSize Size1 =
          LocationSize::precise(DL.getTypeStoreSize(I1->second));
      for (auto I2 = Pointers.begin(); I2 != I1; ++I2) {
        LocationSize Size2 =
            LocationSize::precise(DL.getTypeStoreSize(I2->second));
        AliasResult AR =
            AA.alias(MemoryLocation(I1->first, Size1),
                     MemoryLocation(I2->first, Size2));
        switch (AR) {
        case AliasResult::NoAlias:      ++NoAliasCount; break;
        case AliasResult::MayAlias:     ++MayAliasCount; break;
        case AliasResult::PartialAlias: ++PartialAliasCount; break;
        case AliasResult::MustAlias:    ++MustAliasCount; break;
        }
      }
    }
  }

  const int64_t AliasSum =
      NoAliasCount + MayAliasCount + PartialAliasCount + MustAliasCount;

  errs() << "===== Alias Analysis Evaluator Report =====\n";
  if (AliasSum == 0) {
    errs() << "  Alias Analysis Evaluator Summary: No pointers!\n";
    return;
  }

  errs() << "  " << AliasSum << " Total Alias Queries Performed\n";
  errs() << "  " << NoAliasCount << " no alias responses ";
  printPercent(NoAliasCount, AliasSum);
  errs() << "  " << MayAliasCount << " may alias responses ";
  printPercent(MayAliasCount, AliasSum);
  errs() << "  " << PartialAliasCount << " partial alias responses ";
  printPercent(PartialAliasCount, AliasSum);
  errs() << "  " << MustAliasCount << " must alias responses ";
  printPercent(MustAliasCount, AliasSum);
  errs() << "  Alias Analysis Evaluator Pointer Alias Summary: "
         << NoAliasCount * 100 / AliasSum << "%/"
         << MayAliasCount * 100 / AliasSum << "%/"
         << PartialAliasCount * 100 / AliasSum << "%/"
         << MustAliasCount * 100 / AliasSum << "%\n";
}

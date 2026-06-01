#include "seadsa/SeaDsaAliasAnalysis.hh"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/Global.hh"
#include "seadsa/Graph.hh"
#include "seadsa/support/Debug.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include <optional>

#define DEBUG_TYPE "sea-aa"

using namespace llvm;
using namespace seadsa;

namespace {

// Reimplementation of llvm::cflaa::parentFunctionOfValue, which is no longer
// exported in LLVM 20.
const Function *parentFunctionOfValue(const Value *V) {
  if (auto *I = dyn_cast<Instruction>(V)) {
    if (const BasicBlock *BB = I->getParent())
      return BB->getParent();
    return nullptr;
  }
  if (auto *A = dyn_cast<Argument>(V))
    return A->getParent();
  return nullptr;
}

uint64_t storageSize(const Type *t, const DataLayout &dl) {
  return dl.getTypeStoreSize(const_cast<Type *>(t));
}

std::optional<uint64_t> sizeOf(const Graph::Set &types, const DataLayout &dl) {
  if (types.isEmpty()) return 0;

  uint64_t sz = storageSize(*(types.begin()), dl);
  if (types.isSingleton()) return sz;

  auto it = types.begin();
  ++it;
  if (std::all_of(it, types.end(), [dl, sz](const Type *t) {
        return (storageSize(t, dl) == sz);
      })) {
    return sz;
  }
  return std::nullopt;
}

bool mayAlias(const Cell &c1, const Cell &c2, const DataLayout &dl) {
  auto maybeUnsafe = [](const Node *n) {
    return n->isIntToPtr() || n->isPtrToInt() || n->isIncomplete() ||
           n->isUnknown();
  };

  if (c1.isNull() || c2.isNull()) return true;

  const Node *n1 = c1.getNode();
  const Node *n2 = c2.getNode();

  if (maybeUnsafe(n1) || maybeUnsafe(n2)) return true;

  if (n1 != n2) return false; // different nodes cannot alias

  if (n1->isOffsetCollapsed()) return true;

  unsigned o1, o2;
  if (c1.getOffset() <= c2.getOffset()) {
    o1 = c1.getOffset();
    o2 = c2.getOffset();
  } else {
    o1 = c2.getOffset();
    o2 = c1.getOffset();
  }
  assert(o1 <= o2);

  if (!n1->hasAccessedType(o1)) return true;

  auto sizeOfOffset1 = sizeOf(n1->getAccessedType(o1), dl);
  if (!sizeOfOffset1.has_value()) return true;

  return (o1 + sizeOfOffset1.value()) >= o2;
}

} // anonymous namespace

namespace seadsa {

SeaDsaAAResult::SeaDsaAAResult(TargetLibraryInfoWrapperPass &tliWrapper,
                               AllocWrapInfo &awi, DsaLibFuncInfo &dlfi)
    : m_tliWrapper(tliWrapper), m_awi(awi), m_dlfi(dlfi) {}

SeaDsaAAResult::SeaDsaAAResult(SeaDsaAAResult &&RHS)
    : m_tliWrapper(RHS.m_tliWrapper), m_dl(RHS.m_dl), m_awi(RHS.m_awi),
      m_dlfi(RHS.m_dlfi), m_module(RHS.m_module), m_fac(std::move(RHS.m_fac)),
      m_cg(std::move(RHS.m_cg)), m_dsa(std::move(RHS.m_dsa)) {
  RHS.m_module = nullptr;
  RHS.m_dl = nullptr;
}

SeaDsaAAResult::~SeaDsaAAResult() = default;

void SeaDsaAAResult::runOnModule(Module &M) {
  if (m_dsa && m_module == &M) return; // already run on this module

  m_module = &M;
  m_dl = &M.getDataLayout();
  m_fac = std::make_unique<Graph::SetFactory>();
  m_cg = std::make_unique<CallGraph>(M);
  m_awi.initialize(M, nullptr);
  m_dsa = std::make_unique<ContextInsensitiveGlobalAnalysis>(
      *m_dl, m_tliWrapper, m_awi, m_dlfi, *m_cg, *m_fac,
      /*useFlatMemory=*/false);
  DOG(errs() << "Running SeaDsaAA.\n");
  m_dsa->runOnModule(M);
}

Graph *SeaDsaAAResult::getGraph(const Function &F) {
  if (!m_dsa || !m_dsa->hasGraph(F))
    return nullptr;
  return &m_dsa->getGraph(F);
}

const Graph *SeaDsaAAResult::getGraph(const Function &F) const {
  if (!m_dsa || !m_dsa->hasGraph(F))
    return nullptr;
  return &m_dsa->getGraph(F);
}

AliasResult SeaDsaAAResult::alias(const MemoryLocation &LocA,
                                  const MemoryLocation &LocB) {
  DOG(errs() << "SeaDsaAA --- Alias query: " << *LocA.Ptr << " and "
             << *LocB.Ptr << "\n\n";);

  const Value *ValA = LocA.Ptr;
  const Value *ValB = LocB.Ptr;

  if (!ValA || !ValB) return AliasResult(AliasResult::MayAlias);
  if (!ValA->getType()->isPointerTy() || !ValB->getType()->isPointerTy())
    return AliasResult(AliasResult::NoAlias);
  if (ValA == ValB) return AliasResult(AliasResult::MustAlias);

  if (!m_dsa) return AliasResult(AliasResult::MayAlias);

  const Function *FnA = parentFunctionOfValue(ValA);
  const Function *FnB = parentFunctionOfValue(ValB);
  if (!FnA || !FnB) return AliasResult(AliasResult::MayAlias);

  // SeaDsa only handles intra-procedural queries at the moment.
  if (FnA != FnB) {
    DOG(errs() << "SeaDsaAA does not handle inter-procedural queries.\n");
    return AliasResult(AliasResult::MayAlias);
  }

  assert(m_dl);
  Graph &gA = m_dsa->getGraph(*FnA);
  if (gA.hasCell(*ValA) && gA.hasCell(*ValB)) {
    const Cell &c1 = gA.getCell(*ValA);
    const Cell &c2 = gA.getCell(*ValB);

    if (c1.getNode() == c2.getNode() && c1.getOffset() == c2.getOffset()) {
      const Node *N = c1.getNode();
      if (!N->isIntToPtr() && !N->isPtrToInt() && !N->isIncomplete() &&
          !N->isUnknown() && !N->isOffsetCollapsed() && !N->isArray()) {
        return AliasResult(AliasResult::MustAlias);
      }
    }

    if (!::mayAlias(c1, c2, *m_dl)) return AliasResult(AliasResult::NoAlias);
  }

  return AliasResult(AliasResult::MayAlias);
}

AliasResult SeaDsaAAResult::alias(const Value *V1, const Value *V2) {
  return alias(MemoryLocation::getBeforeOrAfter(V1),
               MemoryLocation::getBeforeOrAfter(V2));
}

} // namespace seadsa

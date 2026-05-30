// ==- SeaDsaAliasAnalysis.hh - DSA-based Alias Analysis  ==//
//
// LLVM 20 port:
//   * `AAResultBase<T>` was dropped from LLVM 20; the SeaDsa AA is now a
//     standalone provider used directly by our driver/evaluator rather than
//     plugged into the LLVM AA aggregation pipeline.
//   * `SeaDsaAAWrapperPass` is gone for the same reason.
//
#pragma once

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include "seadsa/Graph.hh"

#include <memory>

namespace llvm {
class CallGraph;
class DataLayout;
class Function;
class Module;
class Value;
} // namespace llvm

namespace seadsa {

class AllocWrapInfo;
class DsaLibFuncInfo;
class ContextInsensitiveGlobalAnalysis;

/// Standalone SeaDsa-based alias analysis.
///
/// Use \c runOnModule once, then repeatedly issue \c alias() queries.
class SeaDsaAAResult {
public:
  explicit SeaDsaAAResult(llvm::TargetLibraryInfoWrapperPass &tliWrapper,
                          AllocWrapInfo &AWI, DsaLibFuncInfo &dlfi);

  SeaDsaAAResult(const SeaDsaAAResult &) = delete;
  SeaDsaAAResult &operator=(const SeaDsaAAResult &) = delete;
  SeaDsaAAResult(SeaDsaAAResult &&RHS);
  ~SeaDsaAAResult();

  /// Build the underlying SeaDsa graph for \p M.  Safe to call multiple times;
  /// re-runs only happen when the module handle changes.
  void runOnModule(llvm::Module &M);

  /// After \c runOnModule, return the DSA graph for \p F, or nullptr if none.
  Graph *getGraph(const llvm::Function &F);
  const Graph *getGraph(const llvm::Function &F) const;

  /// Query whether two pointer values may alias.
  ///
  /// Falls back to \c MayAlias whenever SeaDsa cannot prove disjointness or
  /// the values live in different functions / modules.
  llvm::AliasResult alias(const llvm::MemoryLocation &LocA,
                          const llvm::MemoryLocation &LocB);

  llvm::AliasResult alias(const llvm::Value *V1, const llvm::Value *V2);

private:
  llvm::TargetLibraryInfoWrapperPass &m_tliWrapper;
  const llvm::DataLayout *m_dl = nullptr;
  AllocWrapInfo &m_awi;
  DsaLibFuncInfo &m_dlfi;

  llvm::Module *m_module = nullptr;
  std::unique_ptr<Graph::SetFactory> m_fac;
  std::unique_ptr<llvm::CallGraph> m_cg;
  std::unique_ptr<ContextInsensitiveGlobalAnalysis> m_dsa;
};

} // namespace seadsa

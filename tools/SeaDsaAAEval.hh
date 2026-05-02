// ==- SeaDsaAAEval.hh - SeaDsa AA precision evaluator ==//
//
// Replacement for the stock llvm::createAAEvalPass() that is no longer
// available as a legacy pass in LLVM 20.  The output format mirrors
// llvm::AAEvaluator so that existing log parsers keep working.
//
#pragma once

namespace llvm {
class Module;
} // namespace llvm

namespace seadsa {

class SeaDsaAAResult;

/// Walk every function in \p M, collect the pointer operands of load/store
/// instructions, query \p AA on every N*(N-1)/2 pair and print an
/// "Alias Analysis Evaluator Report" compatible with LLVM's AAEvaluator.
void runSeaDsaAAEvaluator(llvm::Module &M, SeaDsaAAResult &AA);

} // namespace seadsa

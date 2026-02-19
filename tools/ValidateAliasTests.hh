// ==- ValidateAliasTests.hh - Validate alias checks from Test-Suite ==//

#pragma once

namespace llvm {
class Module;
} // namespace llvm

namespace seadsa {
class SeaDsaAAResult;
}

namespace seadsa {

/// Run alias-check validation: scan the module for MAYALIAS/NOALIAS/MUSTALIAS
/// (and PARTIALALIAS) calls, query the built alias model, and print SUCCESS/FAILURE.
/// \return false if any check failed (caller should exit(1)).
bool runValidateAliasTests(llvm::Module &M, SeaDsaAAResult &AA);

} // namespace seadsa

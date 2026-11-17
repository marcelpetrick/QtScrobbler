//
//  Copyright 2019 Mull Project
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>

#include <cassert>

#include "irm/Mutations/VoidCallRemoval.h"

using namespace irm;

VoidCallRemoval::VoidCallRemoval(bool onlyIntrinsics) : onlyIntrinsics(onlyIntrinsics) {}

bool VoidCallRemoval::canMutate(llvm::Instruction* instruction) {
    assert(instruction);
    auto* call = llvm::dyn_cast<llvm::CallBase>(instruction);
    if (!call || (!llvm::isa<llvm::CallInst>(call) && !llvm::isa<llvm::InvokeInst>(call)) ||
        !call->getCalledFunction() || !call->getType()->isVoidTy())
        return false;
    auto* function = call->getCalledFunction();
    // Compiler-generated object construction/destruction is not the source call
    // expression to which its debug location happens to point.
    llvm::ItaniumPartialDemangler name;
    if (!name.partialDemangle(function->getName().str().c_str()) && name.isCtorOrDtor()) return false;
    const bool intrinsic = function->getIntrinsicID() != llvm::Intrinsic::not_intrinsic;
    return onlyIntrinsics ? intrinsic : !intrinsic;
}

void VoidCallRemoval::mutate(llvm::Instruction* instruction) {
    assert(canMutate(instruction));
    if (auto* invoke = llvm::dyn_cast<llvm::InvokeInst>(instruction)) {
        auto* normal = invoke->getNormalDest();
        auto* unwind = invoke->getUnwindDest();
        auto* parent = invoke->getParent();
        // Removing the call removes its exceptional edge as well. Keep the
        // normal continuation and remove incoming PHIs from the unwind block.
        unwind->removePredecessor(parent);
        llvm::BranchInst::Create(normal, llvm::BasicBlock::iterator(invoke));
    }
    instruction->eraseFromParent();
}

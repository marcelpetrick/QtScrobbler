// SPDX-License-Identifier: Apache-2.0
// Native qualification of the LLVM control-flow correction in void_call_fix.cpp.
#include <irm/Mutations/VoidCallRemoval.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <cassert>

int main() {
    llvm::LLVMContext context;
    llvm::SMDiagnostic error;
    auto module = llvm::parseAssemblyString(R"(
        declare i32 @__gxx_personality_v0(...)
        declare void @effect()
        declare void @_ZN6ObjectD2Ev(ptr)
        declare void @_ZN6ObjectC2Ev(ptr)
        declare void @llvm.trap()
        @marker = global i32 0
        define i32 @invoke_paths(i1 %choice) personality ptr @__gxx_personality_v0 {
        entry:
            br i1 %choice, label %left, label %right
        left:
            invoke void @effect() to label %normal unwind label %cleanup
        right:
            invoke void @effect() to label %normal unwind label %cleanup
        normal:
            ret i32 1
        cleanup:
            %from = phi i32 [1, %left], [2, %right]
            %landing = landingpad {ptr, i32} cleanup
            store volatile i32 %from, ptr @marker
            resume {ptr, i32} %landing
        }
        define void @ordinary_calls(ptr %object) {
            call void @effect()
            call void @_ZN6ObjectD2Ev(ptr %object)
            call void @_ZN6ObjectC2Ev(ptr %object)
            call void @llvm.trap()
            unreachable
        }
    )",
                                            error, context);
    if (!module) {
        error.print("invoke-cfg", llvm::errs());
        return 1;
    }
    assert(!llvm::verifyModule(*module, &llvm::errs()));
    irm::RemoveVoidFunctionCall remove;
    irm::RemoveVoidIntrinsicsCall intrinsic;
    auto* function = module->getFunction("invoke_paths");
    llvm::BasicBlock *left = nullptr, *right = nullptr, *cleanup = nullptr;
    for (auto& block : *function) {
        if (block.getName() == "left") left = &block;
        if (block.getName() == "right") right = &block;
        if (block.getName() == "cleanup") cleanup = &block;
    }
    assert(left && right && cleanup);
    assert(remove.canMutate(left->getTerminator()));
    remove.mutate(left->getTerminator());
    assert(llvm::isa<llvm::BranchInst>(left->getTerminator()));
    assert(cleanup->getSinglePredecessor() == right);
    assert(!llvm::verifyModule(*module, &llvm::errs()));
    remove.mutate(right->getTerminator());
    assert(llvm::pred_empty(cleanup));
    assert(!llvm::verifyModule(*module, &llvm::errs()));
    auto& ordinary = module->getFunction("ordinary_calls")->getEntryBlock();
    auto iterator = ordinary.begin();
    assert(remove.canMutate(&*iterator++));
    assert(!remove.canMutate(&*iterator++));
    assert(!remove.canMutate(&*iterator++));
    assert(!remove.canMutate(&*iterator));
    assert(intrinsic.canMutate(&*iterator));
    llvm::outs()
        << "PASS: invoke normal edges, unwind PHIs, ordinary calls, cleanup rejection and intrinsic selection\n";
}

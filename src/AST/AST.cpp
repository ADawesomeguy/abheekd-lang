//
// Created by abheekd on 6/21/2022.
//

#include "AST/AST.hpp"

#include <utility>
#include <llvm/IR/Verifier.h>

using llvm::APFloat;
using llvm::BasicBlock;
using llvm::ConstantFP;
using llvm::StringRef;
using llvm::FunctionType;
using llvm::Value;
using llvm::Function;
using llvm::LLVMContext;
using llvm::IRBuilder;
using llvm::Module;
using llvm::Type;

// CODEGEN BEGIN
static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder;
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;

void InitializeModule() {
    // Open a new context and module.
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("holy jit", *TheContext);

    // Create a new builder for the module.
    Builder = std::make_unique<IRBuilder<>>(*TheContext);
}
// CODEGEN END

ExprAST::~ExprAST() = default;

NumberExprAST::NumberExprAST(double Value) : Value(Value) {}
Value *NumberExprAST::codegen() {
    return ConstantFP::get(*TheContext, APFloat(Value));
}

StringExprAST::StringExprAST(std::string Value) : Value(std::move(Value)) {}

llvm::Value *StringExprAST::codegen() {
    auto temp = StringRef("amogus");
    return Builder->CreateGlobalStringPtr(temp);
}

/*Value *StringExprAST::codegen() {
    return StringLiteral::get(TheContext, APFloat(Value));
}*/

VariableExprAST::VariableExprAST(std::string Name) : Name(std::move(Name)) {}
Value *VariableExprAST::codegen() {
    // Look this variable up in the function.
    Value *V = NamedValues[Name];
    if (!V)
        throw std::runtime_error("codegen error: unknown variable name");
    return V;
}

BinaryExprAST::BinaryExprAST(Token  Op, std::unique_ptr<ExprAST> Left,
                             std::unique_ptr<ExprAST> Right)
                             : Op(std::move(Op)), Left(std::move(Left)),
                             Right(std::move(Right)) {}
Value *BinaryExprAST::codegen() {
    Value *L = Left->codegen();
    Value *R = Right->codegen();
    if (!L || !R)
        return nullptr;

    // todo: string ops
    switch (Op.value.at(0)) {
        case '+':
            return Builder->CreateFAdd(L, R, "add_tmp");
        case '-':
            return Builder->CreateFSub(L, R, "sub_tmp");
        case '*':
            return Builder->CreateFMul(L, R, "mul_tmp");
        case '<':
            L = Builder->CreateFCmpULT(L, R, "cmp_tmp");
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext),
                                        "bool_tmp");
        default:
            throw std::runtime_error("codegen error: unknown operator");
    }
}

CallExprAST::CallExprAST(std::string Callee, std::vector<std::unique_ptr<ExprAST>> Args)
             : Callee(std::move(Callee)), Args(std::move(Args)) {}

Value *CallExprAST::codegen() {
    // Look up the name in the global module table.
    Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF)
        throw std::runtime_error("unknown function: \"" + Callee + "\"");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        throw std::runtime_error("incorrect # arguments passed: expected " + std::to_string(Args.size()) + ", got " + std::to_string(CalleeF->arg_size()));

    std::vector<Value *> ArgsV;
    for (auto & Arg : Args) {
        ArgsV.push_back(Arg->codegen());
        if (!ArgsV.back())
            return nullptr;
    }

    return Builder->CreateCall(CalleeF, ArgsV, "call_tmp");
}

PrototypeAST::PrototypeAST(std::string Name, std::vector<std::string> Args)
                           : Name(std::move(Name)), Args(std::move(Args)) {}

llvm::Function *PrototypeAST::codegen() {
    // Make the function type:  double(double,double) etc.
    std::vector<Type*> Doubles(Args.size(),
                               Type::getDoubleTy(*TheContext));
    FunctionType *FT =
            FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false);
    Function *F =
            Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);

    return F;
}

FunctionAST::FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::unique_ptr<StatementAST> Body)
                         : Proto(std::move(Proto)), Body(std::move(Body)) {}

llvm::Function *FunctionAST::codegen() {
    // First, check for an existing function from a previous 'extern' declaration.
    Function *TheFunction = TheModule->getFunction(Proto->getName());

    if (!TheFunction)
        TheFunction = Proto->codegen();

    if (!TheFunction)
        return nullptr;

    if (!TheFunction->empty())
        throw std::runtime_error("codegen error: cannot redefine function");

    // Create a new basic block to start insertion into.
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args())
        NamedValues[Arg.getName().str()] = &Arg;

    if (Value *RetVal = Body->codegen()) {
        Builder->CreateRet(RetVal);

        // Validate the generated code, checking for consistency.
        llvm::verifyFunction(*TheFunction);

        return TheFunction;
    }
    TheFunction->eraseFromParent();
    return nullptr;
}


StatementAST::~StatementAST() = default;

ExprStatementAST::ExprStatementAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {
    Type = "ExprStatement";
}

llvm::Value *ExprStatementAST::codegen() {
    return this->Expr->codegen();
}

BlockStatementAST::BlockStatementAST(std::vector<std::unique_ptr<StatementAST>> Statements)
                                     : Statements(std::move(Statements)) {
    Type = "BlockStatement";
}

llvm::Value *BlockStatementAST::codegen() {
    for (const auto &S : Statements) {
        if (auto C = S->codegen()) {
            if (S->Type == "ReturnStatement") {
                return C;
            }
        } else {
            return nullptr;
        }
    }
    return nullptr;
}

ReturnStatementAST::ReturnStatementAST(std::unique_ptr<ExprAST> Argument)
                                       : Argument(std::move(Argument)) {
    Type = "ReturnStatement";
}

llvm::Value *ReturnStatementAST::codegen() {
    return this->Argument->codegen();
}

#pragma once

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include <unordered_set>

class RefactorHandler : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    explicit RefactorHandler(clang::Rewriter& rewrite);

    // Метод run вызывается для каждого совпадения с матчем.
    // Мы проверяем тип совпадения по bind-именам и применяем рефакторинг.
    virtual void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

private:
    // 1. Невиртуальные деструкторы
    void handle_nv_dtor(const clang::CXXDestructorDecl* dtor, clang::DiagnosticsEngine& diag, clang::SourceManager& sm);

    // 2. Методы без override
    void
    handle_miss_override(const clang::CXXMethodDecl* method, clang::DiagnosticsEngine& diag, clang::SourceManager& sm);

    // 3. range-for без &
    void handle_crange_for(const clang::VarDecl* loop_var, clang::DiagnosticsEngine& diag, clang::SourceManager& sm);

private:
    clang::Rewriter& rewrite_;
    // Для хранения позиций деструкторов, к которым уже добавлен virtual
    std::unordered_set<unsigned> virtual_dtor_locations_;
};

class ComplexConsumer : public clang::ASTConsumer {
public:
    // Конструктор принимает Rewriter для изменения кода.
    explicit ComplexConsumer(clang::Rewriter& rewrite);
    // Метод HandleTranslationUnit вызывается для каждого файла.
    void HandleTranslationUnit(clang::ASTContext& context) override;

private:
    // Обработчик матчеров.
    RefactorHandler handler_;
    // MatchFinder для поиска узлов AST.
    clang::ast_matchers::MatchFinder finder_;
};

class CodeRefactorAction : public clang::ASTFrontendAction {
public:
    // Returns our ASTConsumer per translation unit.
    virtual std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance& compiler, clang::StringRef file) override;
    virtual bool BeginSourceFileAction(clang::CompilerInstance& compiler) override;
    virtual void EndSourceFileAction() override;

private:
    clang::Rewriter rewriter_;
};

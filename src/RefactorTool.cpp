#include "RefactorTool.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include <cctype>
#include <unordered_set>

RefactorHandler::RefactorHandler(clang::Rewriter& rewrite) : rewrite_(rewrite) {
}

// Метод run вызывается для каждого совпадения с матчем.
// Мы проверяем тип совпадения по bind-именам и применяем рефакторинг.
void RefactorHandler::run(const clang::ast_matchers::MatchFinder::MatchResult& result) {
    auto& diag = result.Context->getDiagnostics();
    auto& sm = *result.SourceManager;  // Получаем SourceManager для проверки isInMainFile

    if (const auto* dtor = result.Nodes.getNodeAs<clang::CXXDestructorDecl>("nonVirtualDtor")) {
        handle_nv_dtor(dtor, diag, sm);
    }
    if (const auto* method = result.Nodes.getNodeAs<clang::CXXMethodDecl>("missingOverride")) {
        handle_miss_override(method, diag, sm);
    }
    if (const auto* loop_var = result.Nodes.getNodeAs<clang::VarDecl>("loopVar")) {
        handle_crange_for(loop_var, diag, sm);
    }
}

void RefactorHandler::handle_nv_dtor(
    const clang::CXXDestructorDecl* dtor, clang::DiagnosticsEngine& diag, clang::SourceManager& sm
) {
    if (dtor == nullptr || dtor->isImplicit() || dtor->isVirtual()) {
        return;
    }
    if (!sm.isWrittenInMainFile(sm.getSpellingLoc(dtor->getLocation()))) {
        return;
    }

    const auto* dtor_parent = dtor->getParent();
    if (dtor_parent == nullptr || !dtor_parent->isThisDeclarationADefinition()) {
        return;
    }

    namespace am = clang::ast_matchers;

    const auto matches = am::match(
        am::translationUnitDecl(
            am::hasDescendant(
                am::cxxRecordDecl(
                    am::unless(am::equalsNode(dtor_parent)), am::isDerivedFrom(am::equalsNode(dtor_parent))
                )
            )
        ),
        dtor->getASTContext()
    );
    if (matches.empty()) {
        return;
    }

    clang::SourceLocation insert_loc = sm.getSpellingLoc(dtor->getLocation());
    if (insert_loc.isInvalid()) {
        return;
    }
    const auto offset = sm.getFileOffset(insert_loc);
    if (!virtual_dtor_locations_.insert(offset).second) {
        return;
    }

    rewrite_.InsertTextBefore(insert_loc, "virtual ");

    const unsigned diag_id =
        diag.getCustomDiagID(clang::DiagnosticsEngine::Remark, "добавлен virtual к деструктору базового класса");
    diag.Report(dtor->getLocation(), diag_id);
}

void RefactorHandler::handle_miss_override(
    const clang::CXXMethodDecl* method, clang::DiagnosticsEngine& diag, clang::SourceManager& sm
) {
    if (method == nullptr || method->isImplicit() || method->size_overridden_methods() == 0 ||
        method->hasAttr<clang::OverrideAttr>()) {
        return;
    }
    if (!sm.isWrittenInMainFile(sm.getSpellingLoc(method->getLocation()))) {
        return;
    }

    const clang::SourceLocation name_end_loc = clang::Lexer::getLocForEndOfToken(
        sm.getSpellingLoc(method->getNameInfo().getEndLoc()), 0, sm, method->getASTContext().getLangOpts()
    );
    if (name_end_loc.isInvalid()) {
        return;
    }

    const auto buffer = clang::Lexer::getSourceText(
        clang::CharSourceRange::getCharRange(name_end_loc, sm.getLocForEndOfFile(sm.getFileID(name_end_loc))),
        sm,
        method->getASTContext().getLangOpts()
    );
    if (buffer.empty()) {
        return;
    }

    // Ищем закрывающую скобку именно списка параметров, чтобы вставить `override`
    // до квалификаторов метода и тела функции.
    size_t right_paren_pos = std::string::npos;
    int depth = 0;
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '(') {
            ++depth;
        } else if (buffer[i] == ')') {
            --depth;
            if (depth == 0) {
                right_paren_pos = i;
                break;
            }
        }
    }
    if (right_paren_pos == std::string::npos) {
        return;
    }

    clang::SourceLocation insert_loc = name_end_loc.getLocWithOffset(static_cast<int>(right_paren_pos + 1));
    rewrite_.InsertTextBefore(insert_loc, " override");

    const unsigned diag_id =
        diag.getCustomDiagID(clang::DiagnosticsEngine::Remark, "добавлен override к переопределяющему методу");
    diag.Report(method->getLocation(), diag_id);
}

void RefactorHandler::handle_crange_for(
    const clang::VarDecl* loop_var, clang::DiagnosticsEngine& diag, clang::SourceManager& sm
) {
    if (loop_var == nullptr || loop_var->isImplicit()) {
        return;
    }
    if (!sm.isWrittenInMainFile(sm.getSpellingLoc(loop_var->getLocation()))) {
        return;
    }

    const clang::QualType loop_type = loop_var->getType();
    if (!loop_type.isConstQualified() || loop_type->isReferenceType() || loop_type->isFundamentalType()) {
        return;
    }

    const clang::TypeSourceInfo* type_info = loop_var->getTypeSourceInfo();
    if (type_info == nullptr) {
        return;
    }
    clang::SourceLocation insert_loc = clang::Lexer::getLocForEndOfToken(
        sm.getSpellingLoc(type_info->getTypeLoc().getEndLoc()), 0, sm, loop_var->getASTContext().getLangOpts()
    );
    if (insert_loc.isInvalid()) {
        return;
    }

    rewrite_.InsertTextBefore(insert_loc, "&");

    const unsigned diag_id =
        diag.getCustomDiagID(clang::DiagnosticsEngine::Remark, "добавлен & к const-переменной range-for");
    diag.Report(loop_var->getLocation(), diag_id);
}

namespace {

auto nv_dtor_matcher() {
    namespace am = clang::ast_matchers;

    return am::cxxDestructorDecl(
               am::unless(am::isImplicit()),
               am::unless(am::isVirtual()),
               am::ofClass(am::cxxRecordDecl(am::isDefinition()))
    )
        .bind("nonVirtualDtor");
}

auto no_override_matcher() {
    namespace am = clang::ast_matchers;

    return am::cxxMethodDecl(
               am::unless(am::isImplicit()),
               am::unless(am::cxxDestructorDecl()),
               am::unless(am::hasAttr(clang::attr::Override))
    )
        .bind("missingOverride");
}

auto no_ref_const_var_in_range_loop_matcher() {
    namespace am = clang::ast_matchers;

    return am::varDecl(
               am::hasAncestor(am::cxxForRangeStmt()),
               am::hasType(
                   am::qualType(
                       am::isConstQualified(),
                       am::unless(am::referenceType()),
                       am::unless(am::hasCanonicalType(am::builtinType()))
                   )
               )
    )
        .bind("loopVar");
}

}  // namespace

// Конструктор принимает Rewriter для изменения кода.
ComplexConsumer::ComplexConsumer(clang::Rewriter& rewrite) : handler_(rewrite) {
    // Создаем MatchFinder и добавляем матчеры.
    finder_.addMatcher(nv_dtor_matcher(), &handler_);
    finder_.addMatcher(no_override_matcher(), &handler_);
    finder_.addMatcher(no_ref_const_var_in_range_loop_matcher(), &handler_);
}

// Метод HandleTranslationUnit вызывается для каждого файла.
void ComplexConsumer::HandleTranslationUnit(clang::ASTContext& context) {
    finder_.matchAST(context);
}

std::unique_ptr<clang::ASTConsumer>
CodeRefactorAction::CreateASTConsumer(clang::CompilerInstance& compiler, clang::StringRef file) {
    rewriter_.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
    return std::make_unique<ComplexConsumer>(rewriter_);
}

bool CodeRefactorAction::BeginSourceFileAction(clang::CompilerInstance& compiler) {
    // Инициализируем Rewriter для рефакторинга.
    rewriter_.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
    return true;  // Возвращаем true, чтобы продолжить обработку файла.
}

void CodeRefactorAction::EndSourceFileAction() {
    // Применяем изменения в файле.
    if (rewriter_.overwriteChangedFiles()) {
        llvm::errs() << "Error applying changes to files.\n";
    }
}

namespace {

llvm::cl::OptionCategory tool_category("refactor-tool options");

int run_refactor_tool(int argc, const char** argv) {
    // Парсер опций: Обрабатывает флаги командной строки, компиляционные базы данных.
    auto expected_parser = clang::tooling::CommonOptionsParser::create(argc, argv, tool_category);
    if (!expected_parser) {
        llvm::errs() << expected_parser.takeError();
        return 1;
    }

    clang::tooling::CommonOptionsParser& options_parser = expected_parser.get();
    // Создаем ClangTool
    clang::tooling::ClangTool tool(options_parser.getCompilations(), options_parser.getSourcePathList());

    // Запускаем RefactorAction.
    return tool.run(clang::tooling::newFrontendActionFactory<CodeRefactorAction>().get());
}

}  // namespace

#ifndef REFACTOR_TOOL_DISABLE_MAIN

int main(int argc, const char** argv) {
    return run_refactor_tool(argc, argv);
}

#endif

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

#include <memory>

using namespace clang;

namespace {

class FindLambdaVisitor: public RecursiveASTVisitor<FindLambdaVisitor> {
    DiagnosticsEngine &m_diag_engine;
    uint32_t m_diag_id_this, m_diag_id_ref, m_diag_id_ptr;

    public:
        explicit FindLambdaVisitor(ASTContext *context):
            m_diag_engine{context->getDiagnostics()},
            m_diag_id_this{m_diag_engine.getCustomDiagID(
                    DiagnosticsEngine::Warning, "Lambda captures this")},
            m_diag_id_ref{m_diag_engine.getCustomDiagID(
                    DiagnosticsEngine::Warning,
                    "Lambda captures var by ref: %0")},
            m_diag_id_ptr{m_diag_engine.getCustomDiagID(
                    DiagnosticsEngine::Warning,
                    "Lambda captures pointer: %0 (type %1)")}
        {}

        bool VisitLambdaExpr(LambdaExpr *expr) {
            auto location = expr->getLocStart();
            for (auto &&capture: expr->captures()) {
                auto kind = capture.getCaptureKind();
                if (kind != LCK_ByCopy) {
                    if (kind == LCK_This) {
                        m_diag_engine.Report(location, m_diag_id_this);
                        continue;
                    }
                    auto &&bld = m_diag_engine.Report(location, m_diag_id_ref);
                    if (auto var = capture.getCapturedVar()) {
                         bld << var->getNameAsString();
                    } else {
                        bld << "unknown";
                    }
                    continue;
                }

                // check if var is ptr
                auto var = capture.getCapturedVar();
                if (var->getType()->isPointerType()) {
                    m_diag_engine.Report(location, m_diag_id_ptr) <<
                        var->getNameAsString() << var->getType().getAsString();
                }
            }
            return true;
        }
};

class FindLambdaConsumer: public clang::ASTConsumer {
    public:
        explicit FindLambdaConsumer(ASTContext *context): m_visitor(context) {}

        void HandleTranslationUnit(clang::ASTContext &context) override {
            m_visitor.TraverseDecl(context.getTranslationUnitDecl());
        }
    private:
        FindLambdaVisitor m_visitor;
};

class FindLambdaAction final: public clang::PluginASTAction {
    bool ParseArgs(const CompilerInstance &,
            const std::vector<std::string>&) override {
        return true;
    }

    public:
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
                clang::CompilerInstance &compiler,
                llvm::StringRef in_file) override {
            return std::make_unique<FindLambdaConsumer>(
                    &compiler.getASTContext());
        }
};

static FrontendPluginRegistry::Add<FindLambdaAction>
_register("chkref", "check reference/pointer captures in lambda");

} // anonymous namespace

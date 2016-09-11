#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/Type.h"

#include <memory>
#include <unordered_set>
#include <deque>

#define force_assert(_expr) do { \
    if (!(_expr)) { \
        fprintf(stderr, "assertion %s failed\n", #_expr); \
        __builtin_trap(); \
    } \
} while(0)

using namespace clang;

namespace {

class FindLambdaVisitor: public RecursiveASTVisitor<FindLambdaVisitor> {
    ASTContext *m_context;
    DiagnosticsEngine &m_diag_engine;
    uint32_t m_diag_id_this, m_diag_id_ref, m_diag_id_ptr;

    void handle_capture_this(LambdaExpr *expr) {
        std::string msg;
        auto cls = expr->getLambdaClass();
        force_assert(cls->isLambda());
        llvm::DenseMap< const VarDecl *, FieldDecl * > cap_fields;
        FieldDecl *cap_this = nullptr;
        cls->getCaptureFields(cap_fields, cap_this);
        force_assert(cap_this);
        auto par = cap_this->getType()->getPointeeCXXRecordDecl();
        force_assert(par);
        size_t nr_f = 0;
        msg.append("class `").
            append(par->getNameAsString()).
            append("' with fields:");

        auto on_field = [&](FieldDecl *f, bool require_non_private) {
            if (require_non_private && f->getAccess() == AS_private) {
                return true;
            }
            if (nr_f >= 3) {
                if (nr_f == 3)
                    msg.append(" ...");
                ++ nr_f;
                return false;
            }
            msg.append(" ");
            msg.append(f->getNameAsString());
            ++ nr_f;
            return true;
        };

        std::deque<const CXXRecordDecl*> bases{par};
        while (!bases.empty()) {
            auto cur = bases.front();
            bases.pop_front();
            for (auto &&i: cur->bases()) {
                auto t = i.getType()->getAsCXXRecordDecl();
                force_assert(t);
                bases.push_back(t);
            }
            bool require_non_private = cur != par;
            for (auto f: cur->fields()) {
                if (!on_field(f, require_non_private)) {
                    bases.clear();
                    break;
                }
            }
        }
        m_diag_engine.Report(expr->getLocStart(), m_diag_id_this) << msg;
    }

    public:
        explicit FindLambdaVisitor(ASTContext *context):
            m_context{context},
            m_diag_engine{context->getDiagnostics()},
            m_diag_id_this{m_diag_engine.getCustomDiagID(
                    DiagnosticsEngine::Warning, "Lambda captures this: %0")},
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
                        handle_capture_this(expr);
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

        bool HandleTopLevelDecl(DeclGroupRef decl) override {
            for (auto i: decl) {
                m_visitor.TraverseDecl(i);
            }
            return true;
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

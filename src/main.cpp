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

class FindLambdaVisitor;
class DeclRefExprChecker: public RecursiveASTVisitor<DeclRefExprChecker> {
    bool m_visit_expr = false;
    FindLambdaVisitor * const m_lambda_finder;
    std::unordered_set<Expr*> m_sub_exprs;

    public:
        DeclRefExprChecker(FindLambdaVisitor *v):
            m_lambda_finder{v}
        {}

        bool VisitExpr(Expr *expr) {
            if (m_visit_expr) {
                m_sub_exprs.insert(expr);
            }
            return true;
        }

        bool VisitDeclRefExpr(DeclRefExpr *expr);

        void WorkLambdaBody(LambdaExpr *expr) {
            auto body = expr->getBody();
            m_sub_exprs.clear();
            m_visit_expr = true;
            this->TraverseCompoundStmt(body);
            m_visit_expr = false;
            this->TraverseCompoundStmt(body);
        }
};

class FindLambdaVisitor: public RecursiveASTVisitor<FindLambdaVisitor> {
    ASTContext *m_context;
    DiagnosticsEngine &m_diag_engine;
    uint32_t m_diag_id_this, m_diag_id_ref, m_diag_id_ptr;
    DeclRefExprChecker m_decl_ref_checker{this};
    std::unordered_set<std::string> m_cur_warned_ptr;

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
            m_cur_warned_ptr.clear();
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
                    WarnPtr(location, var->getNameAsString(),
                            var->getType().getAsString());
                }
            }
            // XXX: only for dependent name lookup, and it should be handled
            // aftre Sema; but I do not know how to do this ...
            m_decl_ref_checker.WorkLambdaBody(expr);
            return true;
        }

        void WarnPtr(const SourceLocation &loc, const std::string &name,
                const std::string &type) {
            if (m_cur_warned_ptr.insert(name).second) {
                m_diag_engine.Report(loc, m_diag_id_ptr) << name << type;
            }
        }
};

bool DeclRefExprChecker::VisitDeclRefExpr(DeclRefExpr *expr) {
    if (m_visit_expr)
        return true;

    if (auto f = expr->getFoundDecl()) {
        auto vard = dyn_cast<VarDecl>(f);
        if (vard && vard->hasInit()) {
            auto init = vard->getInit();
            if (init->getType()->isPointerType() && !m_sub_exprs.count(init)) {
                m_lambda_finder->WarnPtr(
                        expr->getLocation(),
                        vard->getNameAsString(),
                        init->getType().getAsString());
            }
        }
    }
    return true;
}

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

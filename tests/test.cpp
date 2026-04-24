#include "RefactorTool.h"
#include "clang/Tooling/Tooling.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>

namespace {

class TestCodeRefactorAction : public clang::ASTFrontendAction {
public:
    TestCodeRefactorAction(std::string_view source_code, std::string& output)
        : source_code_(source_code), output_(output) {
    }

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance& compiler, clang::StringRef file) override {
        rewriter_.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
        return std::make_unique<ComplexConsumer>(rewriter_);
    }

    bool BeginSourceFileAction(clang::CompilerInstance& compiler) override {
        rewriter_.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
        return true;
    }

    void EndSourceFileAction() override {
        const clang::SourceManager& sm = rewriter_.getSourceMgr();
        const clang::FileID main_file_id = sm.getMainFileID();
        const llvm::RewriteBuffer* rewrite_buffer = rewriter_.getRewriteBufferFor(main_file_id);
        bool changed = rewrite_buffer != nullptr;
        if (!changed) {
            output_ = source_code_;
            return;
        }
        output_.assign(rewrite_buffer->begin(), rewrite_buffer->end());
    }

private:
    std::string_view source_code_;
    std::string& output_;
    clang::Rewriter rewriter_;
};

std::string run_refactor(std::string_view source_code) {
    std::string output;
    auto action = std::make_unique<TestCodeRefactorAction>(source_code, output);

    const bool ok =
        clang::tooling::runToolOnCodeWithArgs(std::move(action), std::string(source_code), {"-std=c++17"}, "input.cc");
    EXPECT_TRUE(ok);
    if (!ok) {
        return {};
    }

    return output;
}

TEST(NonVirtualDestructorRefactor, AddsVirtualToBaseDestructorWithDerivedClass) {
    constexpr std::string_view source = R"(class Base {
public:
    ~Base() = default;
};

class Derived : public Base {};
)";
    constexpr std::string_view expected = R"(class Base {
public:
    virtual ~Base() = default;
};

class Derived : public Base {};
)";

    EXPECT_EQ(run_refactor(source), expected);
}

TEST(NonVirtualDestructorRefactor, DoesNotTouchClassWithoutDerivedClasses) {
    constexpr std::string_view source = R"(class Standalone {
public:
    ~Standalone() = default;
};
)";
    constexpr std::string_view expected = R"(class Standalone {
public:
    ~Standalone() = default;
};
)";

    EXPECT_EQ(run_refactor(source), expected);
}

TEST(OverrideRefactor, AddsOverrideToOverridingMethod) {
    constexpr std::string_view source = R"(class Base {
public:
    virtual void foo() {}
};

class Derived : public Base {
public:
    void foo() {}
};
)";
    constexpr std::string_view expected = R"(class Base {
public:
    virtual void foo() {}
};

class Derived : public Base {
public:
    void foo() override {}
};
)";

    EXPECT_EQ(run_refactor(source), expected);
}

TEST(OverrideRefactor, DoesNotTouchMethodThatAlreadyHasOverride) {
    constexpr std::string_view source = R"(class Base {
public:
    virtual void foo() {}
};

class Derived : public Base {
public:
    void foo() override {}
};
)";
    constexpr std::string_view expected = R"(class Base {
public:
    virtual void foo() {}
};

class Derived : public Base {
public:
    void foo() override {}
};
)";

    EXPECT_EQ(run_refactor(source), expected);
}

TEST(RangeForRefactor, AddsReferenceToConstRangeLoopVariable) {
    constexpr std::string_view source = R"(struct Item {
    int value;
};

struct ItemRange {
    Item* begin_ptr;
    Item* end_ptr;
};

Item* begin(ItemRange range) {
    return range.begin_ptr;
}

Item* end(ItemRange range) {
    return range.end_ptr;
}

void test() {
    Item items[2] = {{1}, {2}};
    ItemRange range{items, items + 2};
    for (const Item item : range) {
    }
}
)";
    constexpr std::string_view expected = R"(struct Item {
    int value;
};

struct ItemRange {
    Item* begin_ptr;
    Item* end_ptr;
};

Item* begin(ItemRange range) {
    return range.begin_ptr;
}

Item* end(ItemRange range) {
    return range.end_ptr;
}

void test() {
    Item items[2] = {{1}, {2}};
    ItemRange range{items, items + 2};
    for (const Item& item : range) {
    }
}
)";

    EXPECT_EQ(run_refactor(source), expected);
}

TEST(RangeForRefactor, DoesNotTouchFundamentalTypeInRangeLoop) {
    constexpr std::string_view source = R"(struct IntRange {
    int* begin_ptr;
    int* end_ptr;
};

int* begin(IntRange range) {
    return range.begin_ptr;
}

int* end(IntRange range) {
    return range.end_ptr;
}

void test() {
    int items[2] = {1, 2};
    IntRange range{items, items + 2};
    for (const int item : range) {
    }
}
)";
    constexpr std::string_view expected = R"(struct IntRange {
    int* begin_ptr;
    int* end_ptr;
};

int* begin(IntRange range) {
    return range.begin_ptr;
}

int* end(IntRange range) {
    return range.end_ptr;
}

void test() {
    int items[2] = {1, 2};
    IntRange range{items, items + 2};
    for (const int item : range) {
    }
}
)";

    EXPECT_EQ(run_refactor(source), expected);
}

}  // namespace

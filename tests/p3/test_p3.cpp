#include "../mini_test.h"
#include "../test_helpers.h"

#include "tools/calculator.h"
#include "tools/file_tools.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;
};

class CountingTool : public NamedTool {
public:
    explicit CountingTool(std::string n) : NamedTool(std::move(n)) { ++live; }
    ~CountingTool() override { --live; }
    static int live;
};
int CountingTool::live = 0;

bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

std::vector<std::string> scale_names() {
    std::vector<std::string> names;
    for (int i = 0; i < 10000; ++i) names.push_back("tool_" + std::to_string(i));
    return names;
}

std::size_t weak_hash(const std::string& s) {
    return static_cast<unsigned char>(s[0]) + s.size();
}

std::size_t djb2(const std::string& s) {
    std::size_t h = 5381;
    for (unsigned char c : s) h = h * 33 + c;
    return h;
}

template <typename H>
std::size_t longest_chain(const std::vector<std::string>& names, std::size_t buckets, H hasher) {
    std::vector<int> counts(buckets, 0);
    for (const std::string& n : names) ++counts[hasher(n) % buckets];
    return static_cast<std::size_t>(*std::max_element(counts.begin(), counts.end()));
}

CalcToken tok(CalcToken::Kind k, double v = 0.0) {
    CalcToken t;
    t.kind = k;
    t.value = v;
    return t;
}

struct WsFixture {
    TempDir dir;
    Workspace ws;
    fs::path out;
    std::string why;
    WsFixture() : ws(dir.path()) {}
    bool resolve(const std::string& p) {
        why.clear();
        return ws.resolve(p, &out, &why);
    }
};

}

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(reg_empty_table) {
    ToolRegistry r;
    CHECK_EQ(r.size(), std::size_t{0});
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK_EQ(r.max_probe_length(), std::size_t{0});
    CHECK(r.find("anything") == nullptr);
    CHECK(r.find("") == nullptr);
    int visited = 0;
    r.for_each([&](const Tool&) { ++visited; });
    CHECK_EQ(visited, 0);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK_EQ(r.size(), std::size_t{0});
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(!r.add(std::unique_ptr<Tool>()));
    CHECK_EQ(r.size(), std::size_t{1});
    CHECK(r.find("alpha") != nullptr);
}

TEST(reg_find_absent_name) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("alphabet")));
    CHECK(r.find("alph") == nullptr);
    CHECK(r.find("alphaa") == nullptr);
    CHECK(r.find("Alpha") == nullptr);
    CHECK(r.find("") == nullptr);
    CHECK(r.find("alpha") != nullptr);
    CHECK(r.find("alphabet") != nullptr);
    CHECK(r.find("alpha") != r.find("alphabet"));
}

TEST(reg_size_tracks_adds) {
    ToolRegistry r;
    CHECK_EQ(r.size(), std::size_t{0});
    CHECK(r.add(std::make_unique<NamedTool>("a")));
    CHECK_EQ(r.size(), std::size_t{1});
    CHECK(!r.add(std::make_unique<NamedTool>("a")));
    CHECK_EQ(r.size(), std::size_t{1});
    CHECK(!r.add(nullptr));
    CHECK_EQ(r.size(), std::size_t{1});
    CHECK(r.add(std::make_unique<NamedTool>("b")));
    CHECK_EQ(r.size(), std::size_t{2});
    CHECK(r.add(std::make_unique<NamedTool>("c")));
    CHECK_EQ(r.size(), std::size_t{3});
    CHECK(!r.add(std::make_unique<NamedTool>("b")));
    CHECK_EQ(r.size(), std::size_t{3});
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    std::vector<std::string> names = {"read_file", "write_file", "list_dir", "calculator", "echo"};
    for (const std::string& n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));

    std::vector<std::string> seen;
    r.for_each([&](const Tool& t) { seen.push_back(t.name()); });

    CHECK_EQ(seen.size(), names.size());
    std::sort(seen.begin(), seen.end());
    std::sort(names.begin(), names.end());
    CHECK(seen == names);
}

TEST(reg_for_each_gives_real_objects) {
    ToolRegistry r;
    auto a = std::make_unique<NamedTool>("a");
    auto b = std::make_unique<NamedTool>("b");
    const Tool* pa = a.get();
    const Tool* pb = b.get();
    REQUIRE(r.add(std::move(a)));
    REQUIRE(r.add(std::move(b)));
    int matched = 0;
    r.for_each([&](const Tool& t) {
        if (&t == pa || &t == pb) ++matched;
    });
    CHECK_EQ(matched, 2);
}

TEST(reg_destroys_tools_once) {
    CountingTool::live = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountingTool>("a")));
        REQUIRE(r.add(std::make_unique<CountingTool>("b")));
        CHECK_EQ(CountingTool::live, 2);

        CHECK(!r.add(std::make_unique<CountingTool>("a")));
        CHECK_EQ(CountingTool::live, 2);
        CHECK_EQ(r.size(), std::size_t{2});

        for (int i = 0; i < 200; ++i)
            REQUIRE(r.add(std::make_unique<CountingTool>("t" + std::to_string(i))));
        CHECK_EQ(CountingTool::live, 202);
        CHECK_EQ(r.size(), std::size_t{202});
    }
    CHECK_EQ(CountingTool::live, 0);
}

TEST(reg_hash_order_sensitive) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1235"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("uool_1234"));
    CHECK(ToolRegistry::hash("a") != ToolRegistry::hash("aa"));
    CHECK(ToolRegistry::hash("") != ToolRegistry::hash("a"));
    CHECK(ToolRegistry::hash("calculator") == ToolRegistry::hash("calculator"));
}

TEST(reg_hash_distinct_values) {
    std::vector<std::size_t> hashes;
    for (const std::string& n : scale_names()) hashes.push_back(ToolRegistry::hash(n));
    std::sort(hashes.begin(), hashes.end());
    std::size_t distinct =
        static_cast<std::size_t>(std::unique(hashes.begin(), hashes.end()) - hashes.begin());
    std::printf("    distinct hash values for tool_0..tool_9999: %zu\n", distinct);
    CHECK(distinct >= 9990);
}

TEST(reg_resize_keeps_pointers) {
    ToolRegistry r;
    std::size_t initial = r.bucket_count();
    std::vector<Tool*> ptrs;
    std::size_t last = initial;
    int resizes = 0;

    for (int i = 0; i < 300; ++i) {
        auto t = std::make_unique<NamedTool>("t" + std::to_string(i));
        ptrs.push_back(t.get());
        REQUIRE(r.add(std::move(t)));
        if (r.bucket_count() != last) {
            CHECK_EQ(r.bucket_count(), last * 2);
            last = r.bucket_count();
            ++resizes;
        }
        CHECK(r.load_factor() <= 1.0);
    }
    CHECK(resizes >= 2);
    CHECK(r.bucket_count() > initial);
    CHECK((r.bucket_count() & (r.bucket_count() - 1)) == 0);

    for (int i = 0; i < 300; ++i)
        CHECK(r.find("t" + std::to_string(i)) == ptrs[static_cast<std::size_t>(i)]);
    CHECK_EQ(r.size(), std::size_t{300});
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    std::vector<std::string> names = scale_names();

    auto first = std::make_unique<NamedTool>(names[0]);
    Tool* first_ptr = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(r.find(names[0]) == first_ptr);

    double worst_load = r.load_factor();
    for (std::size_t i = 1; i < names.size(); ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>(names[i])));
        double lf = r.load_factor();
        if (lf > worst_load) worst_load = lf;
        if (lf > 1.0) {
            CHECK(lf <= 1.0);
            break;
        }
    }
    CHECK(worst_load <= 1.0);
    CHECK_EQ(r.size(), std::size_t{10000});
    CHECK(r.find(names[0]) == first_ptr);

    std::size_t found = 0;
    for (const std::string& n : names)
        if (r.find(n) != nullptr) ++found;
    CHECK_EQ(found, std::size_t{10000});
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.find("tool_") == nullptr);

    std::size_t visited = 0;
    r.for_each([&](const Tool&) { ++visited; });
    CHECK_EQ(visited, std::size_t{10000});

    CHECK(r.max_probe_length() <= 64);
    std::printf("    10,000 tools: bucket_count=%zu load_factor=%.4f max_probe_length=%zu "
                "(worst load factor during adds: %.4f)\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), worst_load);
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    std::vector<std::string> names = scale_names();
    for (const std::string& n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));
    std::size_t buckets = r.bucket_count();

    std::size_t real = r.max_probe_length();
    std::size_t fnv = longest_chain(names, buckets, ToolRegistry::hash);
    std::size_t weak = longest_chain(names, buckets, weak_hash);
    std::size_t dj = longest_chain(names, buckets, djb2);

    std::printf("    longest chain at %zu buckets: FNV-1a (registry)=%zu  FNV-1a (recomputed)=%zu"
                "  s[0]+size=%zu  djb2=%zu\n", buckets, real, fnv, weak, dj);

    CHECK_EQ(fnv, real);
    CHECK(weak > 1000);
    CHECK(weak > 10 * real);

    std::vector<std::size_t> weak_values;
    for (const std::string& n : names) weak_values.push_back(weak_hash(n));
    std::sort(weak_values.begin(), weak_values.end());
    CHECK(std::unique(weak_values.begin(), weak_values.end()) - weak_values.begin() == 4);
}

TEST(ws_simple_path) {
    WsFixture f;
    REQUIRE(f.resolve("notes/todo.txt"));
    CHECK(f.out == f.ws.root() / "notes" / "todo.txt");
}

TEST(ws_dot_and_double_slash) {
    WsFixture f;
    REQUIRE(f.resolve("./notes//todo.txt"));
    CHECK(f.out == f.ws.root() / "notes" / "todo.txt");
}

TEST(ws_dot_is_root) {
    WsFixture f;
    REQUIRE(f.resolve("."));
    CHECK(f.out == f.ws.root());
}

TEST(ws_dotdot_inside) {
    WsFixture f;
    REQUIRE(f.resolve("a/../b"));
    CHECK(f.out == f.ws.root() / "b");
}

TEST(ws_dotdot_to_root) {
    WsFixture f;
    REQUIRE(f.resolve("a/.."));
    CHECK(f.out == f.ws.root());
}

TEST(ws_nonexistent_file) {
    WsFixture f;
    REQUIRE(f.resolve("new/dir/file.txt"));
    CHECK(f.out == f.ws.root() / "new" / "dir" / "file.txt");
    CHECK(!fs::exists(f.ws.root() / "new"));
}

TEST(ws_dotdot_in_filename) {
    WsFixture f;
    REQUIRE(f.resolve("my..notes.txt"));
    CHECK(f.out == f.ws.root() / "my..notes.txt");
    REQUIRE(f.resolve("notes/..hidden/x"));
    CHECK(f.out == f.ws.root() / "notes" / "..hidden" / "x");
}

TEST(ws_rejects_empty) {
    WsFixture f;
    CHECK(!f.resolve(""));
    CHECK(!f.why.empty());
}

TEST(ws_rejects_nul) {
    WsFixture f;
    std::string with_nul = std::string("notes") + '\0' + "/x";
    REQUIRE(with_nul.size() == 8);
    CHECK(!f.resolve(with_nul));
    CHECK(!f.why.empty());
}

TEST(ws_rejects_absolute) {
    WsFixture f;
    CHECK(!f.resolve("/etc/passwd"));
    CHECK(!f.why.empty());
    CHECK(!f.resolve((f.ws.root() / "notes" / "todo.txt").string()));
    CHECK(!f.why.empty());
}

TEST(ws_rejects_bare_dotdot) {
    WsFixture f;
    CHECK(!f.resolve(".."));
    CHECK(!f.why.empty());
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_climb_out) {
    WsFixture f;
    CHECK(!f.resolve("a/../../b"));
    CHECK(!f.why.empty());
}

TEST(ws_rejects_out_and_back) {
    TempDir d;
    fs::path root = d.path() / "ws";
    fs::create_directories(root);
    Workspace ws(root);
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("notes/../../ws/notes/todo.txt", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_no_filesystem_access) {
    TempDir d;
    fs::path ghost = d.path() / "does_not_exist";
    Workspace ws(ghost);
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("a/b.txt", &out, &reason));
    CHECK(out == ws.root() / "a" / "b.txt");
    CHECK(!fs::exists(ghost));
}

TEST(ws_through_file_tools) {
    TempDir d;
    write_text(d.path() / "notes" / "todo.txt", "- buy milk\n");
    write_text(d.path() / "secret.txt", "outside\n");
    ToolContext ctx{Workspace(d.path() / "notes")};
    ReadFileTool reader;

    ToolResult ok = reader.invoke(args1("path", JsonValue::string("todo.txt")), ctx);
    CHECK(ok.ok);
    CHECK_EQ(ok.content, std::string("- buy milk\n"));

    ToolResult bad = reader.invoke(args1("path", JsonValue::string("../secret.txt")), ctx);
    CHECK(!bad.ok);
    CHECK(!bad.error.empty());

    ToolResult abs = reader.invoke(
        args1("path", JsonValue::string((d.path() / "secret.txt").string())), ctx);
    CHECK(!abs.ok);

    WriteFileTool writer;
    JsonValue a = args1("path", JsonValue::string("../escaped.txt"));
    a.set("content", JsonValue::string("x"));
    CHECK(!writer.invoke(a, ctx).ok);
    CHECK(!fs::exists(d.path() / "escaped.txt"));
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_left_to_right_subtraction) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
}

TEST(calc_left_to_right_division) {
    double v = 0;
    std::string err;
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}

TEST(calc_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
}

TEST(calc_mixed) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
}

TEST(calc_negative_numbers) {
    double v = 0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("2*-3", &v, &err));
    CHECK(v == -6);
    REQUIRE(eval("(-1) + 1", &v, &err));
    CHECK(v == 0);
}

TEST(calc_decimal_result) {
    double v = 0;
    std::string err;
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
    err.clear();
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_single_number) {
    double v = 0;
    std::string err;
    REQUIRE(eval("42", &v, &err));
    CHECK(v == 42);
    REQUIRE(eval("-2.5", &v, &err));
    CHECK(v == -2.5);
    REQUIRE(eval("((7))", &v, &err));
    CHECK(v == 7);
}

TEST(calc_nested_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("((1 + 2) * (3 + 4)) / 7", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("2 * (3 + (4 - (5 - 6)))", &v, &err));
    CHECK(v == 16);
}

TEST(calc_decimals) {
    double v = 0;
    std::string err;
    REQUIRE(eval("0.5 * 4 + 1.25", &v, &err));
    CHECK(v == 3.25);
    REQUIRE(eval("1 + 2 * 3 - 4 / 2", &v, &err));
    CHECK(v == 5);
    REQUIRE(eval("(12 + 30) * 2 / 7", &v, &err));
    CHECK(v == 12);
}

TEST(calc_long_expression) {
    std::string expr = "1";
    for (int i = 0; i < 19999; ++i) expr += "+1";
    double v = 0;
    std::string err;
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 20000);

    expr = "100000";
    for (int i = 0; i < 10000; ++i) expr += "-1";
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 90000);
}

TEST(calc_deep_nesting) {
    int depth = 5000;
    std::string expr(static_cast<std::size_t>(depth), '(');
    expr += "1";
    expr += std::string(static_cast<std::size_t>(depth), ')');
    double v = 0;
    std::string err;
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 1);
}

TEST(calc_bad_tokens) {
    using K = CalcToken::Kind;
    double v = 0;
    std::string err;

    CHECK(!calc_evaluate({}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({tok(K::Plus)}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({tok(K::Number, 1), tok(K::Plus)}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({tok(K::RParen)}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({tok(K::LParen), tok(K::Number, 1)}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    CHECK(!calc_evaluate({tok(K::Number, 1), tok(K::Number, 2)}, &v, &err));
    CHECK(!err.empty());

    err.clear();
    REQUIRE(calc_evaluate({tok(K::Number, 6), tok(K::Divide), tok(K::Number, 3)}, &v, &err));
    CHECK(v == 2);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }

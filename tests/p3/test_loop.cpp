// tests/p3/test_loop.cpp — PROVIDED.
//
// End-to-end tests of the tool loop: model -> tool -> model with no user turn
// in between. These use the provided Harness and dispatcher, but they only
// pass once YOUR ToolRegistry, Workspace::resolve and calc_evaluate work.
// Read them to see how your classes are used. Do not modify this file.
//
//   ./build/test_loop             run all
//   ./build/test_loop loop_chain  run tests whose names start with "loop_chain"

#include "../mini_test.h"
#include "../test_helpers.h"

#include "harness/harness.h"
#include "harness/tool_dispatcher.h"
#include "harness/transcript.h"
#include "json/json_value.h"
#include "model/replay_client.h"
#include "tools/builtin_tools.h"
#include "tools/calculator.h"
#include "tools/calculator_tool.h"
#include "tools/file_tools.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
struct LoopFixture {
    TempDir      dir;
    ToolRegistry registry;
    ToolContext  ctx;
    LoopFixture() : ctx(Workspace(dir.path())) {
        register_builtin_tools(registry);
        write_text(dir.path() / "todo.txt", "- buy milk\n");
    }
};
}  // namespace

TEST(loop_model_tool_model_without_user_turn) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    int model_calls = 0;
    std::vector<Role> seen;
    auto model = std::make_unique<ListModel>(
        std::vector<std::string>{
            "Checking." + call("c1", "read_file", R"({"path":"todo.txt"})"),
            "You need to buy milk.",
        },
        &model_calls, &seen);
    LinesInput in({"read my todo"});
    StringOutput out;
    Harness h(std::move(model), HarnessConfig{}, &f.registry, &f.ctx);
    StopReason why = h.run(in, out);
    CHECK(why.kind == StopReason::Kind::UserExit);
    CHECK(model_calls == 2);
    CHECK(in.reads == 2);                       // one real turn, then EOF
    REQUIRE(seen.size() == 2);
    CHECK(seen[0] == Role::User);
    CHECK(seen[1] == Role::Tool);               // second call saw the tool result
    const Conversation& c = h.conversation();
    REQUIRE(c.size() == 4);
    CHECK(c.at(0).role() == Role::User);
    CHECK(c.at(1).role() == Role::Assistant);
    CHECK(c.at(2).role() == Role::Tool);
    CHECK(c.at(3).role() == Role::Assistant);
    CHECK(field(result_json(c.at(2)), "content").as_string() == "- buy milk\n");
    CHECK(out.data.find("[tool read_file c1 -> ok]") != std::string::npos);
}

TEST(loop_escape_attempt_is_an_error_not_a_crash) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    int model_calls = 0;
    auto model = std::make_unique<ListModel>(
        std::vector<std::string>{
            call("e1", "read_file", R"({"path":"../../etc/passwd"})"),
            call("e2", "read_file", R"({"path":"todo.txt"})"),
            "Only the workspace is readable.",
        },
        &model_calls);
    LinesInput in({"read the password file"});
    StringOutput out;
    Harness h(std::move(model), HarnessConfig{}, &f.registry, &f.ctx);
    h.run(in, out);
    CHECK(model_calls == 3);
    const Conversation& c = h.conversation();
    REQUIRE(c.size() == 6);   // user, asst, tool(error), asst, tool(ok), asst
    CHECK(field(result_json(c.at(2)), "ok").as_bool() == false);
    CHECK(field(result_json(c.at(4)), "ok").as_bool() == true);
    CHECK(out.data.find("[tool read_file e1 -> error]") != std::string::npos);
}

TEST(loop_chained_tool_rounds) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    int model_calls = 0;
    auto model = std::make_unique<ListModel>(
        std::vector<std::string>{
            call("a", "write_file", R"({"path":"n.txt","content":"7"})"),
            call("b", "read_file", R"({"path":"n.txt"})"),
            call("c", "calculator", R"({"expression":"7*6"})"),
            "The answer is 42.<|end_conversation|>",
        },
        &model_calls);
    LinesInput in({"go"});
    StringOutput out;
    Harness h(std::move(model), HarnessConfig{}, &f.registry, &f.ctx);
    CHECK(h.run(in, out).kind == StopReason::Kind::Sentinel);
    CHECK(model_calls == 4);
    CHECK(in.reads == 1);
    REQUIRE(h.conversation().size() == 8);   // user + 4 assistant + 3 tool
    CHECK(field(result_json(h.conversation().at(2)), "ok").as_bool());
    CHECK(field(result_json(h.conversation().at(4)), "content").as_string() == "7");
    CHECK(field(result_json(h.conversation().at(6)), "content").as_string() == "42");
}

TEST(loop_tool_step_limit) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    std::vector<std::string> replies;
    for (int i = 0; i < 10; ++i) replies.push_back(call("r", "calculator", R"({"expression":"1"})"));
    replies.push_back("done<|end_conversation|>");
    int model_calls = 0;
    HarnessConfig cfg;
    cfg.max_tool_steps = 3;
    LinesInput in({"loop forever", "stop"});
    StringOutput out;
    Harness h(std::make_unique<ListModel>(replies, &model_calls), cfg, &f.registry, &f.ctx);
    h.run(in, out);
    CHECK(out.data.find("tool step limit") != std::string::npos);
    CHECK(model_calls >= 4);
    CHECK(in.reads >= 2);   // control went back to the user
}

TEST(loop_without_registry_is_p2_behavior) {
    int model_calls = 0;
    auto model = std::make_unique<ListModel>(
        std::vector<std::string>{call("c1", "calculator", R"({"expression":"1+1"})"), "bye"},
        &model_calls);
    LinesInput in({"hi"});
    StringOutput out;
    Harness h(std::move(model), HarnessConfig{});
    h.run(in, out);
    CHECK(model_calls == 1);
    CHECK(h.conversation().size() == 2);
}

TEST(loop_sentinel_wins_over_tool_call) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    int model_calls = 0;
    auto model = std::make_unique<ListModel>(
        std::vector<std::string>{
            call("w", "write_file", R"({"path":"late.txt","content":"x"})") + "<|end_conversation|>"},
        &model_calls);
    LinesInput in({"hi"});
    StringOutput out;
    Harness h(std::move(model), HarnessConfig{}, &f.registry, &f.ctx);
    CHECK(h.run(in, out).kind == StopReason::Kind::Sentinel);
    CHECK(!fs::exists(f.dir.path() / "late.txt"));
    CHECK(model_calls == 1);
}

TEST(loop_client_error) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    LinesInput in({"hi", "again"});
    StringOutput out;
    Harness h(std::make_unique<ListModel>(std::vector<std::string>{"only one"}), HarnessConfig{},
              &f.registry, &f.ctx);
    StopReason why = h.run(in, out);
    CHECK(why.kind == StopReason::Kind::ClientError);
    CHECK(why.detail.find("exhausted") != std::string::npos);
}

TEST(loop_system_message_lists_tools) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    HarnessConfig cfg;
    cfg.system_message = "Tools:\n" + describe_tools(f.registry);
    Harness h(std::make_unique<ListModel>(std::vector<std::string>{}), cfg, &f.registry, &f.ctx);
    REQUIRE(h.conversation().size() == 1);
    const Message& sys = h.conversation().at(0);
    CHECK(sys.role() == Role::System);
    for (const char* n : {"read_file", "write_file", "list_dir", "calculator"})
        CHECK(sys.content().find(n) != std::string::npos);
}

TEST(loop_transcript_round_trip_with_tools) {
    LoopFixture f;
    REQUIRE(f.registry.find("calculator") != nullptr);   // needs your ToolRegistry
    const std::string path = (f.dir.path() / "t.txt").string();
    std::vector<std::string> replies = {
        "Reading." + call("c1", "read_file", R"({"path":"todo.txt"})"),
        "Milk.",
        "Bye<|end_conversation|>",
    };
    Conversation first;
    {
        LinesInput in({"todo?", "thanks"});
        StringOutput out;
        HarnessConfig cfg;
        cfg.system_message = "sys line 1\nsys line 2";
        Harness h(std::make_unique<ListModel>(replies), cfg, &f.registry, &f.ctx);
        h.run(in, out);
        first = h.conversation();
        REQUIRE(save_transcript(first, path));
    }
    Conversation loaded;
    std::string err;
    REQUIRE(load_transcript(path, &loaded, &err));
    REQUIRE(loaded.size() == first.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(loaded.at(i).role() == first.at(i).role());
        CHECK(loaded.at(i).content() == first.at(i).content());
    }
    // Replaying the saved assistant turns re-runs the same tools and
    // reproduces the same conversation.
    LinesInput in2({"todo?", "thanks"});
    StringOutput out2;
    HarnessConfig cfg2;
    cfg2.system_message = "sys line 1\nsys line 2";
    Harness h2(std::make_unique<ReplayModelClient>(path), cfg2, &f.registry, &f.ctx);
    CHECK(h2.run(in2, out2).kind == StopReason::Kind::Sentinel);
    REQUIRE(h2.conversation().size() == first.size());
    for (std::size_t i = 0; i < first.size(); ++i)
        CHECK(h2.conversation().at(i).content() == first.at(i).content());
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }

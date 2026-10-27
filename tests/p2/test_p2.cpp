// tests/p2/test_p2.cpp
//
// PROVIDED. P2 regression suite. It uses only the P2 public interface of
// Conversation and SentinelScanner, plus the provided Harness with NO tools,
// so it must pass unchanged on top of your P2 code (interface-freeze rule).
// You may add your own P2 tests to this file.

#include "../mini_test.h"

#include "core/conversation.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "harness/transcript.h"
#include "model/replay_client.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

class LinesInput : public InputSource {
public:
    explicit LinesInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}
    std::string read_line() override {
        if (i_ >= lines_.size()) { eof_ = true; return ""; }
        return lines_[i_++];
    }
    bool is_eof() const override { return eof_; }
private:
    std::vector<std::string> lines_;
    std::size_t i_ = 0;
    bool eof_ = false;
};

class StringOutput : public OutputSink {
public:
    std::string data;
    void write(std::string_view t) override { data += t; }
};

class FixedModel : public ModelClient {
public:
    using ModelClient::generate;
    explicit FixedModel(std::string reply, std::size_t chunk = 0)
        : reply_(std::move(reply)), chunk_(chunk) {}
    void generate(const Conversation&, TokenSink& sink) override {
        if (chunk_ == 0) sink.on_chunk(reply_);
        else for (std::size_t i = 0; i < reply_.size(); i += chunk_) sink.on_chunk(reply_.substr(i, chunk_));
        sink.on_complete();
    }
private:
    std::string reply_;
    std::size_t chunk_;
};

std::string scan_all(SentinelScanner& s, const std::vector<std::string>& chunks, bool* found) {
    std::string out;
    *found = false;
    for (const auto& c : chunks) {
        auto r = s.feed(c);
        out += r.safe_text;
        if (r.sentinel_found) { *found = true; return out; }
    }
    out += s.flush().safe_text;
    return out;
}

const std::string kSent = "<|end_conversation|>";

}  // namespace

// ---------------------------------------------------------------- Conversation

TEST(conv_empty) {
    Conversation c;
    CHECK(c.size() == 0);
    CHECK(c.begin() == c.end());
}

TEST(conv_append_and_order) {
    Conversation c;
    c.append(Message(Role::System, "sys"));
    for (int i = 0; i < 100; ++i) c.append(Message(Role::User, std::to_string(i)));
    REQUIRE(c.size() == 101);
    CHECK(c.at(0).role() == Role::System);
    for (int i = 0; i < 100; ++i) CHECK(c.at(i + 1).content() == std::to_string(i));
}

TEST(conv_holds_tool_role) {   // P3: the new enum value must flow through
    Conversation c;
    c.append(Message(Role::Tool, "<|tool_result|>{}<|/tool_result|>"));
    REQUIRE(c.size() == 1);
    CHECK(c.at(0).role() == Role::Tool);
}

TEST(conv_range_for) {
    Conversation c;
    for (int i = 0; i < 10; ++i) c.append(Message(Role::User, std::to_string(i)));
    int k = 0;
    for (const Message& m : c) CHECK(m.content() == std::to_string(k++));
    CHECK(k == 10);
}

TEST(conv_copy_is_deep) {
    Conversation a;
    for (int i = 0; i < 5; ++i) a.append(Message(Role::User, "m" + std::to_string(i)));
    Conversation b = a;
    REQUIRE(b.size() == a.size());
    CHECK(a.begin() != b.begin());
    b.append(Message(Role::User, "only in b"));
    CHECK(a.size() == 5);
    CHECK(b.size() == 6);
    Conversation c;
    c = a;
    CHECK(c.begin() != a.begin());
    CHECK(c.at(4).content() == "m4");
    Conversation& alias = c;
    c = alias;   // self-assignment must be harmless
    CHECK(c.size() == 5);
}

TEST(conv_move_steals) {
    Conversation a;
    for (int i = 0; i < 5; ++i) a.append(Message(Role::User, "x"));
    const Message* p = a.begin();
    Conversation b = std::move(a);
    CHECK(b.begin() == p);
    CHECK(a.size() == 0);   // NOLINT(bugprone-use-after-move): spec requires valid+empty
    Conversation c;
    c.append(Message(Role::User, "old"));
    c = std::move(b);
    CHECK(c.begin() == p);
    CHECK(c.size() == 5);
    a.append(Message(Role::User, "moved-from is reusable"));
    CHECK(a.size() == 1);
}

// ---------------------------------------------------------- SentinelScanner

TEST(scan_clean_text) {
    SentinelScanner s(kSent);
    bool found = false;
    CHECK(scan_all(s, {"Just ", "some text."}, &found) == "Just some text.");
    CHECK(!found);
}

TEST(scan_every_split) {
    const std::string text = "Goodbye." + kSent;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner s(kSent);
        bool found = false;
        std::string out = scan_all(s, {text.substr(0, split), text.substr(split)}, &found);
        CHECK(found);
        CHECK(out == "Goodbye.");
    }
}

TEST(scan_one_byte_at_a_time) {
    const std::string text = "abc<|end_<|end_conv" + kSent + "ignored";
    SentinelScanner s(kSent);
    std::vector<std::string> chunks;
    for (char ch : text) chunks.push_back(std::string(1, ch));
    bool found = false;
    CHECK(scan_all(s, chunks, &found) == "abc<|end_<|end_conv");
    CHECK(found);
}

TEST(scan_false_alarm) {
    SentinelScanner s(kSent);
    bool found = false;
    CHECK(scan_all(s, {"Here is <|end_", "world|> done"}, &found) == "Here is <|end_world|> done");
    CHECK(!found);
}

// ------------------------------------------------- Harness (no tools = P2)

TEST(harness_turn_limit) {
    LinesInput in({"a", "b", "c", "d", "e"});
    StringOutput out;
    HarnessConfig cfg;
    cfg.max_turns = 2;
    Harness h(std::make_unique<FixedModel>("reply"), cfg);
    CHECK(h.run(in, out).kind == StopReason::Kind::TurnLimit);
    CHECK(h.conversation().size() == 4);
}

TEST(harness_eof) {
    LinesInput in({});
    StringOutput out;
    Harness h(std::make_unique<FixedModel>("reply"), HarnessConfig{});
    CHECK(h.run(in, out).kind == StopReason::Kind::UserExit);
}

TEST(harness_sentinel_halts_and_is_hidden) {
    LinesInput in({"hi", "again"});
    StringOutput out;
    Harness h(std::make_unique<FixedModel>("Bye now." + kSent, 3), HarnessConfig{});
    CHECK(h.run(in, out).kind == StopReason::Kind::Sentinel);
    CHECK(out.data.find("<|end_") == std::string::npos);
    CHECK(out.data.find("Bye now.") != std::string::npos);
}

TEST(harness_transcript_round_trip) {
    const std::string path = "p2_round_trip.txt";
    {
        LinesInput in({"one", "two"});
        StringOutput out;
        Harness h(std::make_unique<FixedModel>("same reply"), HarnessConfig{});
        h.run(in, out);
        REQUIRE(save_transcript(h.conversation(), path));
    }
    ReplayModelClient replay(path);
    Conversation empty;
    CHECK(replay.generate(empty).content() == "same reply");
    CHECK(replay.generate(empty).content() == "same reply");
    std::remove(path.c_str());
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }

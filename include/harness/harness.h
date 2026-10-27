// include/harness/harness.h
//
// PROVIDED. The P2 turn loop, extended with the P3 tool loop:
//
//   user turn ──► model ──► [tool calls?] ──yes──► run tools, append results
//                   ▲                                   │
//                   └──────────── (no user turn) ───────┘
//                               no
//                               └──► back to the user
//
// Constructing a Harness without a ToolRegistry gives exactly the P2
// behavior, so every P2 test still compiles and passes unchanged.

#pragma once
#include "harness/tool_dispatcher.h"
#include "model/model_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

inline constexpr std::string_view kEndSentinel = "<|end_conversation|>";

struct HarnessConfig {
    int         max_turns = 20;       // user turns before TurnLimit
    int         max_tool_steps = 8;   // tool rounds allowed per user turn
    std::string system_message;       // appended first if non-empty
};

class InputSource {
public:
    virtual ~InputSource() = default;
    virtual std::string read_line() = 0;
    virtual bool is_eof() const = 0;
};

class OutputSink {
public:
    virtual ~OutputSink() = default;
    virtual void write(std::string_view text) = 0;
};

class Harness {
public:
    // `tools` and `ctx` are optional and NOT owned; if given, they must
    // outlive the Harness. Pass both or neither.
    Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg,
            ToolRegistry* tools = nullptr, ToolContext* ctx = nullptr);

    StopReason run(InputSource& in, OutputSink& out);

    const Conversation& conversation() const { return conv_; }

private:
    void print_call(OutputSink& out, const DispatchedCall& call) const;

    std::unique_ptr<ModelClient>  model_;
    Conversation                  conv_;
    HarnessConfig                 cfg_;
    ToolContext*                  ctx_ = nullptr;
    std::optional<ToolDispatcher> dispatcher_;
};

// include/core/message.h
//
// PROVIDED (P3 version). Replaces the message.h you wrote in P2.
// The only changes from P2: a fourth role, Role::Tool, and two small helpers
// for converting roles to and from the transcript spelling.

#pragma once
#include <string>
#include <string_view>
#include <utility>

enum class Role { System, User, Assistant, Tool };

class Message {
public:
    // Default-constructs an empty System message (needed so Conversation can
    // allocate raw array slots before append() fills them in).
    Message() : role_(Role::System) {}

    Message(Role role, std::string content)
        : role_(role), content_(std::move(content)) {}

    Role               role()    const noexcept { return role_; }
    const std::string& content() const noexcept { return content_; }

private:
    Role        role_;
    std::string content_;
};

// "system", "user", "assistant", "tool" — the spelling used in transcripts.
inline const char* role_to_string(Role r) noexcept {
    switch (r) {
        case Role::System:    return "system";
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
        case Role::Tool:      return "tool";
    }
    return "system";
}

// Inverse of role_to_string. Returns false (and leaves *out alone) on an
// unknown spelling.
inline bool role_from_string(std::string_view s, Role* out) noexcept {
    if (s == "system")    { *out = Role::System;    return true; }
    if (s == "user")      { *out = Role::User;      return true; }
    if (s == "assistant") { *out = Role::Assistant; return true; }
    if (s == "tool")      { *out = Role::Tool;      return true; }
    return false;
}

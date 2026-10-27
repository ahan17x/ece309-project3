// src/model/replay_client.cpp — PROVIDED.
//
// Replays the assistant blocks of a saved transcript, in order. P3 change:
// uses the shared transcript reader, so empty assistant replies and
// "role: tool" blocks are handled the same way --save writes them.

#include "model/replay_client.h"
#include "harness/transcript.h"

#include <stdexcept>

ReplayModelClient::ReplayModelClient(const std::string& transcript_path) {
    Conversation conv;
    std::string error;
    if (!load_transcript(transcript_path, &conv, &error))
        throw std::runtime_error("Could not load transcript: " + error);
    for (const Message& m : conv)
        if (m.role() == Role::Assistant) assistant_replies_.push_back(m.content());
}

void ReplayModelClient::generate(const Conversation& /*conv*/, TokenSink& sink) {
    std::string reply;
    if (current_turn_ < assistant_replies_.size()) {
        reply = assistant_replies_[current_turn_++];
    } else {
        reply = "<|end_conversation|>";   // out of recorded data: end cleanly
    }
    sink.on_chunk(reply);
    sink.on_complete();
}

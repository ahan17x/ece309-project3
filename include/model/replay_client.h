// include/model/replay_client.h

#pragma once
#include "model/model_client.h"
#include <string>
#include <vector>

class ReplayModelClient : public ModelClient {
public:

    using ModelClient::generate;
    explicit ReplayModelClient(const std::string& transcript_path);
    void generate(const Conversation& conv, TokenSink& sink) override;

private:
    std::vector<std::string> assistant_replies_;
    std::size_t current_turn_ = 0;
};
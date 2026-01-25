#pragma once

#include <memory>
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include "AppState.hpp"

template <typename Mutex>
class ImGuiLogSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit ImGuiLogSink(AppState& state) : state_(state) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        state_.AppendLog(std::string(formatted.begin(), formatted.end()));
    }

    void flush_() override {}

private:
    AppState& state_;
};

using ImGuiLogSink_mt = ImGuiLogSink<std::mutex>;
using ImGuiLogSink_st = ImGuiLogSink<spdlog::details::null_mutex>;

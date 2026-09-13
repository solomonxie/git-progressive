#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "agent/loop.hpp"

using namespace gitprogressive;

namespace {

// Returns one scripted CompletionResponse per call, ignoring the request;
// the last response repeats if the loop asks for more than were scripted.
class ScriptedProvider : public Provider {
public:
    explicit ScriptedProvider(std::vector<CompletionResponse> responses) : responses_(std::move(responses)) {}

    CompletionResponse complete(const CompletionRequest&) override {
        CompletionResponse r = responses_[std::min<size_t>(calls_, responses_.size() - 1)];
        ++calls_;
        return r;
    }

    int calls() const { return calls_; }

private:
    std::vector<CompletionResponse> responses_;
    int calls_ = 0;
};

// Always succeeds; used for non-terminal tool calls in a script.
class EchoTool : public Tool {
public:
    ToolSpec spec() const override { return {"echo", "echoes back", "{}"}; }
    std::string invoke(const std::string&) override { return R"({"ok":true})"; }
};

// Fails its first `failures` invocations, then succeeds — models the
// self-correction loop around submit_plan-style validation.
class FlakySubmitTool : public Tool {
public:
    explicit FlakySubmitTool(int failures) : failures_(failures) {}
    ToolSpec spec() const override { return {"submit", "submits the plan", "{}"}; }
    std::string invoke(const std::string&) override {
        if (calls_++ < failures_) return R"({"ok":false,"error":"not yet valid"})";
        return R"({"ok":true})";
    }
    int calls() const { return calls_; }

private:
    int failures_;
    int calls_ = 0;
};

std::vector<std::unique_ptr<Tool>> makeTools(int submitFailures) {
    std::vector<std::unique_ptr<Tool>> tools;
    tools.push_back(std::make_unique<EchoTool>());
    tools.push_back(std::make_unique<FlakySubmitTool>(submitFailures));
    return tools;
}

ToolCall call(const std::string& name) { return {"call-1", name, "{}"}; }

} // namespace

TEST_CASE("AgentLoop stops as soon as the terminal tool succeeds") {
    ScriptedProvider provider({{"", {call("submit")}}});
    AgentLoop loop(provider, "system", makeTools(0));
    CHECK(loop.run("go", "submit", 5) == "submit");
    CHECK(provider.calls() == 1);
}

TEST_CASE("AgentLoop retries the terminal tool after a validation failure") {
    ScriptedProvider provider({{"", {call("submit")}}, {"", {call("submit")}}});
    AgentLoop loop(provider, "system", makeTools(1)); // fails once, then succeeds
    CHECK(loop.run("go", "submit", 5) == "submit");
    CHECK(provider.calls() == 2);
}

TEST_CASE("AgentLoop dispatches a non-terminal tool before the terminal one") {
    ScriptedProvider provider({{"", {call("echo")}}, {"", {call("submit")}}});
    AgentLoop loop(provider, "system", makeTools(0));
    CHECK(loop.run("go", "submit", 5) == "submit");
    CHECK(provider.calls() == 2);
}

TEST_CASE("AgentLoop gives up at the iteration cap without a valid terminal call") {
    // Always fails: never returns a terminal call the loop accepts.
    ScriptedProvider provider({{"", {call("submit")}}});
    AgentLoop loop(provider, "system", makeTools(1000));
    CHECK(loop.run("go", "submit", 3).empty());
    CHECK(provider.calls() == 3);
}

TEST_CASE("AgentLoop prompts the model to continue when it calls no tool") {
    ScriptedProvider provider({{"thinking out loud", {}}, {"", {call("submit")}}});
    AgentLoop loop(provider, "system", makeTools(0));
    CHECK(loop.run("go", "submit", 5) == "submit");
    CHECK(provider.calls() == 2);
}

TEST_CASE("AgentLoop reports an unknown tool call as a failed result, not a crash") {
    ScriptedProvider provider({{"", {call("nonexistent")}}, {"", {call("submit")}}});
    AgentLoop loop(provider, "system", makeTools(0));
    CHECK(loop.run("go", "submit", 5) == "submit");
}

// include/tools/calculator_tool.h
//
// PROVIDED. The `calculator` tool. It glues together the provided tokenizer
// and YOUR calc_evaluate (include/tools/calculator.h).
//   arguments: {"expression": "<string>"}   (required)
//   success content: the result, formatted with format_number()

#pragma once
#include "tools/tool.h"

#include <string>
#include <string_view>

class CalculatorTool : public Tool {
public:
    CalculatorTool();

    std::string      name() const override;          // "calculator"
    std::string      description() const override;
    const JsonValue& schema() const override;
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;

    // calc_tokenize + calc_evaluate + a check that the result is finite.
    // success: returns true, stores the value in *out.
    // failure: returns false, stores a non-empty message in *error.
    static bool evaluate(std::string_view expression, double* out, std::string* error);

private:
    JsonValue schema_;
};

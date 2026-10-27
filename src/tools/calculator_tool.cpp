// src/tools/calculator_tool.cpp — PROVIDED.

#include "tools/calculator_tool.h"
#include "tools/calculator.h"
#include "util/number_format.h"

#include <cmath>
#include <utility>
#include <vector>

CalculatorTool::CalculatorTool() {
    // {"type":"object",
    //  "properties":{"expression":{"type":"string","description":"..."}},
    //  "required":["expression"]}
    JsonValue expr = JsonValue::object();
    expr.set("type", JsonValue::string("string"));
    expr.set("description", JsonValue::string("Arithmetic using + - * / and parentheses, e.g. \"(2+3)*4\""));
    JsonValue props = JsonValue::object();
    props.set("expression", std::move(expr));
    JsonValue req = JsonValue::array();
    req.push_back(JsonValue::string("expression"));

    schema_ = JsonValue::object();
    schema_.set("type", JsonValue::string("object"));
    schema_.set("properties", std::move(props));
    schema_.set("required", std::move(req));
}

std::string CalculatorTool::name() const { return "calculator"; }

std::string CalculatorTool::description() const {
    return "Evaluate an arithmetic expression and return the numeric result.";
}

const JsonValue& CalculatorTool::schema() const { return schema_; }

bool CalculatorTool::evaluate(std::string_view expression, double* out, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expression, &tokens, error)) return false;
    double value = 0.0;
    if (!calc_evaluate(tokens, &value, error)) {
        if (error->empty()) *error = "could not evaluate expression";
        return false;
    }
    if (!std::isfinite(value)) {
        *error = "result is too large";
        return false;
    }
    *out = value;
    return true;
}

ToolResult CalculatorTool::invoke(const JsonValue& args, ToolContext& /*ctx*/) {
    const JsonValue* e = args.find("expression");
    if (e == nullptr) return ToolResult::failure("missing required argument: expression");
    if (e->type() != JsonValue::Type::String)
        return ToolResult::failure("argument 'expression' must be a string");

    double value = 0.0;
    std::string error;
    if (!evaluate(e->as_string(), &value, &error)) return ToolResult::failure(error);
    return ToolResult::success(format_number(value));
}

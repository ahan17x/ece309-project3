#include "tools/calculator.h"

#include <vector>

namespace {

using Kind = CalcToken::Kind;

int precedence(Kind k) {
    if (k == Kind::Times || k == Kind::Divide) return 2;
    if (k == Kind::Plus || k == Kind::Minus) return 1;
    return 0;
}

bool is_operator(Kind k) {
    return k == Kind::Plus || k == Kind::Minus || k == Kind::Times || k == Kind::Divide;
}

bool apply_top(std::vector<double>& values, std::vector<Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "malformed expression";
        return false;
    }
    Kind op = ops.back();
    ops.pop_back();

    double right = values.back();
    values.pop_back();
    double left = values.back();
    values.pop_back();

    double r = 0.0;
    switch (op) {
        case Kind::Plus:  r = left + right; break;
        case Kind::Minus: r = left - right; break;
        case Kind::Times: r = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            r = left / right;
            break;
        default:
            *error = "malformed expression";
            return false;
    }
    values.push_back(r);
    return true;
}

}

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    if (tokens.empty()) {
        *error = "empty expression";
        return false;
    }

    std::vector<double> values;
    std::vector<Kind> ops;

    for (const CalcToken& t : tokens) {
        if (t.kind == Kind::Number) {
            values.push_back(t.value);
        } else if (t.kind == Kind::LParen) {
            ops.push_back(Kind::LParen);
        } else if (t.kind == Kind::RParen) {
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply_top(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "mismatched parentheses";
                return false;
            }
            ops.pop_back();
        } else {
            while (!ops.empty() && is_operator(ops.back()) &&
                   precedence(ops.back()) >= precedence(t.kind)) {
                if (!apply_top(values, ops, error)) return false;
            }
            ops.push_back(t.kind);
        }
    }

    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "mismatched parentheses";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.back();
    return true;
}

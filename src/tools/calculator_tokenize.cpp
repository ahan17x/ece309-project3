// src/tools/calculator_tokenize.cpp — PROVIDED.
//
// Turns text into CalcTokens and checks the sequence is well-formed.
// The check is a tiny two-state machine: at every point we either EXPECT AN
// OPERAND (a number or '(') or EXPECT AN OPERATOR (+ - * / or ')').

#include "tools/calculator.h"

#include <cstdlib>
#include <string>

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

std::string at(std::size_t pos) { return " at position " + std::to_string(pos); }

}  // namespace

bool calc_tokenize(std::string_view s, std::vector<CalcToken>* out, std::string* error) {
    out->clear();
    bool expect_operand = true;   // state of the machine
    int  open_parens = 0;         // '(' seen minus ')' seen
    std::size_t i = 0;

    while (i < s.size()) {
        char c = s[i];
        if (c == ' ' || c == '\t') { ++i; continue; }

        if (expect_operand) {
            // A number, optionally with a '-' glued to its front.
            bool negative = false;
            std::size_t start = i;
            if (c == '-' && i + 1 < s.size() && is_digit(s[i + 1])) {
                negative = true;
                ++i;
            }
            if (is_digit(s[i])) {
                std::size_t digits_start = i;
                while (i < s.size() && is_digit(s[i])) ++i;
                if (i < s.size() && s[i] == '.') {
                    ++i;
                    if (i >= s.size() || !is_digit(s[i])) {
                        *error = "expected a digit after '.'" + at(i);
                        return false;
                    }
                    while (i < s.size() && is_digit(s[i])) ++i;
                }
                std::string text(s.substr(digits_start, i - digits_start));
                CalcToken t;
                t.kind = CalcToken::Kind::Number;
                t.value = std::strtod(text.c_str(), nullptr);
                if (negative) t.value = -t.value;
                out->push_back(t);
                expect_operand = false;
                continue;
            }
            if (c == '(') {
                out->push_back({CalcToken::Kind::LParen, 0.0});
                ++open_parens;
                ++i;
                continue;
            }
            *error = "expected a number or '('" + at(start);
            return false;
        }

        // expect an operator or ')'
        CalcToken t;
        switch (c) {
            case '+': t.kind = CalcToken::Kind::Plus;   break;
            case '-': t.kind = CalcToken::Kind::Minus;  break;
            case '*': t.kind = CalcToken::Kind::Times;  break;
            case '/': t.kind = CalcToken::Kind::Divide; break;
            case ')':
                if (open_parens == 0) {
                    *error = "unmatched ')'" + at(i);
                    return false;
                }
                --open_parens;
                out->push_back({CalcToken::Kind::RParen, 0.0});
                ++i;
                continue;   // still expecting an operator after ')'
            default:
                *error = std::string("unexpected character '") + c + "'" + at(i);
                return false;
        }
        out->push_back(t);
        expect_operand = true;
        ++i;
    }

    if (out->empty()) {
        *error = "empty expression";
        return false;
    }
    if (expect_operand) {
        *error = "expression ends with an operator";
        return false;
    }
    if (open_parens != 0) {
        *error = "missing ')'";
        return false;
    }
    return true;
}

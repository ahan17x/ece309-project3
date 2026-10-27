// include/tools/calculator.h
//
// The calculator in two steps:
//
//   "(2+3)*4"  --calc_tokenize-->  [ (  2  +  3  )  *  4 ]  --calc_evaluate-->  20
//               PROVIDED                                      YOU IMPLEMENT
//
// calc_tokenize (src/tools/calculator_tokenize.cpp) does all the character-
// level work AND checks that the tokens form a well-formed expression, so
// calc_evaluate only ever sees valid input. You implement calc_evaluate
// (src/tools/calculator_eval.cpp) with the two-stack algorithm (spec §4.6).

#pragma once
#include <string>
#include <string_view>
#include <vector>

struct CalcToken {
    enum class Kind { Number, Plus, Minus, Times, Divide, LParen, RParen };

    Kind   kind  = Kind::Number;
    double value = 0.0;          // used only when kind == Number (may be negative)
};

// PROVIDED. Splits `expression` into tokens.
//   Numbers:   digits with an optional fraction: 7, 007, 3.25
//   Operators: + - * /   Parentheses: ( )   Spaces and tabs are skipped.
//   A '-' directly in front of a number, where a number is expected, is
//   part of that number: "-3", "2*-3", "(-1)". Anything else ("-(2)", "--2",
//   "+5") is an error.
// On success: returns true and fills *out with a WELL-FORMED sequence:
//   numbers and operators alternate, parentheses are balanced, it starts and
//   ends with a number or a parenthesis, and it is not empty.
// On failure: returns false and stores a non-empty message in *error.
bool calc_tokenize(std::string_view expression, std::vector<CalcToken>* out,
                   std::string* error);

// YOU IMPLEMENT. Evaluates a token sequence produced by calc_tokenize.
//   * and / bind tighter than + and -; operators of equal precedence are
//   applied left to right (10-4-3 is 3); parentheses group.
// On success: returns true and stores the result in *out.
// On division by zero: returns false and stores "division by zero" in *error.
// You may assume `tokens` is well-formed (it came from calc_tokenize), but
// your function must still not crash if it isn't — return false instead.
bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error);

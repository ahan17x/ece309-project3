// include/util/number_format.h
//
// PROVIDED. One canonical way to turn a double into text, shared by
// JsonValue::dump() and CalculatorTool so that every test can be byte-exact.
//
//   format_number(42.0)   == "42"
//   format_number(-0.0)   == "0"
//   format_number(3.5)    == "3.5"
//   format_number(0.1)    == "0.1"
//   format_number(1e21)   == "1e+21"
//   format_number(1.0/3)  == "0.3333333333333333"
//
// Rule: a finite integral value with |v| < 2^53 prints as a plain integer.
// Anything else prints with the fewest significant digits (15, 16 or 17)
// that parse back to exactly the same double. Non-finite values print as
// "null" (JSON has no NaN/Infinity); callers should reject them earlier.

#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

inline std::string format_number(double v) {
    if (!std::isfinite(v)) return "null";
    if (v == 0.0) return "0";   // also folds -0.0
    if (std::fabs(v) < 9007199254740992.0 && v == std::floor(v)) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f", v);
        return buf;
    }
    char buf[40];
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    return buf;
}

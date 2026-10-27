// src/json/json_value.cpp — PROVIDED. You do not need to read this file.
//
// Representation: separate members (type tag + one field per kind).
// The parser reads the text left to right; nesting depth is capped at
// max_depth so hostile input like "[[[[[[..." produces an error, not a crash.

#include "json/json_value.h"
#include "util/number_format.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <utility>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

JsonValue::JsonValue() = default;
JsonValue::JsonValue(const JsonValue&) = default;
JsonValue& JsonValue::operator=(const JsonValue&) = default;
JsonValue::JsonValue(JsonValue&&) noexcept = default;
JsonValue& JsonValue::operator=(JsonValue&&) noexcept = default;
JsonValue::~JsonValue() = default;

JsonValue JsonValue::null() { return JsonValue(); }

JsonValue JsonValue::boolean(bool b) {
    JsonValue v;
    v.type_ = Type::Bool;
    v.bool_ = b;
    return v;
}

JsonValue JsonValue::number(double d) {
    JsonValue v;
    v.type_ = Type::Number;
    v.number_ = d;
    return v;
}

JsonValue JsonValue::string(std::string s) {
    JsonValue v;
    v.type_ = Type::String;
    v.string_ = std::move(s);
    return v;
}

JsonValue JsonValue::array() {
    JsonValue v;
    v.type_ = Type::Array;
    return v;
}

JsonValue JsonValue::object() {
    JsonValue v;
    v.type_ = Type::Object;
    return v;
}

// ---------------------------------------------------------------------------
// Inspection
// ---------------------------------------------------------------------------

JsonValue::Type JsonValue::type() const noexcept { return type_; }

bool JsonValue::as_bool() const {
    if (type_ != Type::Bool) throw std::logic_error("JsonValue: not a bool");
    return bool_;
}

double JsonValue::as_number() const {
    if (type_ != Type::Number) throw std::logic_error("JsonValue: not a number");
    return number_;
}

const std::string& JsonValue::as_string() const {
    if (type_ != Type::String) throw std::logic_error("JsonValue: not a string");
    return string_;
}

std::size_t JsonValue::size() const {
    if (type_ != Type::Array && type_ != Type::Object)
        throw std::logic_error("JsonValue: size() on a non-container");
    return items_.size();
}

const JsonValue& JsonValue::operator[](std::size_t i) const {
    if (type_ != Type::Array && type_ != Type::Object)
        throw std::logic_error("JsonValue: operator[] on a non-container");
    if (i >= items_.size()) throw std::out_of_range("JsonValue: index out of range");
    return items_[i];
}

const std::string& JsonValue::key_at(std::size_t i) const {
    if (type_ != Type::Object) throw std::logic_error("JsonValue: key_at() on a non-object");
    if (i >= keys_.size()) throw std::out_of_range("JsonValue: index out of range");
    return keys_[i];
}

const JsonValue* JsonValue::find(std::string_view key) const {
    if (type_ != Type::Object) return nullptr;
    for (std::size_t i = 0; i < keys_.size(); ++i)
        if (keys_[i] == key) return &items_[i];
    return nullptr;
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------

void JsonValue::push_back(JsonValue v) {
    if (type_ != Type::Array) throw std::logic_error("JsonValue: push_back() on a non-array");
    items_.push_back(std::move(v));
}

void JsonValue::set(std::string key, JsonValue v) {
    if (type_ != Type::Object) throw std::logic_error("JsonValue: set() on a non-object");
    for (std::size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i] == key) {
            items_[i] = std::move(v);
            return;
        }
    }
    keys_.push_back(std::move(key));
    items_.push_back(std::move(v));
}

bool JsonValue::operator==(const JsonValue& o) const {
    if (type_ != o.type_) return false;
    switch (type_) {
        case Type::Null:   return true;
        case Type::Bool:   return bool_ == o.bool_;
        case Type::Number: return number_ == o.number_;
        case Type::String: return string_ == o.string_;
        case Type::Array:  return items_ == o.items_;
        case Type::Object: return keys_ == o.keys_ && items_ == o.items_;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

namespace {

void dump_string(const std::string& s, std::string& out) {
    static const char* hex = "0123456789abcdef";
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 0xF];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

}  // namespace

// A small recursive helper; depth is bounded because every tree we build by
// parsing is bounded by max_depth.
static void dump_into(const JsonValue& v, std::string& out) {
    switch (v.type()) {
        case JsonValue::Type::Null:   out += "null"; break;
        case JsonValue::Type::Bool:   out += v.as_bool() ? "true" : "false"; break;
        case JsonValue::Type::Number: out += format_number(v.as_number()); break;
        case JsonValue::Type::String: dump_string(v.as_string(), out); break;
        case JsonValue::Type::Array:
            out += '[';
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i) out += ',';
                dump_into(v[i], out);
            }
            out += ']';
            break;
        case JsonValue::Type::Object:
            out += '{';
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i) out += ',';
                dump_string(v.key_at(i), out);
                out += ':';
                dump_into(v[i], out);
            }
            out += '}';
            break;
    }
}

std::string JsonValue::dump() const {
    std::string out;
    dump_into(*this, out);
    return out;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

namespace {

class Parser {
public:
    Parser(std::string_view text, std::size_t max_depth)
        : s_(text), max_depth_(max_depth) {}

    bool parse_document(JsonValue* out) {
        skip_ws();
        if (!parse_value(out, 0)) return false;
        skip_ws();
        if (pos_ != s_.size()) return fail("unexpected trailing characters");
        return true;
    }

    const std::string& error() const { return error_; }

private:
    std::string_view s_;
    std::size_t      pos_ = 0;
    std::size_t      max_depth_;
    std::string      error_;

    bool fail(const std::string& what) {
        if (error_.empty())
            error_ = "at offset " + std::to_string(pos_) + ": " + what;
        return false;
    }

    bool at_end() const { return pos_ >= s_.size(); }
    char peek() const { return s_[pos_]; }

    void skip_ws() {
        while (!at_end()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }

    bool literal(std::string_view word) {
        if (s_.substr(pos_, word.size()) != word)
            return fail("invalid literal");
        pos_ += word.size();
        return true;
    }

    bool parse_value(JsonValue* out, std::size_t depth) {
        if (at_end()) return fail("unexpected end of input, expected a value");
        switch (peek()) {
            case '{': return parse_object(out, depth + 1);
            case '[': return parse_array(out, depth + 1);
            case '"': {
                std::string s;
                if (!parse_string(&s)) return false;
                *out = JsonValue::string(std::move(s));
                return true;
            }
            case 't': if (!literal("true"))  return false; *out = JsonValue::boolean(true);  return true;
            case 'f': if (!literal("false")) return false; *out = JsonValue::boolean(false); return true;
            case 'n': if (!literal("null"))  return false; *out = JsonValue::null();         return true;
            default:
                if (peek() == '-' || (peek() >= '0' && peek() <= '9')) return parse_number(out);
                return fail(std::string("unexpected character '") + peek() + "'");
        }
    }

    bool parse_array(JsonValue* out, std::size_t depth) {
        if (depth > max_depth_) return fail("maximum nesting depth exceeded");
        ++pos_;  // '['
        JsonValue arr = JsonValue::array();
        skip_ws();
        if (!at_end() && peek() == ']') { ++pos_; *out = std::move(arr); return true; }
        for (;;) {
            JsonValue elem;
            skip_ws();
            if (!parse_value(&elem, depth)) return false;
            arr.push_back(std::move(elem));
            skip_ws();
            if (at_end()) return fail("unterminated array");
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; break; }
            return fail("expected ',' or ']' in array");
        }
        *out = std::move(arr);
        return true;
    }

    bool parse_object(JsonValue* out, std::size_t depth) {
        if (depth > max_depth_) return fail("maximum nesting depth exceeded");
        ++pos_;  // '{'
        JsonValue obj = JsonValue::object();
        skip_ws();
        if (!at_end() && peek() == '}') { ++pos_; *out = std::move(obj); return true; }
        for (;;) {
            skip_ws();
            if (at_end() || peek() != '"') return fail("expected string key in object");
            std::string key;
            if (!parse_string(&key)) return false;
            if (obj.find(key) != nullptr) return fail("duplicate key \"" + key + "\"");
            skip_ws();
            if (at_end() || peek() != ':') return fail("expected ':' after object key");
            ++pos_;
            skip_ws();
            JsonValue val;
            if (!parse_value(&val, depth)) return false;
            obj.set(std::move(key), std::move(val));
            skip_ws();
            if (at_end()) return fail("unterminated object");
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; break; }
            return fail("expected ',' or '}' in object");
        }
        *out = std::move(obj);
        return true;
    }

    static int hex_digit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool read_hex4(unsigned* out) {
        if (pos_ + 4 > s_.size()) return fail("truncated \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            int d = hex_digit(s_[pos_ + i]);
            if (d < 0) return fail("invalid hex digit in \\u escape");
            v = (v << 4) | static_cast<unsigned>(d);
        }
        pos_ += 4;
        *out = v;
        return true;
    }

    static void append_utf8(unsigned cp, std::string* s) {
        if (cp < 0x80) {
            *s += static_cast<char>(cp);
        } else if (cp < 0x800) {
            *s += static_cast<char>(0xC0 | (cp >> 6));
            *s += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            *s += static_cast<char>(0xE0 | (cp >> 12));
            *s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *s += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            *s += static_cast<char>(0xF0 | (cp >> 18));
            *s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            *s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *s += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool parse_string(std::string* out) {
        ++pos_;  // opening quote
        std::string s;
        for (;;) {
            if (at_end()) return fail("unterminated string");
            unsigned char c = static_cast<unsigned char>(s_[pos_]);
            if (c == '"') { ++pos_; break; }
            if (c < 0x20) return fail("raw control character in string");
            if (c != '\\') { s += static_cast<char>(c); ++pos_; continue; }

            ++pos_;  // backslash
            if (at_end()) return fail("unterminated escape");
            char e = s_[pos_++];
            switch (e) {
                case '"':  s += '"';  break;
                case '\\': s += '\\'; break;
                case '/':  s += '/';  break;
                case 'b':  s += '\b'; break;
                case 'f':  s += '\f'; break;
                case 'n':  s += '\n'; break;
                case 'r':  s += '\r'; break;
                case 't':  s += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!read_hex4(&cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        // High surrogate: must be followed by \uDC00..\uDFFF.
                        if (pos_ + 2 > s_.size() || s_[pos_] != '\\' || s_[pos_ + 1] != 'u')
                            return fail("unpaired high surrogate");
                        pos_ += 2;
                        unsigned lo = 0;
                        if (!read_hex4(&lo)) return false;
                        if (lo < 0xDC00 || lo > 0xDFFF) return fail("invalid low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return fail("unpaired low surrogate");
                    }
                    append_utf8(cp, &s);
                    break;
                }
                default:
                    --pos_;
                    return fail("invalid escape character");
            }
        }
        *out = std::move(s);
        return true;
    }

    static bool is_digit(char c) { return c >= '0' && c <= '9'; }

    bool parse_number(JsonValue* out) {
        const std::size_t start = pos_;
        if (peek() == '-') ++pos_;
        if (at_end()) return fail("expected digit");
        if (peek() == '0') {
            ++pos_;
        } else if (is_digit(peek())) {
            while (!at_end() && is_digit(peek())) ++pos_;
        } else {
            return fail("expected digit");
        }
        if (!at_end() && peek() == '.') {
            ++pos_;
            if (at_end() || !is_digit(peek())) return fail("expected digit after '.'");
            while (!at_end() && is_digit(peek())) ++pos_;
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            ++pos_;
            if (!at_end() && (peek() == '+' || peek() == '-')) ++pos_;
            if (at_end() || !is_digit(peek())) return fail("expected digit in exponent");
            while (!at_end() && is_digit(peek())) ++pos_;
        }
        // The span [start, pos_) is now a valid JSON number; strtod needs a
        // NUL-terminated buffer, and string_view is not one.
        std::string token(s_.substr(start, pos_ - start));
        double d = std::strtod(token.c_str(), nullptr);
        if (!std::isfinite(d)) {
            pos_ = start;
            return fail("number out of range");
        }
        *out = JsonValue::number(d);
        return true;
    }
};

}  // namespace

JsonValue JsonValue::parse(std::string_view text, std::string* error_out,
                           std::size_t max_depth) {
    Parser p(text, max_depth);
    JsonValue v;
    if (!p.parse_document(&v)) {
        if (error_out) *error_out = p.error();
        return JsonValue();
    }
    if (error_out) error_out->clear();
    return v;
}

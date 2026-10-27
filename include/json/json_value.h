// include/json/json_value.h
//
// PROVIDED. You only USE this class (spec Appendix B explains JSON).
//
// A JsonValue holds one JSON value: null, a bool, a number, a string, an
// array (a list of values), or an object (a list of "key": value members).
// The dispatcher uses JsonValue::parse() to read each tool call, and
// dump() to write each tool result. In your own code you will mostly call
// type(), find(), as_string(), as_number() and as_bool() — see the examples
// in spec Appendix B.

#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    // Default nesting limit for parse(). A document nested deeper than this
    // (counting each '[' or '{' as one level) is a parse error.
    static constexpr std::size_t kDefaultMaxDepth = 128;

    // ---- Construction --------------------------------------------------
    JsonValue();                                  // Null

    static JsonValue null();
    static JsonValue boolean(bool b);
    static JsonValue number(double d);
    static JsonValue string(std::string s);
    static JsonValue array();                     // empty array
    static JsonValue object();                    // empty object

    // Copyable and movable. Copies are deep.
    JsonValue(const JsonValue&);
    JsonValue& operator=(const JsonValue&);
    JsonValue(JsonValue&&) noexcept;
    JsonValue& operator=(JsonValue&&) noexcept;
    ~JsonValue();

    // ---- Inspection ----------------------------------------------------
    Type type() const noexcept;

    // Each accessor throws std::logic_error if type() is not the matching
    // type. Calling the wrong accessor is a programmer error, not a data
    // error — check type() first.
    bool               as_bool()   const;
    double             as_number() const;
    const std::string& as_string() const;

    // Array: number of elements. Object: number of members.
    // Throws std::logic_error for any other type.
    std::size_t size() const;

    // Array: the i-th element. Object: the value of the i-th member, in
    // insertion order. Throws std::out_of_range if i >= size(), and
    // std::logic_error if this is neither an Array nor an Object.
    const JsonValue& operator[](std::size_t i) const;

    // Object only: the key of the i-th member, in insertion order.
    // Same error behavior as operator[].
    const std::string& key_at(std::size_t i) const;

    // Object only: pointer to the value for `key`, or nullptr if absent.
    // Returns nullptr (does not throw) if this is not an Object.
    const JsonValue* find(std::string_view key) const;

    // ---- Building ------------------------------------------------------
    // Array only (std::logic_error otherwise): append an element.
    void push_back(JsonValue v);

    // Object only (std::logic_error otherwise): insert or replace a member.
    // A new key goes at the end; replacing keeps the key's original position.
    void set(std::string key, JsonValue v);

    // ---- Text ----------------------------------------------------------
    // Parses `text` as exactly one JSON value (standard JSON),
    // optionally surrounded by whitespace.
    //   success: returns the value; if error_out != nullptr, clears it.
    //   failure: returns a Null value; if error_out != nullptr, stores a
    //            non-empty, human-readable message that includes the byte
    //            offset where parsing failed.
    // Never crashes and never throws, even on hostile input such as
    // "[[[[[[..." nested thousands deep (nesting beyond max_depth is an error).
    static JsonValue parse(std::string_view text, std::string* error_out,
                           std::size_t max_depth = kDefaultMaxDepth);

    // Compact serialization: no whitespace anywhere, object members in
    // insertion order, numbers via format_number() (util/number_format.h),
    // strings escaped (\" \\ \n \t ...). Guarantee: for any value v,
    // parse(v.dump()) gives back an equal value.
    std::string dump() const;

    // Deep structural equality (Object members compared in order).
    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const { return !(*this == other); }

private:
    Type                     type_ = Type::Null;
    bool                     bool_ = false;
    double                   number_ = 0.0;
    std::string              string_;
    std::vector<std::string> keys_;      // Object: keys, parallel to items_
    std::vector<JsonValue>   items_;     // Array elements / Object values
};

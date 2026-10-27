# Project 3 Design Log
**Author:** Ahan Shah · **P2 code used:** my own
**Tools used:** Claude for drafting the code, the tests, and this log from the spec. I went through all of it against the spec tables and ran the build, ctest, and the sanitizers myself.

## 1. Hash table

`ToolRegistry` is a separate-chaining table: a `std::vector` of buckets, each bucket a `std::vector<Entry>`, each `Entry` holding the key, its cached 64-bit hash, and the `std::unique_ptr<Tool>`. I went with chaining over linear probing because a chained table cannot loop forever. With open addressing, `find` on a missing name only stops when it hits an empty slot, so the load factor is a correctness requirement, not a tuning choice. Chaining also degrades gracefully if I get the threshold wrong.

The hash is FNV-1a, 64-bit: xor each byte in, then multiply by the FNV prime. Every byte goes in, and because the xor comes before the multiply, `ab` and `ba` hash differently. `hash()` returns the full value; the index is `hash & (bucket_count - 1)`, since the table starts at 16 buckets and only doubles. Each entry caches its hash, so a rehash never recomputes strings and `find` compares integers first.

The threshold is 0.75, checked before the insert, so `load_factor()` never exceeds 0.75 once `add()` returns. Under uniform hashing the expected chain length equals the load factor, so a lookup touches under two entries on average. The cost is empty buckets at 24 bytes each, about 390 KB at 16,384 buckets. On a crossing, `rehash()` doubles the bucket array and moves every entry into its new slot.

After inserting `tool_0` through `tool_9999`:

| | bucket_count | load_factor | longest chain |
|---|---|---|---|
| FNV-1a (my table) | 16,384 | 0.6104 | 6 |
| `s[0] + s.size()` at that size | 16,384 | | 9,000 |
| djb2 at that size | 16,384 | | 4 |

The weak hash only produces four values here, since every name starts with `t` and the lengths run from 6 to 9. The 9,000 nine-character names all land in one bucket. FNV-1a gives 10,000 distinct values.

**Amortized O(1).** Same geometric argument as `Conversation::append` in P2. A rehash from *n* to *2n* buckets costs O(size), but it only fires when size crosses 0.75*n*, and the previous one fired at 0.375*n*, so 0.375*n* cheap inserts separate any two rehashes. The rehash costs form a geometric series summing to under twice the last term, which is at most the insert count, so each insert pays a constant extra on average.

## 2. Pointer stability

A `Tool` is allocated once, by `std::make_unique` in the caller, and the registry stores that `unique_ptr`. A rehash does `push_back(std::move(e))`, which moves the entry (string, hash, pointer) and not the tool. The heap object never moves or gets copied, including when a bucket's own vector reallocates. The only thing that invalidates a `Tool*` is destroying the registry, and P3 has no remove. `reg_10000_tools` checks this: the pointer from the first insert equals `find("tool_0")` after 9,999 more inserts and ten doublings.

## 3. Calculator trace

`2 * (3 + 4) - 5`, with `values` for numbers and `ops` for operators.

- `2`: push. values `[2]`.
- `*`: ops is empty, so push. ops `[*]`.
- `(`: push. ops `[* (]`.
- `3`: push. values `[2 3]`.
- `+`: top of ops is `(`, not an operator, so push. ops `[* ( +]`.
- `4`: push. values `[2 3 4]`.
- `)`: apply until `(` is on top. Pop `+`, right=4, left=3, push 7. Then pop the `(`. values `[2 7]`, ops `[*]`.
- `-`: `*` on top has precedence 2 ≥ 1, so apply it: right=7, left=2, push 14. Push `-`. values `[14]`, ops `[-]`.
- `5`: push. values `[14 5]`.
- End: drain ops. Pop `-`, right=5, left=14, push 9.

One value is left, 9. The `)` forced the addition before anything outside the parentheses could use it, and the `>=` test on `-` forced the multiplication before the subtraction was pushed.

## 4. Errors as values

A tool failing is a normal event in an agent loop. The model asks for a missing file or divides by zero, and the right outcome is an `"ok":false` message it can read and react to. That result gets serialized to JSON either way, so the error is data by the time anyone sees it. Returning a `bool` plus a reason also puts the failure in the signature. Nobody can call `resolve` and forget the rejected case, because the return value is the only way to get a path out. An exception hides that and uses control flow to carry what is really data.

Exceptions are for programmer errors, where a contract was broken and no meaningful value exists: `as_string()` on a JSON number, `Conversation::at` past the end. The dispatcher still catches anything a tool throws and turns it into a failure result, so a bug in one tool does not take the harness down.

## 5. What I would design differently

I would keep entries in one dense `std::vector<Entry>` and make the bucket array integer indices into it, like CPython's dict. The bucket array drops from 24 bytes per slot to 4, a rehash only touches integers, `for_each` gets insertion order for free so `--tools` would not need to sort, and a lookup walks one contiguous array instead of a vector per bucket. I would also have `resolve` return a result struct instead of two out-pointers, since `*out` is unspecified on rejection.

# Boost.Describe - API Design Quality Evaluation

## Executive Summary

Boost.Describe is a compact, macro-based reflection library that
achieves an unusually favorable ratio of power to surface area. The
core API is three macros and three query aliases, yet it enables
generic JSON serialization, operator synthesis, enum-string
conversion, and more. Its primary design weakness is the absence of
inline documentation in the public headers and a reliance on the
user already understanding `boost::mp11::mp_for_each` before the
library becomes useful.

## Verdict

✅ PASS: 28/39 — A small, well-factored reflection primitive with
strong composability and minimal ceremony, held back by absent
inline documentation and the mp11 learning prerequisite.

## Gates

| Gate                | Result | Finding                                                                                         |
|---------------------|--------|-------------------------------------------------------------------------------------------------|
| G0. Documentation   | ✅     | AsciiDoc reference covers every macro, alias, and descriptor shape; 16 compilable examples      |
| G1. No UB Traps     | ✅     | Undescribed types cause substitution failure, not UB; `enum_to_string` returns a default on miss |
| G2. Ownership        | ✅     | Header-only, zero heap allocation, all descriptors are constexpr statics; no ownership questions |

**Gate Result:** ✅ ALL GATES PASS

### G0 Notes

The documentation exists in external AsciiDoc files, not as
Javadoc/Doxygen in the headers themselves. A user reading the header
sees only bare signatures with copyright comments. The AsciiDoc
reference, however, is thorough: it specifies every macro expansion,
every descriptor shape (`value`, `name`, `pointer`, `modifiers`),
and every precondition. The 16 example programs are self-contained
and compilable. This passes the gate, but the total absence of
inline doc comments in the headers is a notable gap (scored in
item 13).

### G1 Notes

Using `describe_enumerators<E>` on an undescribed enum is a
substitution failure (SFINAE-friendly), not undefined behavior. The
`has_describe_*` traits let users test before use.
`enum_to_string` returns a caller-supplied default rather than
invoking UB on an unrecognized value. No raw pointer ownership, no
dangling reference hazards, no destructor traps.

### G2 Notes

All descriptors are constexpr static types. There is no dynamic
allocation, no resource management, no pointers to manage. The
library produces metadata - type lists of descriptor structs with
`constexpr` members. Ownership is not a concern.

## Design Scores

| #  | Item                    | Score | |
|----|-------------------------|-------|----|
| 1  | Call-Site Clarity        | 2     | ⚠️ |
| 2  | Minimal Ceremony        | 2     | ⚠️ |
| 3  | Progressive Disclosure   | 3     | ✅ |
| 4  | Pit of Success           | 3     | ✅ |
| 5  | Naming and Vocabulary    | 2     | ⚠️ |
| 6  | Composability            | 3     | ✅ |
| 7  | Module Depth             | 3     | ✅ |
| 8  | Consistency              | 3     | ✅ |
| 9  | Error Reporting          | 2     | ⚠️ |
| 10 | Type Safety              | 2     | ⚠️ |
| 11 | Default Ergonomics       | 2     | ⚠️ |
| 12 | Appropriate Complexity   | 3     | ✅ |
| 13 | Documentation Quality    | 2     | ⚠️ |
|    | **TOTAL**               | **30/39** | |

## Strengths

1. **Composability**: The descriptor lists are plain mp11 type lists.
   Any algorithm that works on type lists - `mp_for_each`,
   `mp_transform`, `mp_filter` - works on descriptors. This enabled
   16 diverse examples (JSON, Serialization, fmt, hashing, operators,
   RPC dispatch) without the library needing to know about any of
   those domains.

2. **Appropriate Complexity**: Three macros (`BOOST_DESCRIBE_STRUCT`,
   `BOOST_DESCRIBE_CLASS`, `BOOST_DESCRIBE_ENUM`) and three query
   aliases (`describe_members`, `describe_bases`,
   `describe_enumerators`) form the entire core. There are no
   factories, no concept hierarchies, no policy templates. The
   abstraction level precisely matches the problem: annotate types,
   query metadata.

3. **Pit of Success**: Misuse paths are blocked at compile time.
   Querying an undescribed type is a substitution failure.
   `enum_to_string` requires the caller to provide a default,
   preventing null-pointer surprises. The `operators` namespace
   requires an explicit `using` declaration, preventing accidental
   operator hijacking across unrelated types.

## Weaknesses

1. **mp11 as hidden prerequisite**: The primary iteration mechanism
   is `boost::mp11::mp_for_each`, which appears in every example
   but is not part of Describe itself. A user cannot iterate
   descriptors without understanding mp11 type-list mechanics.
   The docs mention this dependency but do not teach it.
   *Suggestion:* Provide a `describe::for_each_member<T, M>(f)`
   convenience function that hides the mp11 machinery for the
   common case, or at minimum add a "Getting Started" section
   that teaches the mp11 iteration pattern in 5 lines.

2. **No inline documentation in headers**: The public headers
   contain zero doc comments. A user reading `enum_to_string.hpp`
   sees a bare template with no description of parameters, return
   value, or behavior. All documentation lives in external AsciiDoc
   files that IDE tooling cannot surface.
   *Suggestion:* Add Javadoc-style `/** */` comments to every
   public function and type alias so that IDE tooltips, code
   completion, and `hover` show useful information.

3. **Modifier bitmask is a plain enum, not an enum class**: The
   `modifiers` enum is unscoped, so `mod_public`, `mod_protected`,
   etc. are injected into `boost::describe` directly. The values
   are raw powers of two with no type safety preventing accidental
   mixing with unrelated integers. The bitmask combination uses
   `static_cast<modifiers>(...)`, which the user must also do for
   custom combinations.
   *Suggestion:* Make `modifiers` a scoped `enum class` with
   overloaded `operator|` and `operator&` for type-safe bitmask
   composition, or provide named constants and a builder.

## Detailed Analysis

### 1. Call-Site Clarity — ⚠️

**Score: 2/3**

The annotation macro call sites are clean and read as declarations:

```cpp
BOOST_DESCRIBE_STRUCT(Y, (X), (m, f))
```

The query call sites, however, expose mp11 machinery:

```cpp
boost::mp11::mp_for_each<boost::describe::describe_members<T,
    boost::describe::mod_public>>([&](auto D) {
    obj[D.name] = boost::json::value_from(t.*D.pointer);
});
```

The user's algorithm ("for each public member, serialize it") is
present but wrapped in template-heavy scaffolding. With namespace
aliases this improves, but the raw call site is not pure algorithm.

**Evidence:** Every example in `example/` uses
`boost::mp11::mp_for_each<boost::describe::describe_members<T, M>>`
as the iteration idiom. The framework (mp11) is visible at every
call site.

---

### 2. Minimal Ceremony — ⚠️

**Score: 2/3**

Describing a struct is two lines:

```cpp
struct Point { int x; int y; };
BOOST_DESCRIBE_STRUCT(Point, (), (x, y))
```

Describing an enum and converting to string is three lines:

```cpp
enum Color { red, green, blue };
BOOST_DESCRIBE_ENUM(Color, red, green, blue)
auto name = boost::describe::enum_to_string(c, "(unknown)");
```

The ceremony is proportional to the task. The one source of
friction is member name duplication: the user must list every
member name in the macro argument, which the compiler cannot
verify matches the actual members. This is inherent to the
pre-reflection C++ era and not a design defect per se, but it
is ceremony.

**Evidence:** `BOOST_DESCRIBE_STRUCT(B, (), (v, m))` in
`example/to_json.cpp` - two tokens of setup per member.

---

### 3. Progressive Disclosure — ✅

**Score: 3/3**

Three clear levels:

- **Beginner**: `BOOST_DEFINE_ENUM_CLASS(E, a, b, c)` plus
  `enum_to_string` / `enum_from_string`. No mp11 needed.
- **Intermediate**: `BOOST_DESCRIBE_STRUCT` plus
  `using boost::describe::operators::operator==;` for automatic
  equality, comparison, and printing. Still no mp11 needed.
- **Expert**: `describe_members<T, M>` with `mp_for_each` to build
  generic JSON serialization, RPC dispatch, Boost.Serialization
  adapters, fmtlib formatters.

Each level is independently useful. The beginner never encounters
type lists. The intermediate user gets operators for free. The
expert gets full reflection primitives.

**Evidence:** `example/enum_to_string.cpp` (beginner),
`example/equality.cpp` (intermediate), `example/json_rpc.cpp`
(expert).

---

### 4. Pit of Success — ✅

**Score: 3/3**

Misuse is caught at compile time. Using `describe_enumerators<E>`
on an undescribed type is a substitution failure, not silent
misbehavior. The `has_describe_*` traits provide a safe test.
`enum_to_string` takes a mandatory default parameter, so the caller
cannot accidentally dereference a null name. The operators live in
a dedicated `operators` namespace and only activate for described
non-union types, preventing accidental ADL hijacking.

The most likely user error - forgetting to list a member in the
macro - produces incorrect reflection rather than UB. This is a
data correctness issue, not a safety issue, and is inherent to
manual annotation.

**Evidence:** `enum_to_string(E e, char const* def)` forces the
caller to handle the unknown-value case. `describe_bases<T, M>`
causes substitution failure for undescribed `T`. `operators` are
gated by `has_describe_bases<T>::value &&
has_describe_members<T>::value && !std::is_union<T>::value`.

---

### 5. Naming and Vocabulary — ⚠️

**Score: 2/3**

Core names are clear and follow Boost conventions:
`describe_members`, `describe_bases`, `describe_enumerators`,
`enum_to_string`, `enum_from_string`. The `BOOST_DESCRIBE_STRUCT`
and `BOOST_DESCRIBE_CLASS` distinction correctly mirrors the
C++ struct/class distinction (all-public vs. mixed access).

Minor issues: `mod_any_member` is not immediately clear - it means
"all kinds of members regardless of static/function classification"
rather than "any access level" (that is `mod_any_access`). The
names `mod_any_member` vs. `mod_any_access` are confusable at
first glance. `BOOST_DESCRIBE_MAKE_NAME` is awkward - it creates a
type from a string literal for compile-time name lookup, but the
name does not suggest this.

**Evidence:** `modifiers.hpp` defines both `mod_any_member = 64`
and `mod_any_access = mod_public | mod_protected | mod_private`.

---

### 6. Composability — ✅

**Score: 3/3**

Descriptors are mp11 type lists. Any mp11 algorithm works:
`mp_for_each`, `mp_transform`, `mp_filter`, `mp_find_if`,
`mp_copy_if`. The library composes with Boost.JSON
(`tag_invoke`), Boost.Serialization (`serialize`), fmtlib
(`formatter`), Boost.Hash (`hash_combine`), and `std::tuple`
without any glue code from Describe itself. Each example in the
repository demonstrates composition with a different ecosystem.

The `operators` namespace composes via `using` declarations,
allowing per-namespace opt-in without polluting other namespaces.

**Evidence:** 16 example programs compose Describe with 6
different libraries/frameworks. `descriptor_by_pointer` and
`descriptor_by_name` compose with the descriptor lists to enable
random-access lookup.

---

### 7. Module Depth — ✅

**Score: 3/3**

The public surface is remarkably small: 3 annotation macros, 3
query aliases, 3 detection traits, 2 enum-string functions, 7
operators, 2 lookup utilities, and 1 modifier enum. Behind this,
the library hides preprocessor metaprogramming (64-element
`PP_FOR_EACH`), ADL-based descriptor injection, base modifier
computation, mp11-based filtering, and inherited/hidden member
resolution.

The `#include` fan-out is modest. Including `<boost/describe.hpp>`
pulls in mp11 algorithm headers and `<type_traits>`, `<iosfwd>`,
`<cstring>`. No heavy standard library headers. The detail/
directory contains 9 headers that never leak into the public
interface.

**Evidence:** `include/boost/describe.hpp` is 22 lines. The entire
public API is ~900 lines across 12 headers. The detail/
implementation is ~580 lines across 9 headers.

---

### 8. Consistency — ✅

**Score: 3/3**

One pattern governs all operations:

- **Annotation**: `BOOST_DESCRIBE_*(Type, ...members...)`
- **Query**: `describe_*<T, M>` returns a type list of descriptors
- **Detection**: `has_describe_*<T>` returns a boolean trait
- **Descriptors**: always have `::name` (char const*) and a
  type-specific payload (`::value` for enums, `::pointer` for
  members, `::type` for bases)

Learn how `describe_enumerators` works, and you can predict
`describe_members` and `describe_bases`. The modifier bitmask
pattern is uniform across bases and members. The naming convention
(`describe_X` for query, `has_describe_X` for detection,
`BOOST_DESCRIBE_X` for annotation) is mechanical and predictable.

**Evidence:** All three query aliases follow the pattern
`describe_<thing><T[, M]>`. All three traits follow
`has_describe_<thing><T>`.

---

### 9. Error Reporting — ⚠️

**Score: 2/3**

Compile-time errors are SFINAE-friendly: using `describe_members`
on an undescribed type is a substitution failure, which produces
a reasonably clear error in the context of a `static_assert` or
an `enable_if`. The `has_describe_*` traits allow users to write
their own `static_assert` messages.

Runtime errors are handled well for enums: `enum_to_string` returns
a caller-supplied default, and `enum_from_string` returns `bool`.
No exceptions, no silent corruption.

The gap is in compile-time diagnostics when the user makes a
mistake in the macro invocation (e.g., misspelling a member name).
The preprocessor error messages from `BOOST_DESCRIBE_PP_FOR_EACH`
can be opaque. There is no `static_assert` inside the macros to
produce a human-readable message.

**Evidence:** `enum_from_string` returns `bool`; `enum_to_string`
returns `def` parameter on miss. `describe_members` on undescribed
type: substitution failure.

---

### 10. Type Safety — ⚠️

**Score: 2/3**

The descriptor types are distinct: enumerator descriptors have
`value` and `name`, member descriptors have `pointer`, `name`, and
`modifiers`, base descriptors have `type` and `modifiers`. You
cannot confuse them at compile time.

The weakness is the `modifiers` enum. It is an unscoped enum with
integer values. Combining modifiers requires bitwise OR on raw
integers, and the result must be `static_cast`ed back to
`modifiers`. There is no compile-time enforcement that the bitmask
combinations are valid (e.g., passing `mod_virtual` as a member
filter is meaningless but not caught). The `M` parameter in
`describe_members<T, M>` is `unsigned`, not `modifiers`, so any
integer passes.

**Evidence:** `modifiers.hpp` defines `enum modifiers` (unscoped).
`describe_members<T, M>` takes `unsigned M`. The reference doc
shows `template<class T, unsigned M> using describe_bases`.

---

### 11. Default Ergonomics — ⚠️

**Score: 2/3**

`BOOST_DEFINE_ENUM` and `BOOST_DEFINE_ENUM_CLASS` combine
definition and description in one macro - good defaults.
`enum_to_string` and `enum_from_string` work with zero
configuration.

The operators require an explicit `using` declaration per operator
per namespace. This is the correct design (prevents accidental
ADL hijacking), but it means the user must write 6 `using`
declarations for full operator coverage. A `using namespace
boost::describe::operators;` shortcut exists but pulls all
operators, which may be more than desired.

There is no default for the modifier bitmask in `describe_members`
or `describe_bases`. The user must always specify `mod_public` or
`mod_any_access`. A defaulted template parameter for the common
case (`mod_public`) would reduce friction.

**Evidence:** `example/equality.cpp` uses
`describe_members<T, mod_any_access>` - the modifier is always
explicit. Every example specifies the full modifier bitmask.

---

### 12. Appropriate Complexity — ✅

**Score: 3/3**

The library introduces exactly the types needed: descriptor
structs with `constexpr` members, a modifier bitmask, and type
aliases that return type lists. There are no factories, no
builders, no policy templates, no concept hierarchies. The
preprocessor machinery in `detail/` is complex but completely
hidden. The user-facing surface has fewer moving parts than
`std::tuple`.

The decision to expose mp11 type lists rather than inventing a
custom iteration mechanism is a deliberate design choice that
reduces the library's own complexity while leveraging existing
infrastructure.

**Evidence:** The entire public API fits in 12 headers totaling
~900 lines. The umbrella header is 22 lines. There is one enum,
zero classes, zero concepts in the public API.

---

### 13. Documentation Quality — ⚠️

**Score: 2/3**

The AsciiDoc documentation is well-structured: overview, enum
usage, class usage, examples, reference. The reference section
specifies every macro expansion, every descriptor shape, and every
precondition. The 16 examples are diverse and compilable,
covering JSON, Serialization, fmt, hashing, operators, and RPC.

The gaps: (1) No inline doc comments in headers - IDE tooling
cannot surface documentation. (2) No "Getting Started" tutorial
that walks a newcomer through the first use case step by step.
(3) The examples in the doc reference are code-only with no
narrative explanation of what each line does. (4) The mp11
dependency is mentioned but not taught - a user unfamiliar with
mp11 must leave the Describe docs to learn the iteration pattern.

**Evidence:** All 12 public headers contain zero `/** */` or
`///` doc comments. The reference doc at `doc/describe/reference.adoc`
covers every API element but opens with macro signatures, not
use cases.

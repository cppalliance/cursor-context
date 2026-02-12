# libassert - API Design Quality Evaluation

## Executive Summary

libassert is a C++ assertion library that replaces `<cassert>` with rich diagnostic output including automatic expression decomposition, stack traces, syntax highlighting, and extra diagnostic values. The macro-based interface (`ASSERT(expr)`, `DEBUG_ASSERT(expr)`, `PANIC(...)`) is remarkably close to the ideal call site for assertions - one macro, one expression, zero ceremony for the common case. The library's strongest qualities are its call-site clarity, progressive disclosure from simple assertions to custom failure handlers, and the depth of diagnostic machinery hidden behind a tiny surface. Its primary weaknesses are the heavy transitive include fan-out from the main header, the global mutable state used for configuration, and the macro-dominated public surface that limits composability and type safety at the API boundary.

## Verdict

✅ PASS: 30/39 — An exceptionally well-designed assertion library whose macro interface achieves near-ideal call-site clarity; held back by heavy include fan-out, global configuration state, and limited composability outside the assertion macro context.

## Gates

| Gate                | Result | Finding                                                                                                          |
|---------------------|--------|------------------------------------------------------------------------------------------------------------------|
| G0. Documentation   | ✅     | README provides thorough documentation organized by task with compilable examples for every macro and configuration point |
| G1. No UB Traps     | ✅     | The obvious way to use every macro is safe; `ASSUME` UB in release is explicit and opt-in; `ASSERT(1 = 2)` compiling is documented |
| G2. Ownership       | ✅     | All types are value types or macro-local temporaries; `assertion_info` owns its trace via `unique_ptr`; no ambiguous lifetime questions |

**Gate Result:** ✅ ALL GATES PASS

### G0 Notes

The README serves as the primary documentation and is comprehensive. It opens with a 30-second overview containing compilable examples, then progressively covers the full API surface: macro variants (conditional and unconditional), expression decomposition behavior, extra diagnostics, stack traces, custom failure handlers, stringification customization, test framework integration, color configuration, literal formatting, path mode, and platform logistics. Every public function and type in the header is documented in the README with pseudo-declarations, behavioral descriptions, and code examples. The `assertion_info` struct is documented with a labeled anatomy diagram showing what each field maps to in the output. The headers themselves contain sparse inline comments (a reasonable choice for a library distributed as a shared library), but the README fully covers the API contract.

### G1 Notes

The natural usage pattern - `ASSERT(condition)` or `ASSERT(condition, "message", extra_diag)` - has no UB traps. The library's most dangerous construct, `ASSUME`, which marks the failure path as `__builtin_unreachable()` in release, is explicitly named and documented as distinct from `ASSERT`. The documentation calls out that `ASSERT(1 = 2)` compiles due to expression decomposition (the `=` is captured as an assignment operator) and notes the bit field workaround. The `_VAL` variants' return value rules are explicitly specified. There is no silent UB on any common usage path. The one subtle edge - `DEBUG_ASSERT_VAL` must still be evaluated in release even though the check is disabled - is documented.

### G2 Notes

The library has no resource-managing types that the user must construct. `assertion_info` is constructed internally by the macro machinery and passed to the failure handler by const reference. It owns its stack trace via `unique_ptr<trace_holder>` with a custom deleter, and owns its `binary_diagnostics_descriptor` and `extra_diagnostic` vector by value. The user never allocates, never deletes, never manages lifetimes. The `color_scheme` struct requires its `string_view` members to have static storage duration, which is documented in a comment directly on the struct. The `set_failure_handler` function takes a raw function pointer (`void(*)(const assertion_info&)`) which has no ownership implications.

## Design Scores

| #  | Item                    | Score |      |
|----|-------------------------|-------|------|
| 1  | Call-Site Clarity        | 3     | ✅   |
| 2  | Minimal Ceremony        | 3     | ✅   |
| 3  | Progressive Disclosure   | 3     | ✅   |
| 4  | Pit of Success           | 2     | ⚠️   |
| 5  | Naming and Vocabulary    | 3     | ✅   |
| 6  | Composability            | 1     | 🟡   |
| 7  | Module Depth             | 2     | ⚠️   |
| 8  | Consistency              | 3     | ✅   |
| 9  | Error Reporting          | 3     | ✅   |
| 10 | Type Safety              | 1     | 🟡   |
| 11 | Default Ergonomics       | 3     | ✅   |
| 12 | Appropriate Complexity   | 2     | ⚠️   |
| 13 | Documentation Quality    | 3     | ✅   |
|    | **TOTAL**               | **30/39** |  |

## Strengths

1. **Call-site clarity is near-ideal**: `ASSERT(vec.size() > min_items(), "not enough items", vec)` reads as pure intent. The expression, the message, and the diagnostic values are all visible in a single line with no framework scaffolding. The automatic expression decomposition eliminates the need for `ASSERT_EQ`, `ASSERT_LT`, and other typed assertion macros that plague competing libraries. The user writes their condition as they think it; the library does the rest.

2. **Progressive disclosure from one-liner to full customization**: A beginner writes `ASSERT(x > 0)`. An intermediate user adds a message and extra diagnostics: `ASSERT(x > 0, "invalid count", x, y)`. An expert installs a custom failure handler, configures color schemes, enables safe comparisons, and integrates with Catch2 or GoogleTest. Each layer is independently discoverable without requiring understanding of the next, and the simple surface requires zero configuration.

3. **Extraordinary diagnostic depth behind a tiny interface**: The `ASSERT` macro hides expression decomposition, C++ syntax parsing, value stringification (with automatic container/tuple/optional/smart-pointer support), stack trace generation via cpptrace, syntax highlighting, literal format inference, diff highlighting, and path disambiguation. The user's interface is a single macro. The implementation depth-to-surface ratio is exceptional.

## Weaknesses

1. **Heavy transitive include fan-out from the main header**: `#include <libassert/assert.hpp>` transitively pulls in `<string>`, `<string_view>`, `<vector>`, `<optional>`, `<cstdint>`, `<functional>`, `<type_traits>`, `<utility>`, `<cpptrace/cpptrace.hpp>`, and the library's own `platform.hpp`, `utilities.hpp`, `stringification.hpp`, and `expression-decomposition.hpp`. The stringification header further pulls in `<sstream>`, `<tuple>`, and conditionally `<format>`, `<fmt/format.h>`, and `<magic_enum.hpp>`. A user who only wants `ASSERT(x > 0)` pays for the full stringification and stack trace machinery in every translation unit. — *Suggestion:* Split into a lightweight `assert-core.hpp` that provides the macros with minimal includes, and a separate `assert-full.hpp` that adds the stringification and diagnostic machinery. Use a forward-declared `assertion_info` in the core header and link the diagnostic generation into the shared library.

2. **Global mutable state for configuration**: `set_color_scheme`, `set_separator`, `set_literal_format_mode`, `set_path_mode`, `set_diff_highlighting`, and `set_failure_handler` all mutate global state with no thread-safety guarantees (the separator is explicitly documented as not thread-safe; others are unspecified). A library that uses libassert cannot safely configure it without risking conflicts with the application or other libraries. — *Suggestion:* Provide a `config` object that can be passed to the failure handler or associated with a thread, allowing per-context configuration without global mutation. Alternatively, document the thread-safety properties of each setter and provide a `configure_once` idiom.

3. **Macros as the entire public API limit composability and type safety**: Because the assertion interface is entirely macro-based, there is no function or type that can be composed with standard algorithms, passed as a callback, stored in a data structure, or subjected to overload resolution. The `expression_decomposer` template, while clever, leaks into the public headers as a detail type that users cannot interact with meaningfully. The macro boundary also means that compile-time errors from misuse (e.g., a non-boolean expression) surface as template errors deep in `expression_decomposer`, not as clear messages at the call site. — *Suggestion:* This is inherent to the expression decomposition approach and is an acceptable tradeoff for the domain. Consider providing a non-macro `libassert::check(bool, message)` function for contexts where expression decomposition is not needed and composability matters.

## Detailed Analysis

### 1. Call-Site Clarity — ✅

**Score: 3/3**

The assertion call site is as close to pure intent as a C++ assertion library can achieve. `ASSERT(map.contains("foo"), "expected key not found", map)` reads as a single English sentence: assert the map contains "foo", and if it fails, tell me about the map. The automatic expression decomposition means the user never writes `ASSERT_EQ(a, b)` or `ASSERT_GT(x, 0)` - they write the condition as they think it. The `_VAL` variants (`ASSERT_VAL(fopen(path, "r") != nullptr)`) integrate assertions into expressions without breaking the reading flow. The framework machinery (expression decomposer, stringification, trace generation) is completely invisible at the call site.

**Evidence:** `ASSERT(vec.size() > min_items(), "vector doesn't have enough items", vec);` and `float f = *ASSERT_VAL(get_param());` from the README.

### 2. Minimal Ceremony — ✅

**Score: 3/3**

The simplest use case is a single token beyond the expression itself: `ASSERT(condition)`. There is no object to construct, no context to initialize, no executor to provide. The CMake integration is a single `target_link_libraries` call. Adding a message is one extra string argument. Adding diagnostics is one extra value per diagnostic. The `PANIC` and `UNREACHABLE` macros take zero required arguments. The library requires no initialization, no cleanup, and no global setup for basic use.

**Evidence:** `DEBUG_ASSERT(map.contains("foo"), "expected key not found", map);` - three tokens of ceremony (the macro name, the message, and the diagnostic), all of which directly express intent.

### 3. Progressive Disclosure — ✅

**Score: 3/3**

Three clear levels: (1) Beginner: `ASSERT(x > 0)` - one macro, one expression, automatic diagnostics. (2) Intermediate: `ASSERT(x > 0, "must be positive", x, y)` - add messages and extra diagnostics, enable safe comparisons with `-DLIBASSERT_SAFE_COMPARISONS`, use `ASSERT_VAL` for inline assertions. (3) Expert: install custom failure handlers with `set_failure_handler`, configure color schemes, integrate with Catch2/GoogleTest via dedicated headers, enable diff highlighting, customize stringification via `libassert::stringifier<T>` specialization. Each level is accessible without understanding the next. The beginner never sees `assertion_info`, `color_scheme`, or `expression_decomposer`.

**Evidence:** The README itself is structured as progressive disclosure - it opens with the 30-second overview, then philosophy, then features, then methodology, then in-depth documentation.

### 4. Pit of Success — ⚠️

**Score: 2/3**

The common cases are safe and natural. `ASSERT(condition)` does the right thing. `DEBUG_ASSERT` is clearly named to indicate debug-only checking. `PANIC` and `UNREACHABLE` are `[[noreturn]]`, preventing control flow past them. However, there are edge cases that can surprise: `ASSERT(1 = 2)` compiles due to expression decomposition capturing the assignment operator - the library documents this but does not prevent it. Template arguments in assertion expressions require extra parentheses (`ASSERT((foo<a, b>()) == c)`) due to macro comma handling. Bit field comparisons require workarounds (`+s.bit == 1` instead of `s.bit == 1`). `ASSUME` silently becomes UB-on-failure in release, which is the correct behavior for the feature but requires the user to understand the distinction between `ASSERT` and `ASSUME`.

**Evidence:** The README notes section: "Because of expression decomposition, `ASSERT(1 = 2);` compiles." and "expressions with template arguments will need to be templatized."

### 5. Naming and Vocabulary — ✅

**Score: 3/3**

The macro names form a clear, self-documenting taxonomy. `DEBUG_ASSERT` asserts in debug only. `ASSERT` asserts always. `ASSUME` asserts in debug and hints in release. `PANIC` is unconditional failure. `UNREACHABLE` marks unreachable code. `_VAL` suffixes return a value. The naming convention is immediately predictable: once you know `ASSERT`, you can predict `ASSERT_VAL`, `DEBUG_ASSERT`, `DEBUG_ASSERT_VAL`, `ASSUME`, `ASSUME_VAL`. The configuration functions use clear verb-noun naming: `set_failure_handler`, `set_color_scheme`, `set_separator`, `set_path_mode`. The `assertion_info` struct uses self-describing field names: `expression_string`, `binary_diagnostics`, `extra_diagnostics`, `file_name`, `line`, `function`. No jargon, no abbreviations, no framework-specific vocabulary.

**Evidence:** The assertion type enum: `debug_assertion`, `assertion`, `assumption`, `panic`, `unreachable` - each name is the plain English word for what it represents.

### 6. Composability — 🟡

**Score: 1/3**

Macros cannot compose with standard algorithms, cannot be passed as function arguments, and cannot be stored or parameterized. The library is a closed system: assertion macros expand to complex statement expressions that cannot be broken apart or recombined. The `stringifier<T>` customization point and `set_failure_handler` are the only composition seams. The GoogleTest and Catch2 integrations require dedicated headers that redefine `ASSERT` and install a failure handler in a global `pre_main` variable - this is necessary but fragile (the headers must be included before any test code, and they conflict with each other). The `assertion_info` struct is composable once you have it in a failure handler, with clean accessor methods (`header()`, `tagline()`, `statement()`, `to_string()`), but the user cannot create one directly or compose with the assertion evaluation itself.

**Evidence:** The GoogleTest integration header (`assert-gtest-macros.hpp`) redefines `ASSERT` as a `do { try { LIBASSERT_ASSERT(...); SUCCEED(); } catch(...) { FAIL() << e.what(); } } while(0)` wrapper - the only way to integrate is macro wrapping, not function composition.

### 7. Module Depth — ⚠️

**Score: 2/3**

The user-facing interface is tiny: 8 assertion macros, a handful of configuration setters, and a `stringifier` customization point. Behind them lies a vast implementation: expression decomposition, C++ syntax parsing, value stringification for dozens of types, stack trace generation, syntax highlighting, literal format inference, path disambiguation, and debugger detection. The depth-to-surface ratio is excellent at the macro level. However, `#include <libassert/assert.hpp>` pulls in 5 library headers plus standard headers (`<string>`, `<vector>`, `<optional>`, `<sstream>`, `<tuple>`, `<type_traits>`, `<utility>`, `<cstdint>`, `<functional>`), exposing the full `expression_decomposer` template and all of `stringification.hpp` to the user's translation unit. The `detail` namespace hides most of this, but the compile-time cost and symbol pollution are real. The library does offer a module partition via `assert-macros.hpp` for C++20 module builds, which shows awareness of the problem.

**Evidence:** `assert.hpp` includes `platform.hpp`, `utilities.hpp`, `stringification.hpp`, `expression-decomposition.hpp`, plus `cpptrace/cpptrace.hpp` or `cpptrace/basic.hpp`. The user gets all of this from one `#include`.

### 8. Consistency — ✅

**Score: 3/3**

Every conditional assertion macro follows an identical pattern: `MACRO(expression [, message] [, extra_diagnostics...])`. Every unconditional assertion follows: `MACRO([message] [, extra_diagnostics...])`. Every `_VAL` variant has the same argument structure as its non-`_VAL` counterpart and returns the value. The prefixed (`LIBASSERT_ASSERT`) and unprefixed (`ASSERT`) versions are mechanically related. The `assertion_info` formatting methods all follow the same pattern: `method(int width = 0, const color_scheme& scheme = get_color_scheme()) const` returning `std::string`. The configuration setters all follow `set_X(X_type)` / `get_X()` naming. The three predefined color schemes are all `static const` members of `color_scheme`. The consistency is thorough enough that a user who learns `ASSERT` can predict the entire API surface.

**Evidence:** `assertion_info::header()`, `::tagline()`, `::statement()`, `::print_binary_diagnostics()`, `::print_extra_diagnostics()`, `::print_stacktrace()`, `::to_string()` - all take the same `(int width, const color_scheme&)` parameters with the same defaults.

### 9. Error Reporting — ✅

**Score: 3/3**

This is the library's raison d'etre, and it excels. Assertion failures produce: the assertion type and location, the function signature, the optional message, the expression string, the decomposed left/right values of binary expressions, extra diagnostic values with their expression strings, and a full stack trace with syntax highlighting and path disambiguation. The `errno` special handling automatically calls `strerror`. Smart literal formatting shows hex/binary when the expression uses hex/binary. Diff highlighting (opt-in) shows string differences. Redundant diagnostics (`0 => 0`) are suppressed. The failure output is structured with labeled sections (Where, Extra diagnostics, Stack trace) that can be parsed both by humans and by custom failure handlers via the `assertion_info` accessors.

**Evidence:** The anatomy diagram in the README shows each piece of output mapped to its `assertion_info` field, with seven distinct accessor methods for extracting individual pieces of the diagnostic output.

### 10. Type Safety — 🟡

**Score: 1/3**

The macro interface provides minimal type safety. Any expression that is convertible to `bool` is accepted by `ASSERT` - there is no way to express "this assertion expects a comparison" vs. "this assertion expects a boolean." The message parameter is detected by type (`is_string_type`) rather than by position - if the first extra argument happens to be a string, it becomes the message, which can be surprising. The workaround for this (pass an empty string first: `ASSERT(foo, "", str)`) is documented but unergonomic. The `ASSUME` macro silently becomes `__builtin_unreachable()` in release, distinguished from `ASSERT` only by name - there is no type-level distinction between "check this" and "optimize on this." The `color_scheme` struct is 16 `string_view` members with no validation that they contain valid ANSI escape sequences. The `literal_format` enum uses bitwise flags with an `operator|` overload but no type wrapper to prevent mixing with other unsigned values.

**Evidence:** `struct color_scheme { std::string_view string; std::string_view escape; ... }` - 16 unvalidated `string_view` fields, any of which can be set to any string including invalid escape sequences.

### 11. Default Ergonomics — ✅

**Score: 3/3**

The library works perfectly out of the box with zero configuration. The default color scheme (`ansi_rgb`) produces colorful output on terminals that support it. The default literal format mode (`infer`) automatically shows hex/binary when the expression uses hex/binary. The default path mode (`disambiguated`) shows the minimum path needed to identify the file. The default separator (`=>`) is the conventional choice. The default failure handler prints to stderr and aborts - exactly what most users expect from a failed assertion. The debugger breakpoint feature is opt-in (`-DLIBASSERT_BREAK_ON_FAIL`), so it does not surprise users who are not using a debugger. Safe comparisons are opt-in (`-DLIBASSERT_SAFE_COMPARISONS`), defaulting to the standard C++ comparison semantics users expect. Every configuration point has a sensible default that matches the 90% use case.

**Evidence:** The entire 30-second overview in the README uses the library with zero configuration calls - no `set_*` function is called, and the output is fully featured.

### 12. Appropriate Complexity — ⚠️

**Score: 2/3**

The expression decomposition machinery is inherently complex - the `expression_decomposer` template with its three specializations (empty, unary, binary) and 25+ operator overloads is substantial engineering. This complexity is justified by the call-site simplicity it enables: the user writes `ASSERT(a == b)` and gets both values displayed, which is the library's core value proposition. The stringification system similarly justifies its complexity by handling dozens of standard types automatically. However, some complexity seems disproportionate: the `LIBASSERT_MAP` macro machinery (40+ preprocessor macros for mapping over variadic arguments, with separate implementations for MSVC's non-conformant preprocessor) is heavy machinery for what amounts to stringifying a list of argument expressions. The `LIBASSERT_STATIC_DATA` macro with its lambda-in-constexpr workaround for pre-C++23 is clever but opaque. The `assert_value_wrapper` and `get_expression_return_value` templates add indirection to support the `_VAL` variants' return semantics.

**Evidence:** `assert-macros.hpp` contains separate `LIBASSERT_MAP` implementations for conformant and non-conformant preprocessors, totaling 50+ lines of preprocessor metaprogramming for argument stringification.

### 13. Documentation Quality — ✅

**Score: 3/3**

The README opens with a 30-second overview containing two compilable examples - the user sees working code before anything else. The documentation then follows a natural progression: philosophy (why the library exists), features (what it can do, with screenshots), methodology (the assertion types and their semantics), then in-depth reference (every type, function, and macro). The assertion macro pseudo-declarations are clear and complete. The `assertion_info` anatomy section maps output to struct fields with a concrete example. The CMake integration section is copy-paste ready. The Catch2 and GoogleTest integration sections each have a minimal complete example. The FAQ, comparison with other languages, and platform logistics sections address practical concerns. The documentation is organized by what the user wants to do (use assertions, customize output, integrate with test frameworks) rather than by what the library contains (headers, classes, namespaces). The only gap is that the headers themselves contain almost no inline documentation, but the README covers the entire API surface.

**Evidence:** The 30-second overview opens with `#include <libassert/assert.hpp>` followed by two working examples, then a table of all assertion types with one-line descriptions - the user is productive in 30 seconds.

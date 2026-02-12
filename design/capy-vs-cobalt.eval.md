# Comparative Design Evaluation: Boost.Capy vs Boost.Cobalt

Evaluated using the C++ API Design Quality Evaluation Framework (`eval-design.md`). Both libraries provide coroutine-based async primitives for Boost. This report evaluates their **design quality** - ergonomics, usability, elegance - not implementation correctness or performance.

---

## Executive Summary

Boost.Capy demonstrates strong API design discipline across every dimension: exhaustive contract-focused documentation on all public surfaces, minimal template parameter exposure, rigorous encapsulation of internals behind `detail` namespaces, and a consistent pattern language that makes the library predictable after learning one operation. Boost.Cobalt has strong conceptual design - the right abstractions at the right granularity - but the public headers leak implementation details extensively and mix modern C++20 idioms with legacy SFINAE patterns inconsistently.

## Verdicts

**Capy:** ✅ PASS: 33/39 - Exemplary design with minor overload proliferation

**Cobalt:** ⚠️ CONCERNS: 21/39 - Strong concepts undermined by poor encapsulation and inconsistency

---

## Gates

| Gate | Capy | Cobalt | Notes |
|------|------|--------|-------|
| G0. Documentation | ✅ | ✅ | Capy has comprehensive Javadoc on every public type/function with examples, contracts, and cross-references. Cobalt has thorough external AsciiDoc documentation covering task/promise/generator/channel semantics, composition primitives, and error handling modes. |
| G1. No UB Traps | ✅ | ❌ | Capy's natural usage patterns are safe; `[[nodiscard]]` on tasks and results prevents silent discard. Cobalt's `promise::get()` and `generator::get()` have UB via `BOOST_ASSERT(ready())` when called on a not-ready value - release builds silently invoke undefined behavior on the most natural "check and get" pattern. |
| G2. Ownership | ✅ | ✅ | Both libraries use move-only types for tasks and promises. Capy's `any_stream` clearly distinguishes owning (by-value) from non-owning (by-pointer) construction. Cobalt's I/O types follow Asio ownership conventions. |

**Capy Gate Result:** ✅ ALL GATES PASS

**Cobalt Gate Result:** ❌ GATE FAILURE - G1 (UB Traps)

### Cobalt Gate Failure Details

**G1 - UB Traps:** `promise::get()` and `generator::get()` call `BOOST_ASSERT(ready())`. In release builds, this assertion is elided and the function proceeds into undefined behavior. A user who writes the natural pattern `if (p.ready()) { auto val = p.get(); }` is safe, but a user who calls `p.get()` without the check - the most obvious way to retrieve a value - gets silent UB. This should either throw an exception or return `std::optional`/`result`.

*Despite gate failures, the full evaluation is provided below for completeness.*

---

## Design Scores

| # | Item | Capy | | Cobalt | | Winner |
|---|------|------|-|--------|-|--------|
| 1 | Call-Site Clarity | 3 | ✅ | 3 | ✅ | Tie |
| 2 | Minimal Ceremony | 3 | ✅ | 2 | ⚠️ | Capy |
| 3 | Progressive Disclosure | 3 | ✅ | 2 | ⚠️ | Capy |
| 4 | Pit of Success | 3 | ✅ | 1 | 🟡 | Capy |
| 5 | Naming and Vocabulary | 3 | ✅ | 2 | ⚠️ | Capy |
| 6 | Composability | 3 | ✅ | 2 | ⚠️ | Capy |
| 7 | Module Depth | 2 | ⚠️ | 1 | 🟡 | Capy |
| 8 | Consistency | 3 | ✅ | 1 | 🟡 | Capy |
| 9 | Error Reporting | 3 | ✅ | 1 | 🟡 | Capy |
| 10 | Type Safety | 2 | ⚠️ | 2 | ⚠️ | Tie |
| 11 | Default Ergonomics | 3 | ✅ | 2 | ⚠️ | Capy |
| 12 | Appropriate Complexity | 2 | ⚠️ | 1 | 🟡 | Capy |
| 13 | Documentation Quality | 3 | ✅ | 1 | 🟡 | Capy |
| | **TOTAL** | **33/39** | | **21/39** | | |

---

## Strengths

### Capy

1. **Documentation as first-class artifact**: Every public type, function, and concept has contract-focused documentation with parameters, return values, preconditions, thread safety, examples, and cross-references. The `ReadStream` and `WriteStream` concepts document both syntactic and semantic requirements - a rarity in C++ libraries.

2. **Invisible framework**: The `io_env` propagation, TLS frame allocator, and dispatch trampolines are entirely hidden. A user writes `auto [ec, n] = co_await stream.read_some(buf)` and never encounters execution machinery. The two-argument `await_suspend(handle, env)` protocol is a significant innovation that is completely invisible at call sites.

3. **Error handling done right**: The two-tier `error`/`cond` system follows `std::error_code`/`std::error_condition` best practice. `[[nodiscard]]` on `io_result` prevents ignored errors. Structured bindings produce clean `auto [ec, n]` decomposition. Users compare against portable conditions (`cond::eof`), never concrete codes.

### Cobalt

1. **Conceptual clarity of primitives**: The `task`/`promise`/`generator`/`channel` taxonomy maps cleanly to async programming concepts. `task` is lazy, `promise` is eager, `generator` is pull-based, `channel` is a Go-style communication primitive. The vocabulary is immediately recognizable to anyone with async experience.

2. **`co_main` entry point**: Replacing `main()` with a coroutine eliminates all boilerplate for setting up an `io_context`, signal handling, and event loop. The user writes their async algorithm directly. This is the lowest-ceremony entry point of any C++ async library.

3. **Three-mode error handling**: The `as_result`/`as_tuple`/throw pattern gives users a choice of error delivery without changing the operation. `co_await socket.read_some(buf)` throws on error; `co_await as_tuple(socket.read_some(buf))` returns `tuple<error_code, size_t>`. This composable approach is elegant.

---

## Weaknesses

### Capy

1. **Overload proliferation in launch functions**: `run()` has 12 overloads and `run_async()` has 18, covering every combination of executor, stop token, allocator, and completion handler. While systematic and type-safe, this creates a large API surface at the most critical entry point. - *Suggestion:* Consider a builder or options struct to collapse combinations, e.g. `run_async(ex).with_allocator(mr).with_stop_token(st)(task())`.

2. **Two-call launch syntax requires learning**: The `run_async(ex)(my_task())` pattern is technically justified (TLS allocator must be set before task construction) but is unprecedented in C++ and requires explanation. - *Suggestion:* A prominently placed "why two calls?" callout in the quick-start documentation would preempt confusion.

3. **`io_result` field names are generic**: The fields `t1`, `t2`, `t3` on `io_result<T1,T2,T3>` carry no semantic meaning. Structured bindings (`auto [ec, n]`) mask this, but direct field access reads as `result.t1` rather than `result.bytes_transferred`. - *Suggestion:* For the common 1-arg specialization, consider a named alias like `value` alongside `t1`.

### Cobalt

1. **Massive internals leakage**: Nearly every public header exposes `detail::*` types. Return types of `join`, `gather`, `race`, and `with` are opaque `detail::*_impl` types. `channel.hpp` has ~100 lines of `read_op`/`write_op` implementation inline. `this_coro.hpp` mixes user-facing `co_await this_coro::executor` with raw coroutine allocation internals (`allocate_coroutine`, `deallocate_coroutine`, pointer arithmetic). `noop<T>` constructors are public on `task`, `promise`, and `generator`. Compiler errors will reference these internals. - *Suggestion:* Move `detail::*` types out of public headers entirely; use type-erased wrappers or deduced return types that name public concepts.

2. **Error messages contain typos and lack polish**: Error enum values include grammatical errors ("raceed", "completed unexpected", "left_raceed"). `promise.hpp` contains a typo ("promimse"). These are minor individually but collectively signal a lack of polish in the user-facing surface. The external AsciiDoc docs are thorough but the error strings that appear in runtime diagnostics are the documentation users see most often when debugging. - *Suggestion:* Audit all error enum values and string literals for typos. Use a spell checker on string constants.

3. **UB in `get()` on promise/generator**: `promise::get()` and `generator::get()` use `BOOST_ASSERT(ready())`, which in release builds becomes UB on misuse. The "obvious" path of calling `get()` without checking `ready()` is a silent trap. - *Suggestion:* Throw `std::logic_error` or `boost::cobalt::error::wait_not_ready` when `!ready()`, or return `std::optional<T>`.

---

## Detailed Comparative Analysis

### 1. Call-Site Clarity — Tie (3 vs 3)

Both libraries achieve the ideal: call sites read as pure algorithm.

**Capy:**
```cpp
auto [ec, n] = co_await stream.read_some(buf);
if (ec == cond::eof) break;
```

**Cobalt:**
```cpp
auto n = co_await stream.read_some(buffer);
// or with error codes:
auto [ec, n] = co_await as_tuple(stream.read_some(buffer));
```

Both express the user's algorithm without framework noise. Cobalt's `as_tuple` wrapper adds one token for the error-code path; Capy's `io_result` structured binding is slightly more direct. In the throwing path, Cobalt is marginally cleaner (no `ec` variable at all). Both earn full marks.

---

### 2. Minimal Ceremony — Capy 3, Cobalt 2

**Capy** hello-world:
```cpp
task<> say_hello() {
    std::cout << "Hello from Capy!\n";
    co_return;
}
int main() {
    thread_pool pool;
    run_async(pool.get_executor())(say_hello());
}
```

**Cobalt** hello-world:
```cpp
cobalt::main co_main(int argc, char* argv[]) {
    co_return 0;
}
```

Cobalt's `co_main` is the lowest ceremony entry point possible - literally replacing `main` with a coroutine. However, once past the entry point, Cobalt requires `use_op` tokens or `cobalt::io::*` wrappers to bridge Asio operations, adding ceremony at every I/O call site. Capy's two-call launch syntax adds one token of ceremony at the entry point but then I/O operations are uniformly clean throughout. Net: Capy's ceremony is lower across a complete program.

---

### 3. Progressive Disclosure — Capy 3, Cobalt 2

**Capy** layers clearly:
- Beginner: `task<>`, `co_await`, `run_async(ex)(task())`
- Intermediate: `when_all`, `when_any`, `io_task<>`, concepts
- Expert: `any_stream`, `executor_ref`, custom `IoAwaitable`

Each level is accessible without understanding the next. The documentation is structured in numbered sections that progress from foundations through advanced topics.

**Cobalt** has a reasonable layering:
- Beginner: `co_main`, `task`, basic I/O
- Intermediate: `promise`, `generator`, `race`/`join`
- Expert: `op`, `channel`, `with`, custom awaitables

However, the external documentation is organized as flat reference rather than layered tutorial, so intermediate users must search across multiple pages. `gather` vs `join` semantics require reading both pages and comparing. `race`'s URBG parameter and `Ct` template NTTP are advanced features buried in the reference without a progressive introduction.

---

### 4. Pit of Success — Capy 3, Cobalt 1

**Capy** makes correct usage the path of least resistance:
- `[[nodiscard]]` on `task`, `io_result`, `when_all`, `when_any`
- `io_result` forces error acknowledgment via structured bindings
- `error`/`cond` separation prevents comparing against unstable codes
- `run_async` wrapper consumed via rvalue ref-qualifier (cannot reuse)

**Cobalt** has several traps:
- `promise::get()` is UB when `!ready()` (Norman door)
- `generator::get()` same issue
- `std::vector<int> a(4)` / `std::vector<int> b{4}` style ambiguity in `operator bool()` semantics (means "can be awaited" not "is active")
- `noop<T>` constructor on `task`/`promise` is public and undocumented
- Error messages have typos ("raceed", "completed unexpected")

---

### 5. Naming and Vocabulary — Capy 3, Cobalt 2

**Capy** names are self-documenting:
- `read_some`, `write_some` - partial I/O, clearly named
- `ReadStream`, `WriteStream`, `IoAwaitable` - PascalCase concepts
- `cond::eof`, `cond::canceled` - portable error conditions
- `when_all`, `when_any` - standard naming for concurrent composition
- No `async_` prefix (everything is async, prefix is redundant)

**Cobalt** names are mostly good:
- `task`, `promise`, `generator`, `channel` - clear vocabulary
- `race`, `join`, `gather` - descriptive composition names
- `left_race` - clever for deterministic variant
- But: `use_op`, `use_task` are Asio completion-token jargon not self-evident to non-Asio users
- `noop<T>` leaks as a public name with no explanation
- `co_main` overloads global namespace (necessary but unusual)

---

### 6. Composability — Capy 3, Cobalt 2

**Capy:**
- `when_all` and `when_any` accept any `IoAwaitable`
- Range-based overloads for homogeneous collections
- Buffer sequences compose via concepts (`ConstBufferSequence`, `MutableBufferSequence`)
- Type erasure (`any_stream`) enables generic algorithms
- `executor_ref` composes with any executor type

**Cobalt:**
- `race`, `join`, `gather` accept any `awaitable` - good
- Both variadic and range overloads provided
- `as_result`/`as_tuple` wrappers compose with any awaitable
- `channel_reader` makes channels directly `co_await`-able
- But: return types are `detail::*_impl`, making further composition opaque. A user cannot easily determine what type `join(a, b)` returns without reading `detail/join.hpp`.

---

### 7. Module Depth — Capy 2, Cobalt 1

**Capy:** The umbrella header `<boost/capy.hpp>` pulls in ~70 headers. This is a large include fan-out, but the types exposed are coherent (buffers, concepts, execution, I/O). Individual headers like `task.hpp`, `read.hpp`, `write.hpp` are focused and pull minimal dependencies. The `detail/` namespace is properly isolated. The `concept/` directory has 17 headers, which is a large concept surface but each concept is narrow and well-documented.

**Cobalt:** The umbrella header is more restrained (21 includes), but individual headers expose substantial implementation. `channel.hpp` has ~200 lines of `read_op`/`write_op` internals before the user reaches the public API. `this_coro.hpp` is 530 lines mixing user API with coroutine memory allocation internals. The ratio of visible interface to hidden complexity is worse than Capy's.

---

### 8. Consistency — Capy 3, Cobalt 1

**Capy:** One pattern governs all operations:
- I/O operations return `io_task<Ts...>` (which is `task<io_result<Ts...>>`)
- All async operations are `co_await`-able `IoAwaitable` types
- Error delivery is always `auto [ec, ...] = co_await ...`
- Composition is always `when_all(...)` / `when_any(...)`
- Launch is always `run*(ex)(task())`
- Documentation follows the same Javadoc template throughout
- Learn one pattern, predict all others

**Cobalt:** Multiple patterns coexist:
- Error delivery: throw (default) vs `as_tuple` vs `as_result` - three modes is powerful but the user must choose per-call
- Completion tokens: `use_op` vs `use_task` for Asio bridging
- Some types use C++20 concepts, others use C++11 `enable_if`/SFINAE (e.g., `use_task_t` on lines 76-85 of `task.hpp`)
- `join`/`gather`/`race` have different return type patterns (tuple vs variant vs indexed pair)
- `promise` has `operator+()` for detach; `detached` is a separate return type for the same purpose

---

### 9. Error Reporting Quality — Capy 3, Cobalt 1

**Capy:**
- Two-tier `error`/`cond` system is textbook `<system_error>` usage
- `[[nodiscard]]` on `io_result` prevents ignored errors
- Structured bindings produce clean `auto [ec, n]` decomposition
- EOF is an `error_code` (not an exception) - correct for I/O
- Cancellation is an `error_code` - correct for expected outcomes
- Documentation specifies which conditions each operation can produce

**Cobalt:**
- Three-mode error handling (throw/tuple/result) is well-designed
- But: `promise::get()` has UB on misuse, not a clean error
- Error enum values have grammatical errors ("completed unexpected")
- Error messages have typos ("raceed", "left_raceed")
- No documentation on which errors each operation produces
- `channel` read/write operations can throw, return tuple, or return result - but nothing in the header explains when to use which

---

### 10. Type Safety — Tie (2 vs 2)

**Capy:**
- `io_result<Ts...>` prevents ignoring errors via `[[nodiscard]]`
- Concepts enforce correct type relationships at compile time
- `executor_ref` is type-erased but type-safe (target casting)
- But: `io_result` fields `t1`, `t2`, `t3` are weakly named; direct access could confuse `t1` and `t2` on a 2-arg result

**Cobalt:**
- `task<T>` and `promise<T>` are strongly typed
- Move-only semantics prevent accidental sharing
- `channel<T>` is typed
- But: `operator bool()` on `promise` has non-obvious semantics
- `race` return types involve `variant` with index correlation - type-safe but complex

---

### 11. Default Ergonomics — Capy 3, Cobalt 2

**Capy:**
- `task<>` defaults to `void` (idiomatic `task<>` for void)
- `read()` has sensible default `initial_amount = 2048`
- `run_async` defaults to recycling allocator when none specified
- Error conditions have natural defaults - no configuration needed
- The common case (executor + task) requires exactly two things

**Cobalt:**
- `co_main` provides zero-configuration entry - excellent
- `generator<Yield, Push=void>` good default
- `race` defaults to built-in PRNG and `asio::cancellation_type::all`
- But: `channel` requires explicit `limit` parameter (no default buffer size)
- `with` requires explicit teardown callable (reasonable but adds ceremony)
- No default executor/context for simple programs beyond `co_main`

---

### 12. Appropriate Complexity — Capy 2, Cobalt 1

**Capy:**
- Core concepts are narrow: `IoAwaitable` has one requirement (`await_suspend(h, env)`), `ReadStream` has one requirement (`read_some(buffers) -> IoAwaitable`)
- `task<T>` has 1 template parameter
- `io_result` is a simple aggregate
- But: 12+18 overloads on `run`/`run_async` is overengineered for the launch API. The concept hierarchy (17 headers in `concept/`) is thorough but large for the problem space.

**Cobalt:**
- Core types are appropriately simple: `task<T>`, `promise<T>`, `channel<T>`
- But: `race.hpp` has 6 overloads with up to 3 template parameters including an NTTP `Ct` for cancellation type
- `result.hpp` is 340 lines of template metaprogramming for error-mode transformation
- `with.hpp` has 5 overloads with dense `requires` clauses
- `this_coro.hpp` is 530 lines mixing simple user API with deep framework internals
- The `op.hpp` type requires understanding Asio initiating functions and completion token protocol

---

### 13. Documentation Quality — Capy 3, Cobalt 1

**Capy:**
- Every public type and function has structured documentation
- Documentation is contract-focused: parameters, return values, preconditions, postconditions, thread safety
- Concepts document both syntactic and semantic requirements
- Examples compile and teach progressive use
- Cross-references link related concepts
- Warning annotations flag dangerous patterns (e.g., coroutine buffer lifetime)
- Design rationale documented (e.g., two-call syntax justification)
- External docs structured as progressive tutorial (sections 2-8)

**Cobalt:**
- External AsciiDoc docs are thorough, covering task/promise/generator/channel semantics, composition primitives, and error handling modes
- However, error messages have typos ("raceed", "completed unexpected", "left_raceed") and `promise.hpp` contains "promimse"
- External docs are organized as reference rather than progressive tutorial - the distinction between `join` and `gather` requires reading both pages and comparing
- No quickstart guide that shows the three most common patterns in sequence

---

## Summary

| Dimension | Capy | Cobalt | Delta |
|-----------|------|--------|-------|
| Gates passed | 3/3 | 2/3 | Capy +1 |
| Total score | 33/39 | 21/39 | Capy +12 |
| Verdict | PASS | CONCERNS | |

Capy's advantage is not in conceptual design - both libraries chose good abstractions. The gap is in **execution of the public interface**: encapsulation, consistency, and safety of the surfaces users actually touch. Cobalt's concepts are sound (task/promise/generator/channel is a clean taxonomy), its `co_main` entry point is the best in any C++ async library, and its three-mode error handling is well-conceived. But the headers through which users encounter these concepts are cluttered with internals, inconsistent in style, and contain traps (`get()` UB) that undermine the strong conceptual foundation.

The comparison illustrates a principle from *On Design*: "Design is not about accepting the constraints the implementation imposes on users. Design is about absorbing those constraints so users don't have to." Capy absorbs its complexity behind clean interfaces. Cobalt exposes its complexity to the user through leaky abstractions and detail-namespace types in public return positions.

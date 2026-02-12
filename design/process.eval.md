# Boost.Process v2 - API Design Quality Evaluation

Evaluated using the C++ API Design Quality Evaluation Framework (`eval-design.md`). Boost.Process v2 provides cross-platform subprocess management integrated with Boost.Asio, including process launching, I/O redirection, environment manipulation, and async wait/execute operations.

---

## Executive Summary

Boost.Process v2 provides a solid, Asio-native subprocess management API with a clean core workflow: construct a `process` with an executor, executable path, arguments, and optional initializers, then wait synchronously or asynchronously. The `popen` class and `process_stdio` initializer make I/O redirection straightforward, and the `environment` namespace offers a complete vocabulary for environment manipulation. The design is weakened by a massive `environment.hpp` header (1,894 lines of public API surface), a constructor-heavy launch model that produces ten overloads on `basic_popen` alone, and several documentation gaps where critical lifetime relationships and side effects are not stated.

## Verdict

⚠️ CONCERNS: 24/39 — A practical, Asio-native process API with a clean core pattern, undermined by a bloated environment surface, inconsistent documentation, and constructor proliferation that obscures the simple underlying model.

## Gates

| Gate                | Result | Finding                                                                                                   |
|---------------------|--------|-----------------------------------------------------------------------------------------------------------|
| G0. Documentation   | ✅     | Javadoc comments exist on most public types and functions; AsciiDoc reference docs and 6 examples exist    |
| G1. No UB Traps     | ✅     | Natural usage patterns are safe; destructor terminates the process unless detached; no silent UB on happy path |
| G2. Ownership        | ✅     | `basic_process` owns and terminates on destruction; `basic_process_handle` is explicitly non-owning; `popen` owns pipes; `detach()` transfers ownership clearly |

**Gate Result:** ✅ ALL GATES PASS

### G0 Notes

Most public types and key functions carry doc comments - `basic_process` has a class-level description, and functions like `interrupt()`, `request_exit()`, `terminate()`, `wait()`, and `async_wait()` have brief Javadoc. The AsciiDoc documentation includes a quickstart guide, initializer docs, and per-header reference pages. Six example programs compile and demonstrate real usage patterns. However, many doc comments are thin (one-liners that restate the function name), several functions have no doc comments at all (e.g. `running()`, `exit_code()`, `native_handle()`), and the examples are POSIX-only (hardcoded `/usr/bin/` paths). The gate passes because a user can understand the API contract from the combination of headers and external docs.

### G1 Notes

The destructor of `basic_process` calls `terminate_if_running()`, which is the safe default - a process that goes out of scope is terminated rather than leaked or left as a zombie. This is the opposite of `std::thread`'s `terminate()` trap. The `wait()` function returns the exit code directly. `async_wait()` delivers `(error_code, int)` through the standard Asio completion handler pattern. No natural usage path leads to undefined behavior. The `running()` function updates internal exit status as a side effect, which is surprising but not UB.

### G2 Notes

Ownership semantics are unambiguous. `basic_process` is move-only, owns the subprocess handle, and terminates on destruction. `basic_process_handle` is explicitly documented as "an unmanaged version" that "does not terminate the process on destruction." `detach()` returns the handle, transferring to non-owning mode. `popen` owns both the process and its stdin/stdout pipes. `process_stdio` binds to I/O objects by reference (e.g. `readable_pipe&`), and the lifetimes are managed by the caller. The one gap: `process_stdio` holds references to caller-owned pipes but does not explicitly document that the pipes must outlive the process. This is conventional for Asio but not stated.

## Design Scores

| #  | Item                    | Score | |
|----|-------------------------|-------|----|
| 1  | Call-Site Clarity        | 2     | ⚠️ |
| 2  | Minimal Ceremony        | 2     | ⚠️ |
| 3  | Progressive Disclosure   | 2     | ⚠️ |
| 4  | Pit of Success           | 2     | ⚠️ |
| 5  | Naming and Vocabulary    | 2     | ⚠️ |
| 6  | Composability            | 2     | ⚠️ |
| 7  | Module Depth             | 1     | 🟡 |
| 8  | Consistency              | 2     | ⚠️ |
| 9  | Error Reporting          | 2     | ⚠️ |
| 10 | Type Safety              | 2     | ⚠️ |
| 11 | Default Ergonomics       | 2     | ⚠️ |
| 12 | Appropriate Complexity   | 1     | 🟡 |
| 13 | Documentation Quality    | 2     | ⚠️ |
|    | **TOTAL**               | **24/39** | |

## Strengths

1. **Asio-native ownership model**: `basic_process` follows Asio's established patterns exactly - executor parameter, async operations with completion tokens, rebind_executor, move-only semantics. An Asio user can predict the API shape before reading it. The destructor-terminates default prevents the `std::thread` mistake.

2. **Flexible I/O redirection via `process_stdio`**: The aggregate initializer design accepts `nullptr` (null device), `FILE*`, native handles, filesystem paths, and Asio pipes - all through implicit conversions on the binding types. C++17 designated initializers (`.err=nullptr`) make the call site expressive. The `popen` class provides automatic pipe setup for the common read/write case.

3. **Cancellation-aware async execution**: `async_execute()` maps Asio cancellation types to process signals (`total` -> interrupt, `partial` -> request_exit, `terminal` -> terminate), providing a clean integration between Asio's structured cancellation and OS-level process signaling without any user-visible plumbing.

## Weaknesses

1. **Environment header is a shallow module**: `environment.hpp` is 1,894 lines of public API exposing `key_view`, `value_view`, `key_value_pair_view`, `key`, `value`, `key_value_pair`, `key_value_pair_view`, `current_view`, `value_iterator`, multiple hash specializations, comparison operators, and free functions. The user gets approximately 15 public types and hundreds of member functions for what is conceptually "set KEY=VALUE." The `key_char_traits` and `value_char_traits` abstractions add platform-specific complexity to every string operation. — *Suggestion:* Split into `environment/fwd.hpp`, `environment/current.hpp`, and `environment/process_environment.hpp` so users who only need `process_environment` do not pay for the full vocabulary. Consider a simplified facade: `environment::var("PATH")` returning a value directly.

2. **Constructor proliferation on `popen`**: `basic_popen` has 12 constructor overloads covering every combination of executor/context, initializer_list/range args, and default/custom launcher. Each overload is a full template with SFINAE enable_if guards. This creates a wall of signatures that obscures the simple underlying pattern: "executor + exe + args + inits." The Launcher-taking constructors place the launcher as the first argument, changing the positional meaning. — *Suggestion:* Reduce to 2-4 constructors using a tag or builder pattern for custom launchers, or provide a factory function `make_popen(launcher, executor, exe, args, inits...)` that keeps the constructor set minimal.

3. **Documentation gaps on side effects and lifetime**: `running()` modifies internal state as a side effect (caching the exit code) but neither the inline doc comment nor the external reference mentions this. `process_stdio` holds references to caller-owned pipes but does not document that the pipes must outlive the process. The `shell` class documentation does not explain what parsing rules it follows. The external reference for `execute.hpp` opens with an incorrect statement ("The execute header provides two error categories"). — *Suggestion:* Audit the external documentation for completeness on side effects, lifetime requirements, and error behavior. Document `running()`'s state mutation. Specify `process_stdio` lifetime requirements. Fix the `execute.hpp` reference page opening.

## Detailed Analysis

### 1. Call-Site Clarity — ⚠️

**Score: 2/3**

The core launch pattern reads clearly:

```cpp
process proc(ctx, "/bin/ls", {});
proc.wait();
```

This is a clean call site - the user's intent (run ls, wait for it) is visible. The `execute()` wrapper reduces it further:

```cpp
assert(execute(process(ctx, "/bin/ls", {})) == 0);
```

The `popen` usage with Asio's `read`/`write` is similarly clear:

```cpp
popen proc(ctx, exe, {"--version"});
asio::write(proc, asio::buffer("main\n"));
```

However, the `process_stdio` initializer introduces noise at the call site because unused fields must be explicitly defaulted:

```cpp
process proc(ctx, exe, {"--version"},
    process_stdio{{}, rp, {}});
```

The empty braces for stdin and stderr are ceremony. The environment setup requires constructing intermediate containers and calling `find_executable` separately, which is correct but verbose.

**Evidence:** `example/intro.cpp` line 27: `proc::process c{ctx, exe, {"--version"}, proc::process_stdio{nullptr, p}};`

---

### 2. Minimal Ceremony — ⚠️

**Score: 2/3**

The simplest case - launch a process and wait - is three lines:

```cpp
asio::io_context ctx;
process proc(ctx, "/bin/ls", {});
proc.wait();
```

This is reasonable. The `io_context` is mandatory because the library is Asio-native, and users of Asio already have one. The empty `{}` for zero arguments is a minor annoyance but unavoidable with the initializer_list overload.

Ceremony increases when I/O is needed. Reading stdout requires constructing a separate `readable_pipe`, binding it through `process_stdio`, then using `asio::read()`:

```cpp
asio::io_context ctx;
asio::readable_pipe rp{ctx};
process proc(ctx, exe, {"--version"},
    process_stdio{{}, rp, {}});
std::string output;
asio::read(rp, asio::dynamic_buffer(output), ec);
```

Six lines with two framework objects before the user touches their data. The `popen` class reduces this to four lines, which is solid. The environment setup requires building an `unordered_map<key, value>` and wrapping it in `process_environment(my_env)`, which is proportional to the complexity of the task.

**Evidence:** `example/quickstart.cpp` lines 13-17 (launch); `example/intro.cpp` lines 22-36 (I/O with pipe).

---

### 3. Progressive Disclosure — ⚠️

**Score: 2/3**

Three levels exist:

- **Beginner**: `process proc(ctx, exe, args); proc.wait();` - launch and wait.
- **Intermediate**: `popen` for I/O, `process_stdio` for redirection, `process_environment` for env vars, `process_start_dir` for working directory.
- **Expert**: Custom launchers (`bind_launcher`), platform-specific extensions (`windows::as_user_launcher`, `posix::vfork_launcher`, `posix::bind_fd`), process inspection (`ext::cmd`, `ext::cwd`, `ext::env`, `ext::exe`).

The beginner level is accessible. The intermediate level requires understanding the initializer pattern (variadic constructor args) and the `process_stdio` aggregate, which is documented in the quickstart. The expert level (custom launchers) requires understanding the launcher protocol (`on_setup`, `on_exec_setup` callbacks) which is not well-documented in the headers.

The main gap: there is no clear path from "I just want to run a command string" to the shell-parsed model. The `shell` class exists but its relationship to `process` construction is not obvious from the quickstart - the user must find it independently.

**Evidence:** `doc/quickstart.adoc` covers beginner and signal levels but not I/O; `shell.hpp` doc comment shows the pattern but it is not linked from the quickstart.

---

### 4. Pit of Success — ⚠️

**Score: 2/3**

The destructor-terminates design is a strong pit of success: the default behavior (process dies when handle dies) is safe, unlike `std::thread` which called `std::terminate()`. Moving the process transfers ownership cleanly. The `detach()` method requires explicit opt-in to unmanaged mode.

`async_execute` correctly maps cancellation types to signal severity, so the most natural way to cancel (Asio's `cancel_after`) does the right thing.

However, several patterns are traps:

- Calling `exit_code()` before `wait()` returns the native "still active" sentinel (259 on Windows, 0x17f on POSIX) interpreted through `evaluate_exit_code()`, which may return a nonsensical value. The function does not assert or error.
- The `running()` function has the side effect of caching the exit code. Calling `running()` in a loop, then `exit_code()`, works correctly. But calling `exit_code()` without either `wait()` or `running()` first gives a stale result. This implicit state coupling is not documented.
- `process_stdio` with default-constructed members inherits the parent's stdio, which is correct but means `process_stdio{}` with no arguments is identical to omitting it entirely - a potential source of confusion.

**Evidence:** `process.hpp` line 288: `exit_code()` reads `exit_status_` which is initialized to `detail::still_active`. No guard or assert.

---

### 5. Naming and Vocabulary — ⚠️

**Score: 2/3**

Core names are clear and domain-appropriate: `process`, `popen`, `process_handle`, `shell`, `process_stdio`, `process_start_dir`, `process_environment`. These are self-documenting. The verb functions follow OS conventions: `terminate()`, `interrupt()`, `request_exit()`, `suspend()`, `resume()`, `wait()`, `detach()`.

`find_executable()` is clear. `current_pid()`, `all_pids()`, `parent_pid()`, `child_pids()` form a consistent vocabulary.

Minor issues:

- `basic_process` vs `basic_process_handle`: the distinction (owning vs non-owning) is important but "handle" typically implies ownership in C++. "process_ref" or "process_observer" would be clearer.
- `evaluate_exit_code()` is an unusual name for what is essentially "extract the portable exit code from the native status." `portable_exit_code()` or `exit_status()` would be more immediately clear.
- `cstring_ref` is a library-specific vocabulary type that must be learned. It serves a real need (null-terminated string view for C APIs) but adds to the concept count.
- The `ext` namespace abbreviation is unclear without context. `inspect` or `query` would communicate the "read info about another process" purpose.

**Evidence:** `pid.hpp` functions; `exit_code.hpp` `evaluate_exit_code()`; `cstring_ref.hpp`.

---

### 6. Composability — ⚠️

**Score: 2/3**

`popen` satisfies Asio's `AsyncReadStream` and `AsyncWriteStream` concepts, so it composes with `asio::read()`, `asio::write()`, `asio::read_until()`, and `asio::dynamic_buffer`. This is the library's strongest composability feature - the popen object slots into Asio's generic algorithm ecosystem.

`process_stdio` accepts any object with a compatible `native_handle()`, enabling composition with Asio sockets, pipes, and files.

The `environment` types provide iterators, `std::hash` specializations, and comparison operators, enabling use with standard containers (`unordered_map<key, value>`).

However, `process` itself does not compose naturally with standard algorithms or ranges - it is a single object, not a container or range. The `bound_launcher` is a composition mechanism but it composes launchers, not processes. The `shell` class does not directly compose with `process` construction - the user must call `shell.exe()` and `shell.args()` separately.

**Evidence:** `popen.hpp` `read_some()`/`write_some()`/`async_read_some()`/`async_write_some()`; `example/stdio.cpp` line 68: `asio::read_until(proc, asio::dynamic_buffer(line), '\n');`.

---

### 7. Module Depth — 🟡

**Score: 1/3**

This is the library's weakest design dimension. The `environment.hpp` header is 1,894 lines of public API surface exposing approximately 15 types with full member function sets, plus free functions, hash specializations, stream operators, and comparison operators. Including `<boost/process.hpp>` pulls in all of this plus `cstring_ref.hpp`, `bind_launcher.hpp`, `shell.hpp`, and platform-specific launcher headers.

The `process_stdio` header pulls in platform-specific I/O binding detail types (`process_io_binding<Target>`) that are in the `detail` namespace but are visible as template arguments of the `process_stdio` aggregate's public members. The Windows implementation exposes `handle_closer`, `HANDLE`, and `STARTF_USESTDHANDLES` through the stdio header.

The `process_handle.hpp` conditionally includes one of four different platform-specific detail headers, each of which brings in OS-specific types. The user sees `basic_process_handle` as a type alias to a detail type.

The `default_launcher.hpp` similarly aliases to platform detail.

The include fan-out is significant: a single `#include <boost/process.hpp>` brings in Asio headers (executor, completion token, pipes, connect_pipe), filesystem, boost type_traits, algorithm, and platform headers.

**Evidence:** `environment.hpp` 1,894 lines; `stdio.hpp` exposes `detail::process_io_binding<>` as members of `process_stdio`; `process_handle.hpp` uses `using basic_process_handle = detail::basic_process_handle_*<Executor>`.

---

### 8. Consistency — ⚠️

**Score: 2/3**

The library follows Asio conventions consistently: every async operation follows the `async_*(completion_token)` pattern, every synchronous operation has throwing and non-throwing (error_code) overloads, types take an executor template parameter, and `rebind_executor` is provided.

The initializer pattern is consistent: `process_stdio`, `process_start_dir`, and `process_environment` all work the same way - passed as trailing arguments to the process constructor.

The signal functions (`interrupt`, `request_exit`, `terminate`) each have a throwing and non-throwing overload, consistently.

Inconsistencies:

- `running()` is the only query function that has throwing and non-throwing overloads. `exit_code()` and `is_open()` are const and non-throwing. `id()` is const. But `running()` mutates state and can throw. This breaks the pattern.
- The `execute()` function takes `basic_process` by value (moves in), while `async_execute()` also takes by value. But the process constructor takes executor first, while `execute()` takes the process object. The abstraction levels are inconsistent.
- `pid.hpp` free functions (`parent_pid`, `child_pids`, `all_pids`) use the throwing/non-throwing overload pattern but lack doc comments (they use `//` instead of `///`).
- `shell` uses `char_type` (platform-dependent) while the rest of the API uses `string_view` (char).

**Evidence:** `process.hpp` `running()` vs `exit_code()` vs `is_open()`; `pid.hpp` `//` comments on `parent_pid`, `child_pids`.

---

### 9. Error Reporting — ⚠️

**Score: 2/3**

The library follows the Asio dual-overload pattern: every operation has a throwing version and an `error_code&` version. Errors are reported through `boost::system::error_code` with platform-appropriate categories.

The `error.hpp` header provides `utf8_conv_error` and `exit_code_category` for domain-specific error classification. The `check_exit_code()` helper subsumes native exit codes into error_codes, which is a clean pattern for composing with Asio's error handling.

`async_execute()` maps cancellation types to specific signals, and errors from the wait propagate through the completion handler.

Weaknesses:

- `exit_code()` returns a possibly-stale integer with no indication that the process has not been waited on. There is no `std::optional` or error-returning variant.
- `running()` can throw on internal errors but its doc comment does not list what errors are possible.
- The `shell` parser stores parse errors internally but does not expose them - the user cannot distinguish a parse failure from an empty command.
- Compile-time error messages when passing incompatible initializers rely on SFINAE enable_if, which can produce unclear diagnostics.

**Evidence:** `exit_code.hpp` `check_exit_code()`; `error.hpp` `utf8_conv_error` enum; `process.hpp` `exit_code()` returns raw int.

---

### 10. Type Safety — ⚠️

**Score: 2/3**

`pid_type` is a platform-specific integer typedef, not a strong type. This means a `pid_type` can be accidentally mixed with other integers. However, since `pid_type` is the OS convention, this is pragmatic.

`native_exit_code_type` is similarly a raw integer. The `evaluate_exit_code()` function provides the interpretation layer, and `process_is_running()` provides a named predicate rather than raw bitmask tests.

`basic_process` and `basic_process_handle` are distinct types that prevent accidentally using a non-owning handle where an owning process is expected (different types, no implicit conversion).

The `environment::key` and `environment::value` types use platform-specific char traits (`key_char_traits`, `value_char_traits`) which provide some type distinction, though both resolve to string-like types.

`process_stdio` uses distinct binding types for input, output, and error, preventing mixing (you cannot assign a `process_output_binding` where an `process_input_binding` is expected).

The main weakness is `process_stdio`'s acceptance of raw `int` (POSIX fd) or `HANDLE` (Windows) without wrapper types, making it easy to pass an arbitrary integer as a file descriptor.

**Evidence:** `pid.hpp` `typedef int pid_type;`; `exit_code.hpp` `typedef int native_exit_code_type;`; `stdio.hpp` `process_io_binding(int fd)`.

---

### 11. Default Ergonomics — ⚠️

**Score: 2/3**

The defaults are sensible:

- Default executor is `net::any_io_executor` - the common case.
- Omitting initializers gives a process that inherits parent stdio, uses the current environment, and runs in the current directory - correct defaults.
- `process_stdio{}` with no arguments inherits parent handles.
- `shell::exe()` searches the current environment by default.
- `find_executable()` defaults to the current environment.

The user must always provide an executor or execution context, but this is inherent to the Asio model. The empty `{}` for zero arguments is a minor ergonomic cost.

The main gap: there is no default for "just run this command string." The user must either construct executable path + args separately, or use the `shell` class to parse, then extract `exe()` and `args()`. A convenience constructor accepting a single command string would serve the 80% case.

**Evidence:** `default_launcher.hpp` typedef; `shell.hpp` `exe()` default parameter `Environment && env = environment::current()`.

---

### 12. Appropriate Complexity — 🟡

**Score: 1/3**

The core process model is appropriately complex: executor + path + args + initializers. This is proportional to the problem.

However, the library introduces significant complexity beyond what the problem demands:

- **Constructor overload explosion**: `basic_process` has 10+ constructors, `basic_popen` has 12+, each a template with SFINAE guards. The combinatorial expansion covers executor/context x init_list/range x default/custom launcher. This could be reduced with a builder or factory.
- **`bound_launcher`** reimplements `index_sequence` and `make_index_sequence` (available since C++14) and provides 8 overloads of `operator()` for the same executor/context x error_code permutations.
- **`environment.hpp`** is 1,894 lines for what is conceptually a string-string map with platform awareness. The `key_view`, `value_view`, `key_value_pair_view`, `key`, `value`, `key_value_pair` hierarchy (6 types for key-value pairs) is more types than use cases.
- **`cstring_ref`** is a 233-line null-terminated string view type. This is a real need for C API interop, but it adds a vocabulary type the user must learn that is not standard.
- **Platform abstraction via conditional compilation** throughout the headers (Windows vs POSIX `#ifdef` blocks in `stdio.hpp`, `environment.hpp`) exposes implementation structure to the user reading headers.

**Evidence:** `popen.hpp` 12 constructors; `bind_launcher.hpp` custom `index_sequence`; `environment.hpp` 1,894 lines with 6 key-value types; `cstring_ref.hpp` 233 lines.

---

### 13. Documentation Quality — ⚠️

**Score: 2/3**

External AsciiDoc documentation exists and is structured around tasks: quickstart, initializers (stdio, environment, start_dir), then reference. The quickstart opens with a concrete example and builds progressively through lifetime management, signaling, and execute functions.

Six example programs are provided, each demonstrating a real use case: basic launch, pipe I/O, popen, stdio redirection, environment manipulation, and start directory.

Weaknesses:

- Examples are POSIX-only (hardcoded `/usr/bin/g++`, `/bin/ls`, `/bin/bash`). A Windows user cannot run them.
- Parameters, return values, and side effects are unevenly documented across both inline and external docs. The `execute()` function's documentation is the best example with `@param`, `@return`, and `@exception`, but this level of detail is the exception, not the rule.
- The `shell` class documentation does not explain what parsing rules it follows (POSIX shell quoting? Windows CommandLineToArgvW?).
- The reference docs for `execute.hpp` open with "The execute header provides two error categories" which is incorrect - it provides execute functions.
- The `environment.adoc` reference is comprehensive but encyclopedic - organized by type rather than by task.
- `running()`'s side effect of caching the exit code is not documented anywhere.

**Evidence:** `doc/reference/execute.adoc` incorrect opening sentence; examples use POSIX-only paths; `running()` side effect undocumented.

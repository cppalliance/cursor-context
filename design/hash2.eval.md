# Boost.Hash2 - API Design Quality Evaluation

## Executive Summary

Boost.Hash2 is a cleanly factored hashing framework that separates hash algorithms from type serialization, enabling any user-defined type to work with any hash algorithm. The core design - a uniform `HashAlgorithm` concept, a single `hash_append` dispatch function, and a `flavor` mechanism for byte-order control - is well-proportioned to the problem. The library provides 15+ hash algorithm implementations (FNV-1a through SHA-3 and BLAKE2) behind a consistent interface, plus HMAC as a composable wrapper. The primary design weaknesses are the ceremony required to bridge between hash algorithms and `std::hash`-based containers, the `void const*` interface on `update` that provides no type safety at the byte-feeding boundary, and documentation that lacks a quickstart path and is organized by header rather than by task.

## Verdict

✅ PASS: 27/39 — A well-factored hashing framework with strong composability and consistency, held back by container integration ceremony, a raw-pointer byte interface, and documentation that needs a quickstart path.

## Gates

| Gate                | Result | Finding                                                                                                 |
|---------------------|--------|---------------------------------------------------------------------------------------------------------|
| G0. Documentation   | ✅     | AsciiDoc reference covers every type, function, and algorithm; 11 compilable examples exist              |
| G1. No UB Traps     | ✅     | Natural usage patterns are safe; `update` with null pointer and zero size is defined; no silent UB traps |
| G2. Ownership        | ✅     | All types are value types with no heap allocation; `digest<N>` is a plain byte array; no ownership questions |

**Gate Result:** ✅ ALL GATES PASS

### G0 Notes

The external AsciiDoc documentation is thorough: `hashing_bytes.adoc` specifies the full `HashAlgorithm` concept with requirements for each member, `hashing_objects.adoc` explains every dispatch case of `hash_append` with compilable examples, and the reference pages provide effects/returns/requires clauses for every public function. The 11 example programs are self-contained and demonstrate real usage patterns (unordered containers, md5sum CLI, compile-time hashing, HMAC, result extension, JSON value hashing). The public headers themselves contain no inline doc comments, which is a reasonable choice for a library in a large ecosystem where header comment edits would force downstream rebuilds across many translation units. The external documentation fully covers the API contract.

### G1 Notes

The natural usage pattern - construct a hash algorithm, call `update`, call `result` - has no UB traps. Calling `update(nullptr, 0)` is safe (zero-length update is a no-op). Calling `result()` multiple times is explicitly defined to produce a pseudorandom sequence. The constructors with seed=0 or n=0 are equivalent to default construction by convention, which is documented. The `hash_append` function handles negative zero floats correctly (replaces with positive zero before bit_cast). There are no destructor traps, no blocking surprises, and no silent data corruption on the happy path.

### G2 Notes

All types are plain value types. Hash algorithm classes hold fixed-size internal state (arrays of integers). `digest<N>` is a fixed-size byte array. `hmac<H>` composes two instances of `H`. There is no dynamic allocation, no pointer ownership, no reference lifetime hazard. Every type is copyable, and copies are independent of their originals. The `update` function takes a non-owning pointer-and-size pair with no ownership implications.

## Design Scores

| #  | Item                    | Score | |
|----|-------------------------|-------|----|
| 1  | Call-Site Clarity        | 2     | ⚠️ |
| 2  | Minimal Ceremony        | 1     | 🟡 |
| 3  | Progressive Disclosure   | 2     | ⚠️ |
| 4  | Pit of Success           | 2     | ⚠️ |
| 5  | Naming and Vocabulary    | 3     | ✅ |
| 6  | Composability            | 2     | ⚠️ |
| 7  | Module Depth             | 2     | ⚠️ |
| 8  | Consistency              | 3     | ✅ |
| 9  | Error Reporting          | 2     | ⚠️ |
| 10 | Type Safety              | 1     | 🟡 |
| 11 | Default Ergonomics       | 2     | ⚠️ |
| 12 | Appropriate Complexity   | 2     | ⚠️ |
| 13 | Documentation Quality    | 2     | ⚠️ |
|    | **TOTAL**               | **27/39** | |

## Strengths

1. **Consistency**: Every hash algorithm follows the identical interface - default constructor, `uint64_t` seed constructor, byte-sequence seed constructor, `update`, `result`. Learn one algorithm, predict all others. The naming is mechanical: `sha2_256`, `sha2_512`, `blake2b_512`, `fnv1a_64`. The HMAC wrappers follow the same pattern exactly. The `hash_append` family (`hash_append`, `hash_append_range`, `hash_append_size`, `hash_append_range_and_size`, `hash_append_unordered_range`) forms a coherent, predictable vocabulary.

2. **Naming and Vocabulary**: Names are self-documenting and domain-appropriate. `digest<N>` is exactly what it is - an N-byte message digest. `hash_append` says what it does - append a value's representation to a hash. `siphash_64` names the algorithm and its output width. The flavor names (`default_flavor`, `little_endian_flavor`, `big_endian_flavor`) communicate their purpose immediately. There is no jargon that requires framework-specific knowledge - the vocabulary comes from the hashing domain itself.

3. **Composability via algorithm/type separation**: The core design insight - separating the hash algorithm from the type serialization via `hash_append` - means any type that supports `hash_append` automatically works with any hash algorithm, and any new hash algorithm automatically works with all existing types. The `tag_invoke` customization point lets user-defined types participate without including the full `hash_append.hpp` header. The `hmac<H>` wrapper composes any cryptographic hash into its HMAC variant with zero glue code.

## Weaknesses

1. **Container integration requires user-written adapter**: The most common use case for non-cryptographic hashing - using a hash algorithm with `std::unordered_map` or `boost::unordered_flat_map` - requires the user to write a `hash<T, H>` adapter class from scratch. The library provides no ready-made adapter, and the examples devote five progressive versions to building one. This is the primary ceremony barrier. A user who just wants to use SipHash with an unordered map must write 15-20 lines of boilerplate before inserting a single key. — *Suggestion:* Provide a `boost::hash2::hash<T, H>` adapter class (or a `make_hash<H>()` factory) that satisfies the `std::hash` interface out of the box, reducing the common case to a single line: `unordered_map<string, int, boost::hash2::hash<string, siphash_64>> map;`.

2. **`void const*` interface on `update` provides no type safety**: The primary byte-feeding function `update(void const* data, std::size_t n)` accepts any pointer with no compile-time protection against passing the wrong data or mismatched sizes. A user can accidentally pass `sizeof(ptr)` instead of the buffer length, or pass a pointer to a struct instead of its serialized bytes, and the compiler will not catch the mistake. The `constexpr` overload taking `unsigned char const*` is safer but only available in constexpr contexts. — *Suggestion:* Add a `std::span<const unsigned char>` overload (or `std::span<const std::byte>`) as the primary interface, keeping the `void const*` overload for backward compatibility. This would catch size mismatches and pointer type errors at compile time.

3. **Documentation lacks a quickstart path and task-oriented organization**: The documentation opens with the `HashAlgorithm` concept requirements rather than a concrete use case. A user who wants to compute an MD5 checksum must read through the concept specification before finding an example. The reference pages are organized by header, not by task, so "how do I hash a string for an unordered map" requires piecing together information from `hash_append.adoc`, `get_integral_result.adoc`, and `examples.adoc`. — *Suggestion:* Add a "Getting Started" section that opens with the three most common tasks (compute a digest, hash an object, use with an unordered container) as self-contained code blocks, before the concept specification.

## Detailed Analysis

### 1. Call-Site Clarity — ⚠️

**Score: 2/3**

The byte-hashing call site is clean and reads as the user's algorithm:

```cpp
sha2_256 hash;
hash.update(buffer, n);
auto digest = hash.result();
```

Three lines, each expressing a clear step: create, feed, finalize. The `hash_append` call site for hashing C++ objects is also reasonable:

```cpp
fnv1a_64 h;
hash_append(h, {}, my_string);
auto value = get_integral_result<std::size_t>(h);
```

However, the `{}` for the default flavor is a piece of framework vocabulary that the user must understand (or at least accept). The `get_integral_result<std::size_t>(h)` call adds machinery - the user wanted a hash value, and the conversion from the algorithm's result type to `size_t` requires knowing about this utility function. In the unordered container use case, the full call site inside `operator()` is:

```cpp
H h(h_);
boost::hash2::hash_append(h, {}, v);
return boost::hash2::get_integral_result<std::size_t>(h);
```

This is three lines of library machinery to hash a value and get an integer - clear but not invisible.

**Evidence:** `example/hash_without_seed.cpp` lines 16-21: the `operator()` body is three framework calls, none of which express the user's algorithm ("hash this string").

---

### 2. Minimal Ceremony — 🟡

**Score: 1/3**

The simplest use case - hash bytes and get a digest - is three lines, which is proportional:

```cpp
md5_128 hash;
hash.update(data, size);
auto d = hash.result();
```

But the most common use case - use a hash with an unordered container - requires writing a full adapter class. The `hash_without_seed.cpp` example is 12 lines of boilerplate (a template class with `operator()`) before the user can write `map["foo"] = 1`. The seeded variants (`hash_with_uint64_seed.cpp`, `hash_with_byte_seed.cpp`) add another 5-10 lines each. The "universal" adapter in `hash_with_any_seed.cpp` is 20 lines of boilerplate.

For hashing a C++ object to get a digest, the ceremony is:

```cpp
md5_128 hash;
hash_append(hash, {}, my_object);
auto d = hash.result();
```

The `{}` for the flavor is minor but unexplained at the call site. For an integer result, `get_integral_result<std::size_t>(h)` adds another function the user must discover.

The compile-time hashing example requires `unsigned char` arrays specifically because `void const*` cannot be used in constexpr - the user must know about the `unsigned char const*` overload.

**Evidence:** `example/hash_with_any_seed.cpp` requires 20 lines of class definition before the first useful operation. Five examples are dedicated to progressively building the same adapter class.

---

### 3. Progressive Disclosure — ⚠️

**Score: 2/3**

Three levels exist:

- **Beginner**: Hash bytes with `md5_128` + `update` + `result`. Hash a file with `md5sum.cpp` as a template.
- **Intermediate**: Hash C++ objects with `hash_append`. Use flavors for endian control. Use `get_integral_result` for integer results. Build an unordered container adapter.
- **Expert**: Write `tag_invoke` overloads for custom types. Use `hash_append_provider` for decoupled headers. Use compile-time hashing. Extend output with repeated `result()` calls. Write custom hash algorithms conforming to the concept.

The beginner level is accessible - the `HashAlgorithm` concept is simple and well-explained in `hashing_bytes.adoc`. The intermediate level requires learning `hash_append`, flavors, and `get_integral_result`, which is a moderate conceptual jump. The expert level (writing `tag_invoke` overloads with `Provider`, `Hash`, and `Flavor` template parameters) is a significant step up but well-documented.

The gap is that the intermediate level does not have a turnkey solution for the most common intermediate task (use with unordered containers). The user must build the adapter themselves, and the path from "I have a hash algorithm" to "I can use it with `unordered_map`" requires understanding four separate components (`hash_append`, `get_integral_result`, copy semantics of hash objects, and the `std::hash` interface).

**Evidence:** `hashing_bytes.adoc` covers beginner; `hashing_objects.adoc` covers intermediate/expert; `examples.adoc` builds the container adapter progressively across 5 examples.

---

### 4. Pit of Success — ⚠️

**Score: 2/3**

Several design choices push toward correct usage:

- Default construction with seed=0 equivalence means the user cannot accidentally use an uninitialized state.
- `hash_append` handles negative-zero floats automatically, preventing hash collisions between `+0.0` and `-0.0`.
- Calling `result()` multiple times is defined behavior (pseudorandom extension), not UB.
- Unordered ranges are handled correctly by `hash_append_unordered_range`, which produces order-independent hash values.
- The `has_constant_size` trait ensures that fixed-size containers (like `std::array`) do not hash their size, while variable-size containers do - preventing the `[[1], []]` vs `[[], [1]]` collision.

However, several patterns are not prevented:

- Nothing stops a user from using FNV-1a or xxHash for security-sensitive purposes (MAC, digital signatures). The documentation warns against this but the API does not distinguish cryptographic from non-cryptographic algorithms at the type level.
- SipHash without a seed is the default construction path, and the library explicitly allows it. The documentation says "never use SipHash without a seed" but the API makes it easy. The `hash_without_seed.cpp` example demonstrates this exact anti-pattern (though it uses FNV-1a, not SipHash).
- The `void const*` interface lets the user pass any pointer to `update`, including a pointer to a struct that should be serialized through `hash_append` instead. The compiler cannot distinguish "raw bytes" from "structured data that needs serialization."

**Evidence:** `hashing_bytes.adoc` "Choosing a Hash Algorithm" section warns about SipHash without seed; `fnv1a.hpp` default constructor is public with no warning; `hash_append.hpp` handles float negative zero at line-level dispatch.

---

### 5. Naming and Vocabulary — ✅

**Score: 3/3**

Names are self-documenting throughout. The hash algorithm names follow a consistent `algorithm_bits` pattern: `fnv1a_32`, `fnv1a_64`, `sha2_256`, `sha2_512`, `blake2b_512`, `blake2s_256`, `siphash_32`, `siphash_64`, `xxhash_32`, `xxhash_64`, `md5_128`, `sha1_160`, `ripemd_128`, `ripemd_160`. A reader can identify the algorithm family and output width from the name alone.

The HMAC aliases prepend `hmac_`: `hmac_sha2_256`, `hmac_md5_128`. Predictable from the pattern.

The function names are verb phrases that describe their action: `hash_append`, `hash_append_range`, `hash_append_size`, `hash_append_range_and_size`, `hash_append_unordered_range`, `get_integral_result`. Each name communicates exactly what it does.

`digest<N>` is the domain term for a hash output. `flavor` is slightly unusual but is explained clearly and is used consistently. `endian` with values `little`, `big`, `native` is standard vocabulary.

The trait names follow Boost conventions: `is_contiguously_hashable`, `is_trivially_equality_comparable`, `is_endian_independent`, `has_constant_size`. Each reads as a predicate.

**Evidence:** All 30+ type names; all 5 function names; all 4 trait names follow consistent, self-documenting patterns.

---

### 6. Composability — ⚠️

**Score: 2/3**

The algorithm/type separation is the library's core composability feature. Any type that provides a `tag_invoke` overload for `hash_append_tag` automatically works with every hash algorithm. Conversely, any hash algorithm that satisfies the `HashAlgorithm` requirements automatically works with all types that support `hash_append`. This is genuine two-dimensional composability.

The `hmac<H>` template composes any cryptographic hash into its HMAC variant. `hmac<sha2_256>` satisfies the same `HashAlgorithm` interface, so it works with `hash_append` and all existing types.

`digest<N>` provides iterators, `data()`, `size()`, `operator==`, `operator<<`, and `to_string()`, enabling use with standard algorithms and I/O.

However, the library does not compose with `std::hash` or `std::unordered_map` without a user-written adapter. The `json_value.cpp` example shows that composing with Boost.JSON requires writing a `tag_invoke` overload in `boost::json` namespace - correct but requiring knowledge of both libraries' customization mechanisms.

The `hash_append` function does not work with `std::variant`, `std::optional`, or `std::any` out of the box. These are increasingly common vocabulary types, and their absence means the user must write `tag_invoke` overloads for them.

**Evidence:** `hmac.hpp` satisfies HashAlgorithm; `json_value.cpp` requires 7-line `tag_invoke` overload; no `std::hash` adapter provided; no `std::variant`/`std::optional` support.

---

### 7. Module Depth — ⚠️

**Score: 2/3**

The individual algorithm headers are reasonably deep. Including `<boost/hash2/sha2.hpp>` gives the user 6 algorithm classes and 6 HMAC aliases, hiding the SHA-2 round functions, padding logic, block processing, and state management behind a 3-function interface (`update`, `result`, constructors`). The ratio of hidden complexity to visible surface is favorable.

The `hash_append.hpp` header hides substantial dispatch logic (contiguously hashable optimization, floating-point normalization, range/tuple/described-class handling) behind a single function template. However, it pulls in a significant transitive dependency graph: `is_contiguously_hashable.hpp`, `has_constant_size.hpp`, `get_integral_result.hpp`, `flavor.hpp`, and detail headers, plus `boost/container_hash/*`, `boost/describe/*`, and `boost/mp11/*`. A user who includes `hash_append.hpp` gets Boost.ContainerHash's range/tuple/describe detection traits, Boost.Describe's class introspection, and Boost.Mp11's type lists - subsystems the user did not ask for.

The total public header count (21 main + 2 legacy) is proportional to the number of distinct algorithms, which is reasonable. Each algorithm header is self-contained (you can include just `md5.hpp` without `hash_append.hpp` if you only need byte hashing).

Detail headers (13 files) stay in `boost::hash2::detail` and do not leak types into the public namespace.

**Evidence:** `hash_append.hpp` includes `boost/container_hash/is_range.hpp`, `boost/describe/bases.hpp`, `boost/mp11/algorithm.hpp`; `sha2.hpp` includes only `digest.hpp` and `hmac.hpp` plus detail headers.

---

### 8. Consistency — ✅

**Score: 3/3**

One pattern governs all hash algorithms:

- **Construction**: default, `uint64_t` seed, `(void const*, size_t)` byte seed, `(unsigned char const*, size_t)` constexpr byte seed
- **Feeding**: `update(void const*, size_t)` and `update(unsigned char const*, size_t)`
- **Finalizing**: `result()` returns `result_type`
- **Types**: `result_type` typedef, optional `block_size` constant

Every algorithm - from the trivial FNV-1a to the complex SHA-3 Keccak - follows this exact interface. The HMAC wrapper follows the same interface. The user can swap algorithms by changing one type name.

The `hash_append` family is internally consistent: `hash_append` for single values, `hash_append_range` for ranges, `hash_append_size` for sizes, `hash_append_range_and_size` for the combination, `hash_append_unordered_range` for order-independent ranges. The naming is compositional and predictable.

The flavor types are uniform: each has `size_type` and `byte_order`, nothing else.

The seeding convention (0 or null = default construction) is consistent across all algorithms.

**Evidence:** `fnv1a.hpp`, `sha2.hpp`, `siphash.hpp`, `blake2.hpp`, `hmac.hpp` all expose identical interface shapes. `flavor.hpp` three types with identical structure.

---

### 9. Error Reporting — ⚠️

**Score: 2/3**

Compile-time errors are generally clear. Passing a type that does not support `hash_append` (not integral, not a range, not described, no `tag_invoke` overload) produces a compile error. The dispatching in `hash_append.hpp` uses `static_assert` as a fallback, which should produce a readable message.

The `digest<N>` comparison uses constant-time comparison (via `detail::memcmp`), which is correct for cryptographic use and does not silently corrupt timing information.

`to_chars` for `digest<N>` returns `nullptr` if the output buffer is too small - a clear failure signal. The array overload has a `static_assert` on buffer size, catching the error at compile time.

However, there is no error reporting infrastructure for runtime errors. Hash algorithms do not report errors - they silently produce output for any input. This is correct for the domain (hash algorithms are total functions), but it means there is no mechanism to report: truncated input (the user forgot to feed all the data), misuse of `result()` (calling it before any `update`), or algorithm-level issues. Calling `result()` before any `update` calls is valid and produces the hash of the empty message, which is correct per specification but might surprise a user who expected an error.

Compile-time error messages when `tag_invoke` overloads are missing or malformed will reference the dispatch machinery in `hash_append.hpp`, which may be difficult to interpret.

**Evidence:** `digest.hpp` `to_chars` with `static_assert(M >= N * 2 + 1)`; `hash_append.hpp` dispatches through multiple `if constexpr` / `enable_if` branches.

---

### 10. Type Safety — 🟡

**Score: 1/3**

The primary byte-feeding interface is `void const*` + `size_t`, which provides zero type safety. The compiler cannot distinguish:

```cpp
hash.update(&my_int, sizeof(my_int));   // correct
hash.update(&my_int, sizeof(&my_int));  // wrong size, compiles fine
hash.update(&my_struct, sizeof(my_struct)); // probably wrong, compiles fine
```

There is no distinction at the type level between cryptographic and non-cryptographic algorithms. Both `fnv1a_32` and `sha2_256` satisfy the same concept. A user can accidentally use FNV-1a for HMAC (it will compile and produce output, but the output has no cryptographic value). The `hmac<H>` template does not constrain `H` to algorithms with `block_size`, though it will fail at compile time if `block_size` is missing - this is an implicit constraint, not an explicit one.

The `result_type` is either an unsigned integer or `digest<N>`. These are distinct types, but the user must use `get_integral_result` to bridge between them, which is a manual conversion that could be forgotten or applied incorrectly.

The `flavor` parameter to `hash_append` is unconstrained - any type with `size_type` and `byte_order` members will work, but there is no concept or constraint to enforce this. Passing the wrong type produces an error deep in the dispatch machinery.

On the positive side, `digest<N>` is a distinct type from `std::array<unsigned char, N>`, preventing accidental mixing. Different digest sizes are different types (`digest<16>` vs `digest<32>`), so you cannot accidentally compare an MD5 digest to a SHA-256 digest.

**Evidence:** `update(void const* data, std::size_t n)` signature on every algorithm; `hmac<H>` takes unconstrained `H`; `hash_append` takes unconstrained `Flavor`.

---

### 11. Default Ergonomics — ⚠️

**Score: 2/3**

Sensible defaults exist for common cases:

- `hash_append(h, {}, v)` uses `default_flavor` when no flavor is specified - the empty braces serve as "use the default."
- Default construction of any algorithm gives the unseeded initial state as published in the specification.
- `digest<N>` is default-constructible (zero-initialized).
- `hmac<H>` default-constructs to empty-key HMAC.
- Seed of 0 or null byte sequence both mean "default construction" across all algorithms.

However, several defaults are missing:

- There is no default hash algorithm. The user must always choose one, even for the common case of "I just want a good hash for my unordered container." A `default_hash` alias pointing to SipHash-64 (or platform-appropriate variant) would serve the 80% case.
- There is no default adapter for `std::hash`. The user must build one from scratch.
- The `flavor` default is `endian::native`, which produces platform-dependent hash values. This is the right default for performance but means the user must actively choose `little_endian_flavor` for portable hashes. The tradeoff is well-documented but could surprise a user who computes hashes on one platform and verifies on another.

**Evidence:** `hash_append` default flavor via `{}` brace initialization; no `default_hash` alias; no `std::hash` adapter.

---

### 12. Appropriate Complexity — ⚠️

**Score: 2/3**

The core abstraction level is well-calibrated. The `HashAlgorithm` concept has exactly three operations (construct, update, result) plus three constructor forms (default, integer seed, byte seed). This is the minimum needed to support unseeded hashing, seeded hashing, and keyed hashing. The `flavor` mechanism adds one dimension of control (byte order) with one type.

The `hash_append` dispatch handles 12 type categories (integral, float, enum, pointer, nullptr, array, tag_invoke, unordered range, contiguous range, range, tuple, described class). This is a lot of cases, but each serves a real use case, and the dispatch is hidden from the user.

The `tag_invoke` customization mechanism with `hash_append_tag` and `hash_append_provider` is more complex than a simple friend function overload. The `Provider` parameter exists to allow header decoupling (the user type's header need not include `hash_append.hpp`), which is a real concern for large codebases but adds a template parameter that most users will ignore. A simpler `friend void hash_append(Hash& h, Flavor const& f, X const& v)` overload found via ADL would serve 90% of use cases with less machinery.

The `get_integral_result` function handles three cases (contraction, identity, expansion) for integer results and a fourth for array-like results. This complexity is justified by the need to bridge between arbitrary `result_type` widths and `std::size_t`.

The five `hash_append_*` variants (`hash_append`, `hash_append_range`, `hash_append_size`, `hash_append_range_and_size`, `hash_append_unordered_range`) are individually justified but collectively form a function family that the user must understand for custom type support. `hash_append_range_and_size` exists purely as a composition of `hash_append_range` + `hash_append_size`, which could have been left to the user.

**Evidence:** `hash_append.hpp` 12 dispatch cases; `tag_invoke` with `Provider` parameter; 5 `hash_append_*` variants; `get_integral_result` 4 cases.

---

### 13. Documentation Quality — ⚠️

**Score: 2/3**

The AsciiDoc documentation is well-organized into hashing bytes (concept, algorithms, guidance), hashing objects (dispatch rules, type categories, customization), examples (progressive container adapter, md5sum, compile-time, HMAC), and reference (per-header pages with effects/returns clauses). The "Choosing a Hash Algorithm" section provides practical guidance with security considerations. The reference pages specify effects, returns, and requires clauses for every public function - thorough contract documentation. The decision to keep documentation external rather than inline in headers is sound for a library in a large ecosystem, avoiding rebuild cascades when docs are edited.

The remaining gaps are organizational:

- **Reference pages are organized by header, not by task**: A user who wants to know "how do I hash a string for an unordered map" must piece together information from `hash_append.adoc`, `get_integral_result.adoc`, and `examples.adoc`. There is no task-oriented "How To" section.
- **The examples section builds one adapter five times**: Five of the eleven examples are progressive refinements of the same unordered-container adapter class. This is pedagogically sound but means the examples section is dominated by container integration rather than demonstrating the library's breadth.
- **No "Getting Started" quickstart**: The documentation opens with the `hashing_bytes` section, which begins with the `HashAlgorithm` concept requirements. A user who wants to compute an MD5 checksum must read through the concept specification before finding a concrete example. The examples section is separate and later in the document.

**Evidence:** `doc/hash2.adoc` table of contents: Overview, Hashing Bytes, Hashing Objects, Examples, Implementation, Reference; no quickstart section; reference organized per-header.

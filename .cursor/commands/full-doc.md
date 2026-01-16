---
description: Generate comprehensive user-facing documentation for a C++ library
---

# Document a C++ Library

Generate complete user-facing documentation following a structured process.

## Prerequisites

1. **Documentation rule file** - Defines style, tone, formatting. If not provided, ask for it immediately.
2. **Library source access** - `include/`, README, examples, build scripts.
3. **Format** - Use existing format if present; default for Boost: Antora (guides) + Mr. Docs (API reference). See `capy/doc/` for reference.

---

## Step 1: Discover Public Interfaces

- Scan `include/boost/<library_name>/` for `.hpp` files
- **Exclude**: `detail/`, `impl/` directories and `detail` namespaces
- Fallback: Check README or examples for `#include` lines

## Step 2: Generate Introduction Page

Create `intro.adoc` with:

1. Title and one-sentence summary
2. What This Library Does (bullet points)
3. What This Library Does Not Do
4. Design Philosophy
5. Requirements (C++ standard, compilers, dependencies)
6. Quick Example (minimal working code)
7. Next Steps (links)

## Step 3: Determine Top-Level Sections

- Identify functional units; generate 2-4 partitioning options
- Evaluate: singular focus, skippable sections, matches user mental model, minimal dependencies
- Section count: 1 (single utility) → 5-9+ (large framework)
- If unclear, present options to user

## Step 4: Light Fill Each Section

Complete ALL sections before proceeding. Each section follows:

1. **Opening** - What will reader accomplish?
2. **Start simple** - Most common use case with working code
3. **Build progressively** - Simple → complex, call out surprises
4. **Conclusion** - Summary + bridge to next section

Mark gaps with `// TODO: expand`.

## Step 5: Full Fill Each Section

- Add more examples covering edge cases
- Verify simple → complex progression
- **Introduce before referencing** - No forward references to unexplained concepts
- Checklist: flow, code accuracy, domain coverage, completeness

## Step 6: Add Supplementary Pages

- **Concepts** - C++20 concepts, semantic requirements
- **Glossary** - Domain terms not covered by API docs
- **Design Rationale** - Why decisions were made, trade-offs
- **References** - Papers, RFCs, related work

## Step 7: Final Review

- Read sequentially from intro to last page
- **30-minute test**: Can unfamiliar user understand (5 min), install (5 min), complete basic task (20 min)?
- Verify compliance with documentation rule file

---

## Key Principles

- **Terminology consistency** - Once named, never rename mid-document
- **Gotchas over happy-path** - Thread safety, lifetimes, platform quirks, performance cliffs
- **What? Why? How?** - In that order
- **Code for every concept** - Compilable, realistic examples
- **Error documentation** - What errors, how reported, what to do

## C++ Conventions

Begin with namespace admonition:

> Code snippets assume:
> ```cpp
> #include <boost/library.hpp>
> using namespace boost::library;
> ```

Javadoc: Briefs for functions returning values start with "Return". No `@tparam` for variadic args.

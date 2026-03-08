# Lessons

## Error comparison pattern (Capy)

**Never** compare `std::error_code` against `make_error_code(error::...)` directly.
Always compare against the portable error **conditions** from `cond.hpp`:

```cpp
// WRONG
BOOST_TEST(ec == make_error_code(error::canceled));
BOOST_TEST(ec == make_error_code(error::eof));

// RIGHT
BOOST_TEST(ec == cond::canceled);
BOOST_TEST(ec == cond::eof);
```

The `error` enum values are implementation details. The `cond` enum
values are the public contract callers compare against.

This applies everywhere: tests, javadocs, examples. In javadocs,
never say "yields `error::eof`". Say "yields an error that compares
equal to `cond::eof`" or just "yields `cond::eof`".

## Physical design terminology (from Lakos review)

**Never fabricate quotes.** If you cannot find the exact source for
a quote, do not attribute it. Lakos caught a fabricated quote
("strict knowledge hierarchy arranged in as flat as possible a
shape") that he never wrote. Always have a page number or URL.

**Insulation vs. isolation.** In physical design, "insulation" is a
reserved term meaning Pimpl - hiding implementation details from the
client's compiler to avoid recompilation (Lakos'96 Ch. 6).
"Isolation" means keeping packages separate so they can be tested
and deployed independently. Do not conflate them.

**Levelization is about acyclic dependencies, not granularity.**
Levels organize the implied dependencies of logical entities so
that component dependencies are acyclic. They are not a means of
achieving fine-grained components. The genesis: Parnas (1972) for
encapsulation, Dijkstra (1968) for acyclic dependencies, Myers
(1978) for documented interface boundaries.

**No cycle = not necessarily good.** The presence of a cycle means
the design IS broken. The absence of a cycle does not mean the
design IS good. Do not claim the converse of a red flag is a green
flag.

**Do not lecture experts on their own work.** When citing authorities,
apply their principles - do not explain those principles back to
them as if they are new. Assume the reader knows the foundations.

**Things that change together should not be separated.** The
colocation principle cuts both ways. Separate things that change
for different reasons. Keep together things that change together
(private classes, subordinate components).

## PRISM mandatory protocol compliance

**Never rationalize past a process gate.** When a framework document
specifies a mandatory protocol with explicit phase gates (e.g. PRISM
E.1: sufficiency check -> plan file -> operator review -> BUILD ->
generation), follow it even when the operator's request contains
enough detail that the plan "seems redundant." The plan is not for
the AI's benefit - it is the operator's review surface. A detailed
request makes Phase 1 faster; it does not eliminate Phase 2.

Common rationalizations that led to protocol violation:
- "The operator already gave me everything I need"
- "The output quality will speak for itself"
- "Being helpful means producing output fast"

The fix: when about to generate a gated artifact, check whether the
gate was passed. If not, stop and follow the protocol.

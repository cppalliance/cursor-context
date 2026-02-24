# Ivan Yanakiev: HFT Use Case & Corosio/Capy Requirements

**Date:** Feb 13, 2026
**Sources:** Slack conversation (6:37-8:03 AM Pacific) + Huddle (8:02-8:13 AM Pacific)
**Participants:** Vinnie, Steve Gerbino, Ivan Yanakiev, Michael Vandeberg

---

## Executive Summary

Ivan Yanakiev is a potential Corosio/Capy adopter running an HFT system built on 300KLOC of heavily customized ASIO. He's considering migration for three reasons: compile times, readability/maintainability (onboarding is hard), and performance. His architecture is a classic HFT pipeline: IO thread reads from sockets (multicast market data), immediately forks compute work (pricing graphs, XGBoost, neural nets) to a separate pinned CPU core, then writes signals back via IO. The IO thread must **never block**. Data moves between stages exclusively through **lock-free SPSC/MPMC shared-memory queues**. His P99 latency targets are 1.5us for market data and ~1us for order entry. Ivan referenced **TooManyCooks** as the gold standard for async runtime integration with ASIO, and noted that HRT and Citadel use stdexec for exactly this IO-to-compute dispatch pattern. The key message: Corosio must be either an extensible IO library that integrates cleanly with a broader async runtime, or it must provide its own cohesive IO + compute executor model. A pure IO library that can't dispatch to compute pools won't be adopted by this audience.

---

## Ivan's Background & Motivation

- **Current stack:** 300KLOC+ ASIO codebase with custom async compute pools, custom protocols
- **Pain points with ASIO:** compile times, hard to onboard new devs, ASIO backends not extensible without forking and depending on `detail/` headers
- **What would make him switch:** performance parity or better, extensibility to hook DPDK user-space network stacks without forking, clean IO/compute separation
- **Availability:** full-time job + near full-time own business; could try porting websocket or FIX code in ~1 month
- **Time zone:** works 7PM-4AM CET on personal projects; available weekends/evenings

---

## Ivan's Architecture (Reconstructed)

```
Multicast/Network (ASIO or DPDK)
        |
        v
  IO Executor (never blocks, busy poll on dedicated core)
        |
        |  fork / resume_on(compute_executor)
        v
  Compute Executor (pinned CPU core)
        |
        |  runs pricing graph (XGBoost, neural net, FPGA)
        v
  Signal produced
        |
        |  resume_on(io_executor)
        v
  IO Executor sends signal (TCP to OMS, risk manager, etc.)
```

### Key Details

1. **Component framework:** CRTP-based, zero-overhead. Each component has schema-defined inputs (e.g. protobuf). Components are small and composable.

2. **Graph construction:** Traders/quants define compute graphs via an API. A **graph optimizer** compiles these into an efficient callable. The graph executes when a market data message arrives.

3. **Executor model:** The async runtime must support:
   - `async_read` on the IO executor
   - `resume_on(compute_executor)` to dispatch the graph
   - `resume_on(io_executor)` to return and send signals
   - `fork` / `spawn` to avoid blocking the IO loop while the graph runs

4. **Data transport:** Only lock-free shared-memory queues (SPSC, SPMC, MPMC). No channels, no Go-style abstractions. Raw lock-free queues are the only mechanism.

5. **Compute targets:** CPU (XGBoost trees), GPU (neural networks), FPGA - the execution model must be flexible enough to dispatch to any of these.

6. **Migration path:** Companies are NOT rewriting their ASIO networking. They wrap ASIO behind sender/receiver abstractions and continue using it. The stdexec model layers on top of existing IO infrastructure.

7. **Market data path:** Read from socket, do compute, read from socket again. No multicast in the hot path - proxies read from shared memory and handle UDP multicast separately. Market data processors should be as fast as possible and only do IO for exchange data. Busy poll on a dedicated core, then offload processing to pinned CPU core.

8. **Latency targets:** Market data P99 = 1.5us (throughput + latency combo). Order entry = ~1us or below (pure latency, very hard to achieve).

---

## Ivan's Key Concerns About Corosio

1. **Is the angle ergonomic IO with some perf sacrifice, or full async runtime + IO?** This is the central question. If it's just ergonomic IO, HFT won't adopt it.

2. **Extensible backends without forking.** ASIO requires forking and depending on `detail/` headers to provide a custom backend (e.g. DPDK). Corosio must not have this problem. People doing DPDK don't care about POSIX API compliance or full RFC compliance - they want to squeeze maximum hardware IO performance.

3. **IO + compute must be cohesive.** Two separate frameworks for IO and execution that don't integrate = nobody will use it. Either Corosio provides the full async runtime, or it must be extensible enough that an external compute runtime (like TooManyCooks) can merge with it into one cohesive unit.

4. **CPU affinity and executor control.** TooManyCooks provides "incredible support for cpu affinity and control in an intuitive and native to the executor manner api." Corosio needs something comparable.

5. **No synchronization between IO and compute executors.** This is how TooManyCooks handles it - zero sync overhead when switching between IO and compute.

---

## Industry Context (from Ivan)

- **HRT (Hudson River Trading):** Trades trillions using stdexec approach. CppCon engineer explained precisely how they do IO-to-compute dispatch.
- **Citadel:** Migrated an entire asset class to stdexec ~6 months ago.
- **TooManyCooks:** Ivan's preferred async runtime. Has ASIO support, integrates switching between CPU and IO work, has optimizations for Clang HALO that Corosio could potentially adopt. Ivan clarified TooManyCooks is **not** a direct competitor to std::execution - its algorithms are not as extensible or composable. What Ivan likes about it: CPU topology awareness, Clang HALO optimizations, and specific algorithms (fork, fork group, post bulk). He started using it recently and is still evaluating customization points.
- **stdexec / sender-receiver:** The HFT community is adopting this model. It works well for the IO -> compute -> IO pipeline pattern. Ivan doesn't personally use it (prefers TooManyCooks) but understands why the community likes it. Ivan believes std::execution has no real competitor as an abstraction - it is very extensible and its algorithms are composable, allowing users to easily create new ones.
- **Citadel and std::execution:** Herb Sutter was hired as fellow at Citadel and pushed for std::execution adoption. Citadel implemented their own (likely standard-conformant) version and migrated an entire asset class to it. Ivan's take: Citadel's engineers would not sacrifice PnL (profit and loss) for ideology - if they adopted it, it must actually work. This is meaningful signal.
- **Does standardization matter?** Vinnie questioned whether being in the standard matters for HFT, since HFT firms do not share libraries or download algorithms from GitHub. Ivan agreed that standardization per se does not matter - Citadel built their own. What matters is the abstraction itself and its implications for ecosystem building. stdexec provides a "standard way to do things" that influences what gets built on top of it, which is why inclusion in the standard is important even if individual firms roll their own conformant implementations.
- **Golang comparison:** If performance isn't the differentiator, people will just write a Go backend for 50-70k req/s with better ergonomics than anything C++ offers.

---

## Requirements for Capy & Corosio

### What Ivan's use case demands from the async runtime:

| Requirement | Description | Library |
|---|---|---|
| Multi-executor dispatch | `resume_on(executor)` to move coroutine between IO and compute pools | Corosio |
| Fork / fire-and-forget spawn | Spawn graph computation without blocking the IO read loop | Corosio |
| Lock-free queue integration | SPSC/MPMC queues as the data transport between pipeline stages | Capy |
| Never block IO thread | IO loop must be free to read next multicast message immediately | Both |
| Zero-overhead components | CRTP-based component framework with schema inputs | User code (but Capy channels could serve as the glue) |
| Flexible compute dispatch | Route work to CPU thread pool, GPU, or FPGA | Corosio executor model |
| Extensible IO backend | Hook DPDK or custom user-space stack without forking or detail headers | Corosio |
| CPU affinity control | Pin executors to cores, NUMA-aware scheduling | Corosio |
| ASIO interop | Wrap existing ASIO code to participate in multi-executor pipelines | Corosio |

### Specific Technology Gaps to Address:

1. **`resume_on(executor)` primitive in Corosio** - Coroutine must be able to cheaply switch which executor it runs on mid-flight. This is the most critical building block.

2. **Fork/spawn that returns to caller immediately** - The IO loop spawns the graph onto the compute executor and immediately resumes reading. The spawned coroutine runs independently on the compute pool.

3. **Lock-free queue adapters in Capy** - The channel abstraction needs to support (or wrap) lock-free SPSC/SPMC/MPMC queues as the underlying transport. These are the ONLY data movement mechanism in HFT pipelines.

4. **Backpressure / buffering** - Ivan mentioned buffering mechanisms for when the compute pool falls behind. The channel/queue abstraction needs configurable backpressure policy.

5. **ASIO interop layer** - Companies won't rewrite networking. Corosio must interoperate cleanly with existing ASIO-based IO code, potentially wrapping it behind a sender/receiver interface.

6. **Pluggable IO backend** - Must support custom backends (DPDK, user-space stacks) through public API, not `detail/` headers.

7. **Clang HALO optimizations** - TooManyCooks has these; Corosio should research and potentially adopt similar techniques.

---

## Connection to Earlier Huddle (7:40-8:02 AM)

This conversation confirms the hypothesis from the earlier session:

- The **external user with DPDK/user-space sockets** has the same architectural pattern as Ivan
- Their **P99 latency concern is about interior pipeline channels** (Capy's lock-free queues), not Corosio's networking layer
- The **pump mechanism** discussion is relevant to the IO executor side (throughput vs latency tradeoff for the read loop)
- The **frame recycler / `when_all` optimizations** matter for the compute dispatch side (fan-out of graph nodes)

---

## Development Plan Inputs

### Phase 1: Core Executor Primitives (Corosio)
- [ ] Implement `resume_on(executor)` for cross-executor coroutine migration
- [ ] Implement `fork` / fire-and-forget spawn onto a different executor
- [ ] Ensure the IO executor never starves when compute work is dispatched
- [ ] Validate with benchmark: multicast read loop + compute dispatch, measure whether IO thread stays saturated

### Phase 2: Lock-Free Queue Integration (Capy)
- [ ] Design channel adapter for lock-free SPSC queues
- [ ] Design channel adapter for lock-free MPMC queues
- [ ] Implement configurable backpressure policy (drop, block, buffer)
- [ ] Benchmark queue throughput under Capy's async model vs raw queue access

### Phase 3: Pipeline Composition
- [ ] Enable pipeline patterns: read -> dispatch -> compute -> return -> send
- [ ] Ensure the full pipeline can be expressed with Capy channels + Corosio executors
- [ ] Test with realistic HFT-like workload (high message rate, lightweight compute)

### Phase 4: Extensibility & Interop
- [ ] Pluggable IO backend API (support DPDK without forking or detail headers)
- [ ] ASIO interop: wrap ASIO operations behind Corosio's executor model
- [ ] Allow existing ASIO code to participate in multi-executor pipelines
- [ ] CPU affinity / NUMA-aware executor pinning
- [ ] Research Clang HALO optimizations from TooManyCooks

---

## Key Quotes

> "For me performance and low latency is the most important thing. If I see that corosio can bring that or ensures that it's extensible enough to hook a DPDK user space network stack without forking the library or depending on detail headers, I would consider it strongly." -- Ivan

> "If you have 2 separate frameworks for io and execution then it's not great and people will not use it unless the library is extensible enough so that the 2 can be merged into one cohesive unit." -- Ivan

> "People that will bother to do DPDK user space network stack are the same people that don't want LD_PRELOAD because you don't care that much about posix api or full rfc compliance you want to squeeze the maximum of the hardware in terms of IO." -- Ivan

> "If you bother to write network IO in c++ you have a very good reason for it and it's performance. Otherwise I'll just write a golang backend and will easily do 50-70k requests per second and it will be more ergonomic than anything c++ can offer." -- Ivan

> "You cannot afford on every message to pass it into the computer graph and block the IO thread." -- Ivan

> "Only shared memory queues. Lock-free SPSC or MPMC queues. That's the only way data is moved around." -- Ivan

> "They don't rewrite [ASIO networking]. They just wrap it and continue to use stdexec from now on." -- Ivan

> "We want to design Capy to address this use-case so there is an alternative to senders and receivers (which I hate)." -- Vinnie

> "High performance, HFT, compute - all of this is in-scope for Capy and Corosio. We aim to outperform everything." -- Vinnie

> "socket read -> parse, then -> serialize into shm queue + normalized protocol back into shared memory is done in one go as a zero copy operation in shared memory queue which is neat for performance ... after the socket read is done you should never ever block ideally you move to cpu executor and schedule another read again" -- Ivan

> "extensible backend without forking the library users should be able to add backend like dpdk without forking and be able to use the rest of your library or maybe they want their own epoll because they deem your to be too slow" -- Ivan

> "type erasure ... I fear this will be the biggest concern of a rather large and important community looking into these libraries. In my day job I can tell you there is not a single class which uses type erasure or virtual functions. Everything I MEAN EVERYTHING is CRTP all the way down sometimes with 5 layers dep hierarchy" -- Ivan

> "there are even implementation details which if corosio gets right will win on performance on asio they just need to be highlighted well, a few more APIs which are important like receiving multiple messages from UDP very important and saves a lot on syscalls recvmmsg()" -- Ivan

> "a low level abstraction layer which puts the absolute bare minimum between the file descriptor and the calling code" -- Vinnie (defining Corosio's value)

> "I do not think std::execution has any competitor at the moment. It's very interesting and extensible abstraction. The algorithms are composable and users can easily create new ones." -- Ivan

> "std::execution is just a lot of type masturbation to compose types and objects correctly thus even if it's in standard I do think it makes any guarantees for performance this is left to the users which is good. It just provides the standard way to do things from now on it has implications for ecosystems on what to build on top of it so that's why it's important that it will be in the standard" -- Ivan

> "If my year end bonus depends on performance I'll not cut my paycheck because someone said std execution is better I'll prove it and then implement it within a major asset class." -- Ivan

> "HFT/finance do not share libraries. You don't download HFT algos from github. So what is the need for interop? A third-party stdexec is just as good as std::execution." -- Vinnie

---

## Feb 14 Follow-Up: SPSC Queue Deep Dive & Architecture Clarification

### Queue Saturation (Slack, Feb 14)

Vinnie asked Ivan how saturated his SPSC shared-memory queues are (how often empty or full).

- Ivan's queues use a **load factor** metric exposed through Prometheus
- SPMC with 32 consumers: easily above 500k msg/sec
- With 1/2/4/8 consumers: millions of msg/sec, faster than Boost concurrent containers, moodycamel, and Aeron
- Crypto exchange saturation depends on venue: MEXC spot = 25-35k msg/sec, options + futures + spot estimated 90-110k msg/sec
- Average exchange message size: 223 bytes
- Queues are **byte buffers in shared memory with specific layout** where bytes are serialized, not typed template containers

### Coroutines Do Not Fit the Hot Path

Ivan confirmed: **coroutines do not fit into the shared-memory queue pipeline**. The compute pipeline is pure spin loops on dedicated cores. No coroutines, no event loop, no suspension/resumption.

### Inbound Data Pipeline (Ivan, Feb 14)

Ivan clarified his actual path:

> socket read -> parse -> serialize into shm queue + normalized protocol back into shared memory

The parse and serialize-to-queue steps are done **in one go as a zero-copy operation in shared memory**, which is good for performance. After the socket read is done, the IO thread should never block - ideally it moves to a CPU executor and schedules another read immediately.

The earlier description (5 discrete steps) was an idealization. In practice, parse + serialize-to-queue are fused for zero-copy. The key constraint remains: after socket read completes, never block.

### Main Loop Architecture (Ivan, Feb 14)

Ivan shared his actual main loop code:

```cpp
class loop_timer {
    uint64_t loop_timer_freq_cycles_;
    uint64_t next_loop_update_cycles_;
public:
    explicit loop_timer(std::chrono::nanoseconds frequency)
        : loop_timer_freq_cycles_(tsc_clock::nanoseconds_to_cycles(frequency.count()))
        , next_loop_update_cycles_(tsc_clock::get_cycles() + loop_timer_freq_cycles_) {}

    bool should_poll_io_context() const {
        return tsc_clock::get_cycles() > next_loop_update_cycles_;
    }

    void execute_poll(boost::asio::io_context& ioc) {
        uint64_t now = tsc_clock::get_cycles();
        if (now > next_loop_update_cycles_) {
            next_loop_update_cycles_ = now + loop_timer_freq_cycles_;
            ioc.poll();
        }
    }
};

class loop_executor {
public:
    static void execute_iteration(loop_timer& timer, idle_strategy& idle,
                                  queue_poller& poller, boost::asio::io_context& ioc) {
        timer.execute_poll(ioc);   // drain completions (non-blocking)
        idle.idle();               // _mm_pause or busy poll
        poller.poll_all();         // poll shared-memory client queues
    }
};
```

Key insight: `ioc.poll()` is called at a **TSC-controlled frequency** to balance keeping the path hot vs. minimizing syscall overhead. The loop knows roughly how often messages arrive (e.g. every 50us) and tunes the poll interval accordingly.

### Where Corosio Adds Value (Vinnie, Feb 14)

Vinnie defined Corosio's value proposition for this audience:

> A low-level abstraction layer which puts the absolute bare minimum between the file descriptor and the calling code.

Corosio's responsibility lies at the socket read boundary. Everything after that (parse, serialize, queue push) is user code running on the IO or compute executor.

### Ivan's Adoption Concerns (Feb 14)

Ivan listed specific concerns that HFT engineers will raise:

1. **Extensible backend without forking** - Users must be able to add backends like DPDK without forking. They may also want their own epoll implementation if they deem the default too slow.

2. **UDP multi-message receive (`recvmmsg`)** - POSIX allows receiving more than one UDP message per syscall via `recvmmsg()`. This was never implemented in ASIO and would be a significant win for Corosio. Very important for reducing syscall overhead on market data feeds.

3. **Type erasure skepticism** - This will be the biggest concern from the HFT community. Ivan's day job codebase has zero virtual functions and zero type erasure. Everything is CRTP, sometimes 5 layers deep, combined with traits classes for type passing. Ivan personally believes type erasure can work if implemented well, but the skeptical audience will never consider it without extensive benchmarking proving that the specific implementation (small buffer optimization, static vtables, etc.) performs comparably to CRTP-based hierarchies for compile-time types.

4. **Internal data structure quality** - ASIO uses slow data structures internally (`std::deque` with bad allocation patterns, `std::unordered_map` which is slow). Corosio should use better alternatives (`boost::unordered_flat_map` for cache locality, etc.). Getting implementation details right and highlighting them is a competitive advantage over ASIO.

Ivan's summary: correct and optimized implementation details + better APIs (like `recvmmsg`) + a strong benchmark suite proving design choices do not sacrifice performance for ergonomics = path to adoption.

### Corosio Layered Architecture (Vinnie, Feb 14)

Vinnie described Corosio's refactored layered design that addresses the type erasure concern:

**Layer 1 (highest) - Fully type-erased streams:**
- Nothing visible, fast compile times, ABI stability
- e.g. `io_stream&` - runtime polymorphism, virtual dispatch, no inlining

**Layer 2 - Concrete types hiding the platform reactor:**
- Nothing visible, fast compile times, ABI stability
- e.g. `tcp_socket&` - one level of indirection, no inlining

**Layer 3 (lowest) - Concrete types tied to the reactor:**
- Including these headers makes the entire implementation visible
- Full function inlining, no virtuals, no indirection
- e.g. `epoll_tcp_socket`, `iocp_tcp_socket`, `io_uring_tcp_socket`, `dpdk_tcp_socket`

Users choose the tradeoff at each level. An inheritance hierarchy, thoughtfully layered, where the user controls performance vs. abstraction:

```cpp
// Maximum performance: full inlining, direct OS calls
epoll_tcp_socket sock(ctx);

// Can still pass to type-erased interfaces when needed
void do_something(tcp_socket& sock);  // one indirection
void do_generic(io_stream& sock);     // virtual dispatch
```

Algorithms can be written entirely with templates (like ASIO), accepting any concrete socket type. Pass `dpdk_tcp_socket&` and get DPDK performance. Pass `io_uring_tcp_socket&` and get full inlining with io_uring. Pass `tcp_socket&` and pay one indirection. Pass `io_stream&` and get full runtime polymorphism.

### Implications for Capy/Corosio

1. **Capy's value is NOT in the queue itself** - raw SPSC/MPMC queues are commodity; Ivan's are already faster than all open-source alternatives
2. **Capy's value is on the NIC boundary** - the IO-to-compute transition: read from network, deserialize via zero-allocation coroutine, push to queue
3. **The queue write is I/O** - serializing to shared memory or fanning out to sockets should be treated as I/O, potentially on a separate executor
4. **No coroutine wrapper needed for queues** - the compute side spins on `try_pop`/`try_push` directly; adding coroutine overhead would be counterproductive
5. **Lock-free SPSC queue in Capy** - useful as a building block and for the IO-side handoff, but should remain a raw `try_push`/`try_pop` interface without async wrappers in the hot path
6. **Corosio's layered architecture directly addresses the type erasure concern** - HFT users work at layer 3 with zero overhead; library authors and general users work at layers 1-2 with ergonomics

### SPSC Queue Implementation (Feb 14)

Implemented `spsc_queue<T,N>` in Capy as a first step:
- Header: `include/boost/capy/queue/spsc.hpp`
- Tests: `test/unit/queue/spsc.cpp`
- Lock-free, cache-line-separated cursors, power-of-2 capacity, trivially-copyable T constraint
- All tests pass including threaded stress tests

### Open Questions from This Session

- Should the queue also have a byte-buffer variant (variable-length messages, memcpy-based) matching Ivan's "bytes are serialized" model?
- What does the IO-side coroutine look like that reads from the network and pushes to the queue? This is where Capy/Corosio adds value
- How does `resume_on(executor)` interact with the queue push at the end of the inbound pipeline?

---

## Follow-Up

- [ ] Interview Ivan in depth (Steve to conduct, Vinnie to extract report from recording)
- [ ] Get link to CppCon talk Ivan referenced (HRT engineer, ASIO-to-stdexec migration)
- [ ] Review TooManyCooks library for design inspiration (ASIO integration, HALO optimizations, executor affinity API)
- [ ] Assess current Capy channel implementation against lock-free queue requirements
- [ ] Review Corosio executor model for `resume_on` feasibility
- [ ] Ivan to try porting websocket or FIX code to Corosio (~1 month timeframe, no hard ETA)
- [ ] Research Citadel's stdexec migration for patterns/lessons
- [ ] Implement `recvmmsg()` support in Corosio UDP path
- [ ] Build type erasure vs. CRTP benchmark suite to address HFT community skepticism
- [ ] Audit Corosio internals for suboptimal data structures (deque, unordered_map) and replace
- [ ] Document Corosio's layered architecture (type-erased -> concrete -> reactor-specific) for HFT audience
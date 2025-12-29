# Code

- C++11 default; newer only when required (e.g., coroutines).
- APIs start small, widen deliberately
- `detail::` is internal, never public API
- Layout: `include/` (public), `src/` (impl + private), `test/`
- RAII scope guards over try-catch; dismissible for commit/rollback

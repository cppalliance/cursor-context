---
description: Add implementation notes to the top of a file
---

At the top of the file after the includes, put a nice /* */ section which gives a high level overview of how the implementation works. Be sure to point out especially anything tricky or non-obvious that a maintainer or LLM might need to know in order to make quick effective decisions to reason about the code base. Sparingly and judiciously add individual // comments which explain the why (not the what or how) when it is non-obvious.


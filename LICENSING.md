# Licensing

**Status: no `LICENSE` file is included yet, on purpose.** It needs one decision
from the copyright holder of the ShakeToLocate code (see below).

## What is known

- **Mango** is licensed **GPL-3.0-or-later** (Copyright 2025 DreamMaoMao; it also
  carries the notices of dwl, tinywl, dwm and sway, see Mango's `LICENSE*` files).
- `patches/0001-*.patch` modifies Mango source files and `src/input/shake_to_locate.c`
  is compiled into the Mango binary. The binary this installer builds is therefore
  a combined work and can only be distributed under GPL-3.0-or-later terms.
  Anything you publish that contains Mango code (the patch hunks include Mango
  context lines) must stay compatible with that.
- Nothing in this repository copies Mango source files. The patch contains only the
  small edits and their surrounding context lines.

## What is not known

- **Who holds the copyright of the original recovered ShakeToLocate code**, and
  under which terms it was shared. The ZIP contained no license, copyright or
  SPDX notice, and I did not assume one.
- The contributions made while turning it into this project (continuous scaler,
  configuration, integration patch, scripts, tests) were produced in a session
  with an AI assistant for the person who requested them; that person decides how
  to license them.

## Recommended next step

If the copyright holder agrees, release the project under **GPL-3.0-or-later**
(the same as Mango): add the GPLv3 text as `LICENSE`, and put
`SPDX-License-Identifier: GPL-3.0-or-later` at the top of the new source files.
Until then, treat the repository as "all rights reserved" and do not publish it
as open source.

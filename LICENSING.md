## Licensing

**Status:** No LICENSE file is included yet. The project is currently awaiting its initial license selection.

### What is known

- Mango is licensed under GPL-3.0-or-later (Copyright 2025 DreamMaoMao; it also carries notices from dwl, tinywl, dwm, and sway in its LICENSE files).
- ShakeToLocate is an independent project. The original ShakeToLocate code was provided by the project author, who owns the original code.
- The project adds new functionality and integration for the Mango Wayland compositor through patches and supporting scripts.
- `patches/0001-*.patch` modifies Mango source files, and `src/input/shake_to_locate.c` is compiled into the Mango binary.
- The resulting binary is a combined work and must comply with the applicable GPL-3.0-or-later terms.
- This repository does not copy complete Mango source files. It contains only the required patch changes and integration code.

### Project contributions

The project author developed the additional functionality, including the continuous scaler, configuration system, Mango integration, installation scripts, and tests, with assistance from an AI coding assistant.

The project author is responsible for choosing the license for the contributions they own, subject to the rights of any third-party code included in the project.

### Recommended next step

The project author can release their original code and contributions under GPL-3.0-or-later, provided all third-party components and modifications are compatible with that license.

Once the licensing decision is finalized:

1. Add the GPL-3.0-or-later license text as `LICENSE`.
2. Add `SPDX-License-Identifier: GPL-3.0-or-later` to the applicable source files.
3. Preserve all required copyright and attribution notices from Mango and other third-party components.

Until a license is added, the repository remains **All Rights Reserved** by default, except for third-party components governed by their respective licenses.

<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Contributions

Contributions are only accepted under the following conditions:

- Contributions must have a certified origin and grant us permission. To manage this process we use
  [Developer Certificate of Origin (DCO) V1.1](https://developercertificate.org/).
  To indicate that contributors agree to the terms of the DCO, it is necessary to sign off each commit
  by adding a line with name and e-mail address to every git commit message:

  ```log
  Signed-off-by: John Doe <john.doe@example.org>
  ```

  This can be done automatically by adding the `-s` option to your `git commit` command.
  You must use your real name, no pseudonyms or anonymous contributions are accepted.

- You give permission according to the [Apache License 2.0](./LICENSES/Apache-2.0.txt).

To make licensing and provenance clear, **every source file** contributed to this project must include SPDX metadata.
Prefer the short, machine-readable SPDX tags:

- `SPDX-FileCopyrightText: ...`
- `SPDX-License-Identifier: Apache-2.0`

**Do not** copy a C-style `/* ... */` block into files that do not support that comment syntax (Python, shell, YAML, JSON, etc.). Instead use the comment style appropriate for the file's language — examples below.

## Examples

**C / C++ / Java**
```c
//
// SPDX-FileCopyrightText: Copyright <years additions were made to project> <your name>, Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
  ```
**Python / bash / sh**
```python
#
# SPDX-FileCopyrightText: Copyright <years additions were made to project> <your name>, Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
  ```

**Markdown / text files**
```markdown
<!--
    SPDX-FileCopyrightText: Copyright <years additions were made to project> <your name>, Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->
```

## Contribution Workflow

All contributions must follow this process:

1. Fork this repository.

2. Create a new branch from the latest main branch.

3. Commit your changes to your fork.

4. Submit a pull request (PR) from your fork to this repository.

5. Ensure all native unit tests pass before requesting review.

Direct pushes to this repository are not permitted.

## Code Reviews

All contributions must go through code review.
Pull requests are reviewed by project maintainers through the public pull request discussion. In addition,
maintainers may perform internal validation and compliance checks as part of the review process.
A contribution will only be merged once it passes all required checks and receives approval from a maintainer.


---
name: build-amigagpt
description: This skill should be used when the user asks to compile or build AmigaGPT, mentions AmigaOS 3, AmigaOS 4, or MorphOS in a build context, or requests cross-compilation. Builds AmigaGPT for AmigaOS 3, AmigaOS 4, or MorphOS using the repo build scripts (build_os3.sh, build_os4.sh, build_morphos.sh). Do not attempt manual compilation; only run the provided scripts.
---

# Build AmigaGPT (AmigaOS 3 / AmigaOS 4 / MorphOS)

## Rule (hard guardrail)

- **Do not attempt to compile AmigaGPT directly** (no direct `make`, no ad-hoc toolchains, no alternate Docker invocations). In this repo, building is done via the provided scripts and other approaches will not work reliably in this environment.
- **Only build by running the correct script** from the repo root:
  - **AmigaOS 3**: `./build_os3.sh`
  - **AmigaOS 4**: `./build_os4.sh`
  - **MorphOS**: `./build_morphos.sh`

## Optional environment variables (supported by the scripts)

- **Clean build**: `CLEAN=1 ./build_os3.sh` (or OS4/MorphOS equivalent)
- **Debug build**: `DEBUG=1 ./build_os3.sh` (or OS4/MorphOS equivalent)

## Default behavior

When the user asks to build/compile:

1. Determine the target OS (**AmigaOS 3**, **AmigaOS 4**, or **MorphOS**).
2. Run the corresponding script from the repo root.
3. If the build fails, surface the script output and help troubleshoot **within the constraints of these scripts** (do not switch to manual compilation).

## Examples

**User**: "Compile the AmigaOS 3 build."

**Agent**: Run:

```bash
./build_os3.sh
```

**User**: "Do a clean MorphOS build."

**Agent**: Run:

```bash
CLEAN=1 ./build_morphos.sh
```

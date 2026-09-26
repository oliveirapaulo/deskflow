# Working COSMIC native Logitech keyboard-handoff milestone

This document is the replay guide for the **tested keyboard-only milestone**. It is separate from the proposed physical mouse-switching design. At this milestone, Deskflow's virtual mouse remains sourced from the Windows server; the MX Master 3 remains on Windows slot 2; and the MX Keys physically switches Windows slot 2 ↔ Pop slot 1. The current COSMIC relative-pointer patch improves hover focus but can drift under fast motion. Do not mistake this milestone for a finished physical-mouse implementation.

## What is in Git and what is not

The tagged commit contains the patched Deskflow source, optional native HIDAPI build flag, Windows x64 build workflow, Pop Fedora container build/test recipes, and this guide. It does **not** contain compiled executables, Docker images, pairing state, Deskflow TLS certificates/fingerprints, or private machine configuration. The Windows CI artifact is retained for only three days per run; it can be rebuilt from the tagged source. Preserve any particular binary separately if byte-for-byte recovery is required.

The native HID++ implementation is tested against the Logitech receiver `046d:c52b` with MX Keys as receiver device index `0x01`. The report format may apply to other compatible Logitech models, but this milestone does **not** establish model-independent support. The mouse report is not sent by this code.

## Tested topology and settings

| Computer | Deskflow role | Screen name | MX Keys slot | MX Master 3 slot |
| --- | --- | --- | --- | --- |
| Windows 11 PC | Server | `PAULO-PC` | 2 | 2, permanently |
| Pop!_OS COSMIC Wayland | Client | `pop-os` | 1 | Not used; mouse stays on Windows |

The Deskflow layout has `pop-os` to the **left** of `PAULO-PC`. These names are examples for the replay guide; each configured screen name must match the name on its actual machine. The Windows server must be started before the Pop client connects. The actual IP address is a local setting, not part of this milestone. Both sides must have TLS enabled and must approve each other's current certificate fingerprints; a newly generated server certificate needs a new trust confirmation on Pop. Do not copy private certificates or trusted-fingerprint databases into this public Git repository.

Windows portable `settings/Deskflow.conf` must contain the following in addition to the GUI-generated core, layout, and TLS settings:

```ini
[server]
onEnterScreen=pop-os
mxKeysHostOnEnterScreen=1
```

Pop's isolated test configuration must contain:

```ini
[client]
remoteHost=<current Windows server IP or hostname>
mxKeysHostOnScreenLeave=2

[core]
computerName=pop-os
```

The test launcher sets `mxKeysHostOnScreenLeave=2` in its **isolated copy** automatically. It does not edit the installed Flatpak's settings. These native settings are documented in `doc/dev/build.md`.

## Rebuild and run the Pop side

Prerequisites on Pop: Docker access, a COSMIC Wayland session, the Logitech receiver accessible through the matching `/dev/hidraw*` node, and network access for the first Fedora/DNF/CMake dependency fetch. Do not add the user to the broad `input` group. The launcher passes just the matched receiver node and its group into the unprivileged test container. If the device is inaccessible, diagnose its actual udev/logind permissions; do not run the whole client as root merely to make the test pass.

From a checkout of this milestone tag in the Deskflow repository:

```bash
bash tools/build-cosmic-native-container.sh
```

This creates `deskflow-test-fedora43:local` and compiles `build-native-hid/bin/deskflow` plus `build-native-hid/bin/deskflow-core` with `DESKFLOW_NATIVE_MX_KEYS_HANDOFF=ON`. It does not replace the installed Flatpak or any system binary. If another Deskflow process is running from `build-native-hid`, choose an alternative build directory, for example:

```bash
bash tools/build-cosmic-native-container.sh build-native-hid-replay
```

Before starting a new test, quit the existing Deskflow client process; do not let two clients compete for the same server connection. To launch the custom GUI with isolated settings:

```bash
bash tools/test-cosmic-client-container.sh --native-hid --gui
```

For the alternative build directory above:

```bash
DESKFLOW_TEST_BUILD_DIR=build-native-hid-replay \
  bash tools/test-cosmic-client-container.sh --native-hid --gui
```

On its first run the launcher copies existing Deskflow Flatpak settings into an isolated home **if those settings are present**. Otherwise it starts with a minimal client file; configure the Windows server address and `pop-os` name in the custom GUI, approve the displayed server fingerprint after verifying it against Windows, and start the client. Both GUI and core use the same isolated trust database in `~/.local/state/deskflow-cosmic-handoff/container-home-native/`. A terminal must remain open for this test mode. The runtime test image includes `wl-clipboard` for the text clipboard bridge.

The same build can be run as a direct core client after GUI configuration:

```bash
bash tools/test-cosmic-client-container.sh --native-hid
```

Press Ctrl+C in the launching terminal to end the test. Clicking the GUI close button may only hide it in the tray.

## Rebuild and run the Windows side

The tracked `.github/workflows/windows-native-mx-keys.yml` builds a Windows x64 portable package using `DESKFLOW_NATIVE_MX_KEYS_HANDOFF=ON`. Pushing a tag named `cosmic-native-logi-keyboard-working-*` to the fork triggers that workflow at the tagged source commit. Download the `deskflow-native-mx-keys-windows-x64` Actions artifact, then unpack the contained `deskflow-*-win-x64-portable.7z` into a distinct directory. The uploaded artifact expires after three days; rerun the workflow on the same tagged revision for a fresh package if needed. The source and workflow remain in Git.

In the Windows GUI, choose Server, name the computer `PAULO-PC` (or another name used consistently in the layout), put `pop-os` immediately to its left in the layout, enable TLS and clipboard if desired, and save. Quit Deskflow before editing `settings/Deskflow.conf` to add the `[server]` block above; relaunch and click **Start**. In the tested GUI config, `startCoreWithGui=false`, so merely opening the GUI does not start the server. Input Leap or another service must not already own TCP port 24800. Approve the Pop client fingerprint when prompted.

The legacy Input Leap service can retain port 24800 even after its tray UI is stopped. Check Windows Services if Deskflow reports `cannot bind address: [10013]`. Do not delete an existing installation as part of this replay.

## Milestone acceptance and limits

The milestone is live-confirmed when Windows → Pop changes MX Keys to slot 1 and Pop → Windows changes it to slot 2, with typing working on both machines and the MX Master remaining on Windows slot 2 throughout. Plain-text clipboard was observed both directions. Images, rich clipboard formats, and files were not established. COSMIC hover focus works with the patched virtual pointer, but rapid mouse movement may make its logical and visible coordinates drift; the proposed physical-mouse branch is intended to investigate this without modifying the working milestone.

Do not disable the old Waynergy/edge-watcher rollback setup merely because the milestone builds. Inventory the currently running services before making any startup or migration changes.

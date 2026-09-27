# Physical Logitech handoff for Deskflow — project specification

Status: **proposed design; not implemented**. Written 2026-09-26 from the Pop!_OS/Windows handoff discussion. This document is intended to be sufficient context for resuming the project in a new session. It describes the desired next phase, not a claim that the current build already supports it.

Gate-1 update: the installed COSMIC session does not expose the InputCapture portal needed by the preferred physical-edge design. See [the local feasibility report](gate1-cosmic-input-capture-feasibility.md) before implementing client-side capture or choosing a fallback.

## 1. Purpose and change from the working baseline

Build an opt-in Deskflow mode in which a multi-host Logitech mouse and/or keyboard physically changes its Easy-Switch host when the pointer crosses between Deskflow computers. Deskflow remains the secure connection, layout, routing, clipboard, and handoff coordinator. The active computer's operating system handles its *locally connected* mouse and keyboard natively.

The original project brief required **keyboard-only** switching: MX Keys moved between hosts, while MX Master 3 stayed on Windows slot 2 and acted as Deskflow's physical input source. That has been demonstrated working in both directions. It is now a fallback/baseline, **not** the target for this phase. The reason for changing direction is that the COSMIC client currently converts Deskflow's absolute target positions into relative libei pointer motion to make focus-follows-cursor work. Rapid movements can make Deskflow's logical position and COSMIC's visible cursor diverge; the server can think an edge has been reached while the visible cursor remains roughly 50 px away. With the mouse physically connected to Pop, COSMIC should handle its movement and hover focus directly, without continuous virtual mouse injection.

The user wants the mouse to switch too, while retaining the option for another user to switch only a mouse, only a keyboard, both, or neither. This is a change to Deskflow's normal assumption that the server owns the physical input devices for the whole session.

## 2. Known environment and project state

- Repository: `https://github.com/oliveirapaulo/deskflow`, branch `cosmic-keyboard-handoff`; upstream `https://github.com/deskflow/deskflow`.
- Relevant committed baseline: `adf3390a1` (`Add COSMIC handoff and native MX Keys switching`). The Windows x64 native CI build passed and was tested live with the Pop native/container test build. Local `tools/` is untracked and was intentionally not pushed.
- Current two-computer setup: Windows 11 PC (`PAULO-PC`) is the Deskflow server; Pop!_OS COSMIC Wayland `pop-os` is the client, placed to its left. The screen names in this public design are examples; actual Deskflow names must match local configuration. Pop has multiple displays (Dell external plus laptop); the original brief describes the row as Pop laptop, Pop Dell external, Windows external, Windows laptop display. Preserve real monitor geometry rather than assuming one display per computer.
- Confirmed: MX Keys switches Windows slot 2 → Pop slot 1 on entry and Pop slot 1 → Windows slot 2 on exit using native HIDAPI in Deskflow. The MX Master stays Windows slot 2 in the current build. Normal typing on both sides and multiple round trips were observed. Plain-text clipboard worked both directions; images, rich text, and files were not established.
- Confirmed: COSMIC's physical mouse/touchpad correctly changes focus on hover. Our virtual relative-pointer patch also changes hover focus, but it has the accuracy/drift concern above. Do not declare that issue resolved merely because ordinary movement appears smooth.
- This next phase must not delete the current working code, install a replacement, disable rollback services, or push changes as a side effect of writing this specification. The original Waynergy arrangement and its edge watcher were retained for rollback; their actual live status should be re-inventoried before any migration.

## 3. Desired computer layouts

The design must support a server anywhere in a row, not just at its center:

```text
Pop client  <->  PC Windows Server  <->  Other Windows client
Other Windows client  <->  Pop client  <->  PC Windows Server
```

The second case requires a **direct client-to-client logical transition routed by the server**. The mouse need not physically connect to the server on the way from one client to another. The server is the authority for layout and destination selection even while its own mouse is paired to another host.

The three illustrated computers consume all three Easy-Switch slots of a device that has only three. Slot assignments are per *physical device*, not shared between mouse and keyboard. Extra computers can still participate in ordinary Deskflow sharing, but automatic physical switching of one three-slot device cannot be promised on a fourth host.

## 4. User-visible configuration

The concept is one sharing-wide feature, **Enable Logi slot switching**, with per-computer destinations. It may be enabled while editing the server or a client, but must result in one consistent policy, not conflicting local booleans. The server's layout/configuration should be authoritative; if remote-client GUI editing is supported, it must synchronize and validate against that policy. The exact persistence and editing protocol remains a design decision.

For each configured computer, including the server:

```text
Extra options
[x] Enable Logi slot switching  (shared feature state)
[ ] Switch mouse to     [1 | 2 | 3]
[ ] Switch keyboard to  [1 | 2 | 3]
```

Requirements:

1. Mouse and keyboard checkboxes are independent. An unchecked device is never commanded to change host merely because the other device is checked. Existing Deskflow input-sharing behavior remains available for unchecked devices, subject to an explicit design/test of mixed physical/virtual input.
2. A slot chosen for **mouse** on any computer is unavailable for mouse on every other computer. The same rule applies independently to **keyboard**. Mouse slot 1 and keyboard slot 1 may both be assigned to one computer, or separately if that reflects their actual pairings.
3. Slot values are 1, 2, or 3. The UI should explain that these are each device's Logitech host slots and that the device must actually be paired to the corresponding computer. Configuration validation must enforce uniqueness even if a settings file was edited manually.
4. Do not silently assign a free slot, change an existing pairing, or assume that two devices' slots identify the same host. Allow mouse-only and keyboard-only users without forcing the other control on.
5. A machine lacking the needed compatible, accessible Logitech device/receiver must not be presented as ready for a physical handoff. Error state and manual recovery must be visible.
6. Toggling the shared feature off must return to ordinary Deskflow behavior without deleting the saved slot map. Starting or reconnecting must not trigger an unsolicited host switch.

## 5. Ownership and movement model

When a device's physical switching is enabled, track that device's **expected current host** independently of the Deskflow active screen. They will normally agree for a switched mouse, but may differ for a keyboard-only/mouse-only configuration. A switch command must be issued from the computer to which that physical device is *currently connected*, because that computer can address its local Logitech receiver. Do not assume that the mouse's source computer also owns the keyboard.

For a physically switched mouse:

- While on an active host, the local operating system owns ordinary mouse movement, buttons, wheel, cursor location, and hover focus. Do not keep sending Deskflow's virtual motion into that host; this is the central fix for the Pop coordinate-drift problem.
- At an actual inter-computer edge, the active host reports a compact crossing event to the server (source computer, local display/zone, side, crossing coordinate, and transition identity). The server uses its existing screen-layout graph to choose the destination, including client-to-client routes.
- Only the crossing position and any needed handoff state travel over the network. Continuously streaming the local physical cursor location back to the server is *not* required for the proposed initial design. It might be needed for other features later, but must not be assumed available on Wayland.
- The computer currently holding each selected device sends it to its destination slot while that device is still local. This may be different from the pointer's source computer in a mixed mouse/keyboard configuration. The destination confirms presence if a reliable probe is available. Logical Deskflow ownership and physical device commands need an explicit, ordered transition protocol; the current fire-and-forget keyboard hook is not sufficient for the final mode.
- Screen entry uses an absolute position/edge coordinate to place the receiving cursor when possible. Once the physical mouse is local, stop injecting its subsequent motion. Reverting the Pop relative-motion patch alone is **not** sufficient: client-side physical edge detection and safe return switching must work first.

An unchecked device retains a separately defined fallback path. In particular, if the mouse is unchecked, Deskflow continues its conventional virtual mouse route; if the keyboard is unchecked, keyboard input must be tested in the selected fallback mode rather than implicitly switching slots. Mixed modes need explicit tests.

## 6. Edge detection on each platform

The server knows Deskflow's computer adjacency. Each active computer must detect crossings of **its own physical pointer** and ask the server to route them. The client must receive enough adjacency information to arm only meaningful outgoing edges, but the server still makes the final decision and handles disconnected neighbors, lock-to-screen, switch delays, and topology changes.

### Pop!_OS COSMIC Wayland client

- Use the user-authorized XDG Desktop Portal Input Capture interface. It exposes screen **zones** (logical rectangles) and pointer barriers on outside zone boundaries. An activation includes a barrier ID and cursor position. These give a crossing zone, side, and coordinate; they do not necessarily give a stable, user-facing monitor name.
- Build a stable barrier-ID-to-zone/side mapping for the current zone set. Do not confuse a seam between two Pop monitors with an edge leading to another computer. Support partial outer edges when monitors are offset or different sizes, and rebuild on zone/layout change. If a portal rejects a barrier, report that edge unavailable instead of guessing.
- The current `PortalInputCapture` path is constructed only for a **primary/server** `EiScreen`. Adapting it for a client is new work, including portal permission, arming only while appropriate, clean deactivation, and release at the correct destination/entry coordinate.
- The current barrier-generation code loops over zones, resets IDs for each zone, and has a `May not correctly handle different sized screens` comment. Do not reuse it unchanged for multi-monitor physical handoff.
- COSMIC/Wayland does not offer ordinary clients a general continuous global pointer-position query. The portal's edge activation is the intended first milestone; actual behavior on this COSMIC version must be tested.

### Windows client

- Use Windows' real cursor position and enumerated monitor rectangles to identify the current monitor and the relevant *outer* edge. Internal seams stay within Windows. Deskflow already has Windows cursor handling, but it is currently oriented around the server as the physical input owner; a client-side physical-input path is new work.
- Avoid treating Deskflow-injected/warped motion as physical edge movement, avoid polling races or accidental double crossings, and account for negative virtual-screen coordinates, differing monitor sizes/scales, and display changes.
- Windows need not use the COSMIC portal. Its client can send the same platform-neutral crossing request to the server.

The initial UI may treat a computer as one logical Deskflow screen while internally tracking its individual monitors. Per-named-monitor layout configuration is a separate future feature unless actual testing shows it is necessary.

## 7. Handoff protocol and failure behavior

Add an explicit, versioned capability/negotiation path; old Deskflow peers must keep using the legacy behavior rather than misinterpreting new messages. A proposed state sequence is:

```text
LOCAL_ACTIVE -> EDGE_DETECTED -> ROUTE_RESOLVED -> PREPARED
             -> SWITCH_SENT -> DESTINATION_ACTIVE
```

On a crossing request, the server validates that the source is active, the edge has a connected neighbor, the policy and slots are valid, no transition is already running, and the destination is reachable. It sends a transition plan with the destination and the slot for each enabled device. Each command is executed by the host currently holding that device. A successful HID write proves only that a report was sent, not that the peripheral reached the destination; the state model must distinguish **command sent**, **device observed**, and **unknown**.

Use a transition ID and debounce/timeout so one physical edge gesture cannot cause duplicate slot changes or a ping-pong. Define behavior when the mouse and keyboard switch at different speeds, a peer disconnects mid-transition, a receiver cannot be opened, the slot is unpaired, the destination lacks permission, or the user manually presses an Easy-Switch button. Do not report a completed handoff before the necessary evidence is present. If the local command fails, cancel when possible; if the device disappears but no destination confirms it, show a recovery instruction rather than silently guessing. Keep a way to recover using physical device buttons and the old working setup.

Security: only authenticated Deskflow peers may request/accept handoffs. A client cannot choose an arbitrary host slot or destination outside the server-approved configuration. Preserve narrow Linux HID access; do not add the user to the broad `input` group. Do not introduce plaintext or logfile parsing as the control channel.

## 8. Implementation starting points (not instructions to blindly reuse)

- `src/lib/platform/EiScreen.cpp`: current `fakeMouseMove()` converts server absolute coordinates to relative libei movement for COSMIC focus; `enter()` makes an absolute placement. This is the behavior to avoid in physical-mouse mode while retaining legacy mode.
- `src/lib/platform/PortalInputCapture.cpp`: existing primary-side portal zones and edge barriers; adapt carefully for secondary/client use and multiple displays.
- `src/lib/platform/MSWindowsScreen.cpp`: existing cursor position and server-side mouse movement logic; add an appropriate physical Windows client path.
- `src/lib/server/Server.cpp`: server screen adjacency, mapping, and `switchScreen()`; retain the server as router and authority.
- `src/lib/deskflow/MxKeysHandoff.cpp`: native HIDAPI proof of concept for MX Keys. It currently supports *only keyboard* reports and queues a fire-and-forget operation. Its current reports use Logitech receiver `046d:c52b`, vendor usage page `ff00`, usage `0001`, and MX Keys receiver device index `0x01`; slots 1–3 are encoded zero-based in byte 5. General mouse control, robust verification, and multi-receiver/device identification are new work. Do not guess mouse HID++ reports from keyboard reports.
- `src/lib/deskflow/ServerApp.cpp` and `ClientApp.cpp`: current narrow keyboard-only enter/leave hooks. Replace/extend them with the coordinated per-device policy; keep their behavior available until replacement is proven.
- `src/lib/gui/`: layout and server settings UI, including screen-specific options and validation.

The original Windows `switch_to_1.bat` demonstrated a separate mouse report, but that is historical evidence only; confirm the exact mouse model, receiver/path, host-slot payloads in both directions, and device-index mapping on every participating host before implementing native mouse switching. The HID++ command format may apply to other compatible Logitech models, but support for those models has **not** been established by the current two-device test.

## 9. Milestones and acceptance tests

1. **Preserve baseline:** record versions, configs, services, pairing map, logs, recovery commands, and backup/rollback path. Retest current keyboard-only switching. No early replacement of installed apps or services.
2. **Pop physical-mouse proof:** with a safe manual recovery plan, verify client-side portal can identify the correct outer edge and crossing coordinate across both Pop displays. Verify internal monitor seam never triggers handoff and offset/different-sized monitors behave correctly. Do this before removing relative motion for the new mode.
3. **Mouse HID proof:** independently verify slot 2 → 1 and 1 → 2 for MX Master 3 using native code from the currently connected host, without moving MX Keys. Confirm receiver identity, permissions, and failure reporting.
4. **Two-computer handoff:** make Windows ↔ Pop physical mouse crossings work with absolute entry placement, native COSMIC hover focus, accurate edge behavior under fast movement, and independent keyboard switching. Test keyboard-only, mouse-only, and both-device policies.
5. **Windows client + three hosts:** test Windows-client physical edge detection and both layouts in §3, including direct client-to-client routing, each direction, each participating outer edge, and no forced intermediate host connection.
6. **UI and persistence:** implement shared enable and per-computer independent slot controls. Validate same-device uniqueness, pairing/receiver readiness, disabled cases, manual edits, and synchronized configuration on reconnect/restart.
7. **Reliability:** at least 30 full crossings in each relevant direction; rapid movement, multiple monitors, clicks/scroll, disconnection/reconnection, suspend/resume, reboot, topology change, and failed HID/portal permission. No accidental double switch or permanently stranded device without an explicit recovery path.
8. **Migration only after success:** retain the old keyboard-only path and Waynergy setup as rollback until the physical mode is stable in everyday use. Package/autostart decisions are separate from proving core behavior.

Clipboard images/rich formats are deliberately **out of scope** for this physical-input phase; the known plain-text result should be preserved.

## 10. Open engineering questions to settle with prototypes

- Does this COSMIC portal implementation allow a Deskflow **client** to maintain the needed Input Capture session while Remote Desktop injection is also available, and does it return accurate barrier position for fast crossings?
- How should a physical-mode client receive server adjacency and react when a neighbor disconnects or the screen layout changes? Which protocol version/capability should gate the new messages?
- How should a transition be committed if the source device disappears immediately after sending its HID report? What evidence of destination arrival is available on Linux and Windows?
- How should a client GUI edit the single server-authoritative policy without introducing conflicting configs or allowing an untrusted peer to rewrite the layout?
- How does mixed physical/virtual input work when one of mouse/keyboard is unchecked, or when the devices are presently attached to different computers?
- What Logitech receiver and HID++ selection method works safely when a host has several receivers or paired Logitech devices, and what models beyond these exact MX devices should be supported?
- Is per-monitor UI placement needed, or are physical-monitor-aware outer-edge detection plus the existing computer-level Deskflow layout sufficient?

The first implementation experiment should answer the Pop client edge-capture question. Do **not** remove the working relative-injection/keyboard-only path until there is a tested return route for the physical mouse.

## References

- Earlier user-provided project brief (historical keyboard-only goal; this spec supersedes it for the proposed next phase). The essentials of that brief are reproduced above so this specification stands alone.
- [XDG Desktop Portal Input Capture interface](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.InputCapture.html) — zones, external pointer barriers, activation position.
- [Wayland protocol and model](https://wayland.freedesktop.org/docs/book/Protocol.html) — ordinary client visibility limits.
- [Windows GetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos) and [multiple-display monitor functions](https://learn.microsoft.com/en-us/windows/win32/gdi/multiple-display-monitors-functions).

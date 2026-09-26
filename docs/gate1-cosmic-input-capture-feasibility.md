# Gate 1: COSMIC client-side physical edge capture

Status on 2026-09-26: **the standard InputCapture portal path is unavailable in the installed Pop!_OS COSMIC session**. This is a finding about the present desktop stack, not proof that physical Logitech handoff is impossible. No device was switched and the working Deskflow client was not stopped for this check.

## What was checked

Installed packages:

```text
cosmic-comp 0.1~1789989464~24.04~cd881a4
xdg-desktop-portal 1.18.4-1ubuntu2.24.04.2
xdg-desktop-portal-cosmic 0.1.0pop1~1789501966~24.04~2f41161
```

The public `org.freedesktop.portal.Desktop` object exposes `RemoteDesktop` and `ScreenCast`, but not `org.freedesktop.portal.InputCapture`. The COSMIC backend object likewise lacks `org.freedesktop.impl.portal.InputCapture`. The installed `/usr/share/xdg-desktop-portal/portals/cosmic.portal` advertises Access, FileChooser, RemoteDesktop, Screenshot, Settings, and ScreenCast, but no InputCapture. These are read-only checks; neither a permission dialog nor an EIS session was started.

Useful repeatable checks (no pager):

```bash
busctl --user --no-pager introspect \
  org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop \
  | rg 'org.freedesktop.portal.(InputCapture|RemoteDesktop|ScreenCast)'

busctl --user --no-pager introspect \
  org.freedesktop.impl.portal.desktop.cosmic /org/freedesktop/portal/desktop \
  | rg 'org.freedesktop.impl.portal.(InputCapture|RemoteDesktop|ScreenCast)'

rg 'Interfaces=' /usr/share/xdg-desktop-portal/portals/cosmic.portal
```

If `InputCapture` becomes available after a COSMIC update, the next experiment is a **diagnostic-only** Pop client session: get zones, build unique barriers only along the outer edges relevant to Deskflow layout, log activation ID/barrier ID/cursor position, and release immediately. Test with the physical mouse manually on Pop; do not add automatic Logitech switching until edge detection and return are reliable. The probe should also check whether InputCapture can coexist with the current RemoteDesktop client session.

## Why no portal probe was run yet

Deskflow's existing `PortalInputCapture` class requires the public portal interface to create a session. On this installation, a program using that API would fail before it could receive zones or crossing events. A runtime edge test would therefore add no evidence beyond the interface and backend checks. The present Deskflow fork creates `PortalInputCapture` only for a primary/server `EiScreen`; adapting it for a Pop client remains future Deskflow work after the OS capability exists.

The old local `pop-right-edge-watcher` uses a thin GTK layer-shell surface on one right-hand display and demonstrates a different, non-portal mechanism. It is not the proposed portal design: it occupies a strip, currently covers only one outer edge, and is outside Deskflow. Reusing that mechanism inside Deskflow would be an architectural fallback with its own multi-monitor, click interception, and focus tests, not an invisible substitute for the missing portal.

## Upstream status and decision

Correction after checking the live GitHub PR records on 2026-09-26: both proposed implementations are **closed, not open**. A [COSMIC compositor maintainer declined the compositor PR](https://github.com/pop-os/cosmic-comp/pull/2853#issuecomment-5764242265), citing the size, AI-assisted origin, new contributor, and security-critical nature of input capture. The contributor then [closed the companion portal PR](https://github.com/pop-os/xdg-desktop-portal-cosmic/pull/369#issuecomment-5771071859) because the compositor half would not be merged. A previously cached public page had shown stale Open statuses; the live PR records supersede it. Both halves are required for the intended standard portal path, and neither is present in the installed stack. This does not rule out a future, separately implemented COSMIC InputCapture feature, but these two PRs should not be treated as an imminent update.

Safe next choices are:

1. Wait for a future, supported COSMIC InputCapture implementation and rerun this gate. There is no accepted implementation or known delivery date from the reviewed PRs.
2. Explicitly accept and prototype an in-client layer-shell edge fallback, keeping the current keyboard-only milestone untouched. This is a different design and requires user agreement.
3. Test the unmerged COSMIC compositor/portal work in an isolated desktop environment. This is substantially more invasive than changing Deskflow and should not be done on the daily-use session without a separate plan and approval.

No option has been selected by this report. Do not remove the current working Deskflow setup or treat the physical-mouse design as validated.

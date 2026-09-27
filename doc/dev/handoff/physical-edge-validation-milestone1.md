# Physical-edge route validation — milestone 1

Status: experimental source implementation; Linux and Windows builds and unit tests passed, **not live-tested**. No physical Logitech slot switching or Deskflow screen transition is part of this milestone.

## Purpose and opt-in

The Windows Deskflow server remains the layout authority. A COSMIC Wayland client may attach a physical mouse locally and use InputCapture to report an external-edge crossing. The server returns a diagnostic route decision only. Both peers must run this fork's protocol 1.9, have TLS and peer-fingerprint checking enabled, and explicitly set `core/physicalEdgeValidation=true` in their *test* settings. The default is false. Older peers negotiate their existing protocol version and do not receive the new messages.

Do not enable this in the everyday Deskflow configuration until the separate runtime test and rollback plan have been approved. The validation client may ask for a new InputCapture portal permission. Its first approval, rejection, or timeout is one-shot; restart the validation controller/application for another attempt.

## Ownership invariant

The only new server action is a read-only call to `validatePhysicalEdgeRoute()`. It checks the connected source proxy, current active screen, lock state, edge and fraction, then uses existing neighbor mapping to compute a possible destination entry point. It does **not** call `switchScreen()`, `jumpToScreen()`, or emit `CINN`/`COUT`, and does not mutate `m_active`. Approval is not a handoff.

`CPEC` offers the experimental capability; `DPEC` opts the client in. `DERQ` carries request ID, side (0–3 for left/right/top/bottom), and a whole-client-edge fraction in millionths. `CERA` returns request ID and proposed entry coordinates; `CERE` returns request ID and a bounded reason code. The request carries no source name, destination name, portal barrier ID, or raw portal coordinate. The server identifies the source from the connected proxy.

## Geometry and portal lifecycle

`PhysicalEdgeGeometry` uses half-open logical rectangles. It subtracts locally covered sections from each portal-zone edge and only submits exposed segments as barriers. Barrier IDs are unique across zone generations. Before arming, the client compares every portal zone, translated into Deskflow's coordinate space, with the EIS output regions and the complete Deskflow client shape. Mismatched or stale geometry leaves capture disarmed. A matching bounding box alone is insufficient.

The client-side `PortalPhysicalEdge` owns a separate portal session and EIS receiver. It discards captured physical events rather than forwarding motion, buttons, keys, or scroll. Its own GLib context isolates callbacks from the existing RemoteDesktop session. Permission denial, portal disable/closure, server disconnect, failed barrier setup, and bad geometry fail closed. Approval, rejection, and timeout release capture and disable it; elapsed time never re-arms the session. Zone or shape changes invalidate pending geometry before a new barrier set can be armed.

Current barrier policy arms external segments even if the server has no configured neighbor on that side. A no-route response or timeout releases capture and ends the one-shot test. Advertising only configured edges is deferred; this is not a normal-use physical-handoff mode.

## Verification and remaining gate

The isolated Fedora build was configured with `BUILD_TESTS=ON`, `SKIP_BUILD_TESTS=ON`, and `BUILD_X11_SUPPORT=OFF`. The Deskflow GUI and core built, and all 26 unit-test suites passed. The existing `SettingsTests` checks that the opt-in key defaults false and survives settings validation.

These checks do not prove portal behavior in the running desktop, live peer negotiation, server active-screen behavior under a real physical pointer, or safe return after a route response. No live test was performed. The next gate is a separately approved, reversible test with both peers' experimental settings and an explicit way to stop the client without relying on the captured mouse.

The fork's existing manually dispatchable `.github/workflows/windows-native-mx-keys.yml` workflow built and smoke-tested the Windows portable package for [commit `0ab27ff43`](https://github.com/oliveirapaulo/deskflow/commit/0ab27ff43) in [run `36339693581`](https://github.com/oliveirapaulo/deskflow/actions/runs/36339693581). Both Windows cross-platform tests, `PhysicalEdgeProtocolTests` and `PhysicalEdgeRouteTests`, passed. The uploaded package is a short-lived CI artifact; the run and source commit are the durable verification record. This Windows build did not connect to a client or exercise the COSMIC portal.

Milestone 2 must add an ordered physical-slot handoff and a distinct server **commit** operation. It must not reinterpret milestone-1 approval as a committed screen transition.

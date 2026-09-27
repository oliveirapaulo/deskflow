/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "platform/PhysicalEdgeGeometry.h"
#include "platform/PhysicalEdgeOneShot.h"
#include "mt/Thread.h"

#include <glib.h>
#include <libei.h>
#include <libportal/inputcapture.h>
#include <libportal/portal.h>
#include <atomic>
#include <mutex>

class IEventQueue;

namespace deskflow {

// Separate from the primary/server input-forwarding capture path. This class
// never forwards EIS input and never changes Deskflow screen ownership.
class PortalPhysicalEdge
{
public:
  PortalPhysicalEdge(void *eventTarget, IEventQueue *events, physical_edge::Rect shape,
                     std::vector<physical_edge::Rect> eisRegions);
  ~PortalPhysicalEdge();
  void serverDisconnected();
  void shapeChanged(physical_edge::Rect shape, std::vector<physical_edge::Rect> eisRegions);
  void complete(bool approved);
  void serverReady();

private:
  enum class State { WaitingForPermission, WaitingForServer, WaitingForShape, Configuring,
                     Armed, Pending, Complete, Disabled };
  void run(const void *);
  gboolean start();
  void sessionReady(GObject *, GAsyncResult *);
  void barriersReady(GAsyncResult *, uint64_t generation);
  void rebuild();
  void activated(uint32_t id, GVariant *options);
  void deactivated(uint32_t id);
  void disabled();
  void closed();
  void finish(bool approved);
  void releaseAndDisable();
  void clearBarriers();
  void drainEi();
  void post(GSourceFunc callback);
  void removeSource(guint &id);
  static gboolean pendingTimeout(gpointer data);

  void *m_target;
  IEventQueue *m_events;
  GMainContext *m_context = nullptr;
  GMainLoop *m_loop = nullptr;
  Thread *m_thread = nullptr;
  XdpPortal *m_portal = nullptr;
  XdpInputCaptureSession *m_session = nullptr;
  ei *m_ei = nullptr;
  guint m_eiSource = 0;
  guint m_timeoutSource = 0;
  uint32_t m_activationId = 0;
  uint32_t m_nextBarrierId = 1;
  uint64_t m_generation = 0;
  std::atomic<uint64_t> m_shapeRevision{0};
  uint64_t m_configuredShapeRevision = 0;
  bool m_active = false;
  bool m_enabled = false;
  State m_state = State::WaitingForPermission;
  physical_edge::Geometry m_geometry;
  physical_edge::OneShot m_oneShot;
  std::vector<XdpInputCapturePointerBarrier *> m_barriers;
  std::mutex m_shapeMutex;
  physical_edge::Rect m_shape;
  std::vector<physical_edge::Rect> m_eisRegions;
  std::atomic<bool> m_serverConnected{true};
  std::atomic<int> m_result{0}; // 0=pending, 1=approved, 2=rejected
};

} // namespace deskflow

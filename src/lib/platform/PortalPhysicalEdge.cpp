/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/PortalPhysicalEdge.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "base/TMethodJob.h"
#include "deskflow/PhysicalEdgeEvents.h"

#include <glib-unix.h>
#include <cmath>
#include <memory>

namespace deskflow {

PortalPhysicalEdge::PortalPhysicalEdge(void *target, IEventQueue *events, physical_edge::Rect shape,
                                       std::vector<physical_edge::Rect> eisRegions)
    : m_target(target), m_events(events), m_shape(shape), m_eisRegions(std::move(eisRegions))
{
  m_portal = xdp_portal_new();
  m_context = g_main_context_new();
  m_loop = g_main_loop_new(m_context, true);
  m_thread = new Thread(new TMethodJob<PortalPhysicalEdge>(this, &PortalPhysicalEdge::run));
  post([](gpointer data) -> gboolean { return static_cast<PortalPhysicalEdge *>(data)->start(); });
}

PortalPhysicalEdge::~PortalPhysicalEdge()
{
  if (m_loop)
    g_main_loop_quit(m_loop);
  if (m_context)
    g_main_context_wakeup(m_context);
  if (m_thread) {
    m_thread->cancel();
    m_thread->wait();
    delete m_thread;
  }
  removeSource(m_timeoutSource);
  removeSource(m_eiSource);
  if (m_ei)
    ei_unref(m_ei);
  clearBarriers();
  if (m_session)
    g_object_unref(m_session);
  if (m_portal)
    g_object_unref(m_portal);
  if (m_loop)
    g_main_loop_unref(m_loop);
  if (m_context)
    g_main_context_unref(m_context);
}

void PortalPhysicalEdge::run(const void *)
{
  g_main_context_push_thread_default(m_context);
  while (g_main_loop_is_running(m_loop)) {
    Thread::testCancel();
    g_main_context_iteration(m_context, true);
  }
  g_main_context_pop_thread_default(m_context);
}

void PortalPhysicalEdge::post(GSourceFunc callback)
{
  auto *source = g_idle_source_new();
  g_source_set_callback(source, callback, this, nullptr);
  g_source_attach(source, m_context);
  g_source_unref(source);
  g_main_context_wakeup(m_context);
}

void PortalPhysicalEdge::removeSource(guint &id)
{
  if (id && m_context) {
    if (auto *source = g_main_context_find_source_by_id(m_context, id))
      g_source_destroy(source);
    id = 0;
  }
}

gboolean PortalPhysicalEdge::start()
{
  xdp_portal_create_input_capture_session(
      m_portal, nullptr, XDP_INPUT_CAPABILITY_POINTER, nullptr,
      [](GObject *object, GAsyncResult *result, gpointer data) {
        static_cast<PortalPhysicalEdge *>(data)->sessionReady(object, result);
      },
      this
  );
  return G_SOURCE_REMOVE;
}

void PortalPhysicalEdge::sessionReady(GObject *object, GAsyncResult *result)
{
  g_autoptr(GError) error = nullptr;
  m_session = xdp_portal_create_input_capture_session_finish(XDP_PORTAL(object), result, &error);
  if (!m_session) {
    LOG_WARN("physical-edge portal permission/session unavailable: %s", error ? error->message : "unknown");
    m_state = State::WaitingForPermission;
    return;
  }
  const int fd = xdp_input_capture_session_connect_to_eis(m_session, &error);
  if (fd < 0) {
    LOG_WARN("physical-edge EIS connection unavailable: %s", error ? error->message : "unknown");
    m_state = State::Disabled;
    return;
  }
  m_ei = ei_new_receiver(nullptr);
  if (ei_setup_backend_fd(m_ei, fd) != 0) {
    LOG_WARN("physical-edge EIS receiver setup failed");
    ei_unref(m_ei); // libei owns fd even when backend setup fails.
    m_ei = nullptr;
    m_state = State::Disabled;
    return;
  }
  auto *eiSource = g_unix_fd_source_new(ei_get_fd(m_ei), static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR));
  g_source_set_callback(eiSource, reinterpret_cast<GSourceFunc>(+[](gint, GIOCondition condition, gpointer data) -> gboolean {
        auto *self = static_cast<PortalPhysicalEdge *>(data);
        if (condition & (G_IO_HUP | G_IO_ERR)) {
          self->releaseAndDisable();
          self->m_state = State::Disabled;
          self->m_eiSource = 0;
          return G_SOURCE_REMOVE;
        }
        self->drainEi();
        return G_SOURCE_CONTINUE;
      }), this, nullptr);
  m_eiSource = g_source_attach(eiSource, m_context);
  g_source_unref(eiSource);

  g_signal_connect(xdp_input_capture_session_get_session(m_session), "closed",
      G_CALLBACK(+[](XdpSession *, gpointer data) { static_cast<PortalPhysicalEdge *>(data)->closed(); }), this);
  g_signal_connect(m_session, "disabled",
      G_CALLBACK(+[](XdpInputCaptureSession *, GVariant *, gpointer data) {
        static_cast<PortalPhysicalEdge *>(data)->disabled();
      }), this);
  g_signal_connect(m_session, "activated",
      G_CALLBACK(+[](XdpInputCaptureSession *, guint id, GVariant *options, gpointer data) {
        static_cast<PortalPhysicalEdge *>(data)->activated(id, options);
      }), this);
  g_signal_connect(m_session, "deactivated",
      G_CALLBACK(+[](XdpInputCaptureSession *, guint id, GVariant *, gpointer data) {
        static_cast<PortalPhysicalEdge *>(data)->deactivated(id);
      }), this);
  g_signal_connect(m_session, "zones-changed",
      G_CALLBACK(+[](XdpInputCaptureSession *, GVariant *, gpointer data) {
        static_cast<PortalPhysicalEdge *>(data)->rebuild();
      }), this);
  m_state = State::WaitingForShape;
  rebuild();
}

void PortalPhysicalEdge::drainEi()
{
  ei_dispatch(m_ei);
  while (auto *event = ei_get_event(m_ei))
    ei_event_unref(event); // Deliberately never forward local physical input.
}

void PortalPhysicalEdge::clearBarriers()
{
  for (auto *barrier : m_barriers)
    g_object_unref(barrier);
  m_barriers.clear();
}

void PortalPhysicalEdge::rebuild()
{
  if (!m_session || m_state == State::Complete || m_state == State::Disabled ||
      m_state == State::WaitingForPermission)
    return;
  if (m_state == State::Pending) {
    finish(false); // A changed zone set invalidates the pending request.
    return;
  }
  ++m_generation;
  releaseAndDisable();
  clearBarriers();
  m_geometry = physical_edge::Geometry{};

  std::vector<physical_edge::Rect> zones;
  for (auto *node = xdp_input_capture_session_get_zones(m_session); node; node = node->next) {
    gint x = 0, y = 0;
    guint width = 0, height = 0;
    g_object_get(node->data, "x", &x, "y", &y, "width", &width, "height", &height, nullptr);
    if (width > INT32_MAX || height > INT32_MAX) {
      m_state = State::WaitingForShape;
      return;
    }
    zones.push_back({x, y, static_cast<int32_t>(width), static_cast<int32_t>(height)});
  }
  physical_edge::Rect shape;
  std::vector<physical_edge::Rect> eisRegions;
  {
    std::lock_guard lock(m_shapeMutex);
    shape = m_shape;
    eisRegions = m_eisRegions;
    m_configuredShapeRevision = m_shapeRevision.load();
  }
  if (eisRegions.empty() || !m_geometry.reset(zones, shape, m_nextBarrierId, eisRegions)) {
    LOG_WARN("physical-edge portal zones do not match the Deskflow client shape; capture stays disarmed");
    m_state = State::WaitingForShape;
    return;
  }
  m_nextBarrierId += static_cast<uint32_t>(m_geometry.segments().size());
  for (const auto &segment : m_geometry.segments()) {
    const bool vertical = segment.side == physical_edge::Side::Left ||
        segment.side == physical_edge::Side::Right;
    const int x1 = vertical ? segment.fixed : segment.begin;
    const int y1 = vertical ? segment.begin : segment.fixed;
    const int x2 = vertical ? segment.fixed : segment.end - 1;
    const int y2 = vertical ? segment.end - 1 : segment.fixed;
    m_barriers.push_back(XDP_INPUT_CAPTURE_POINTER_BARRIER(g_object_new(
        XDP_TYPE_INPUT_CAPTURE_POINTER_BARRIER, "id", segment.barrierId,
        "x1", x1, "y1", y1, "x2", x2, "y2", y2, nullptr)));
  }
  if (m_barriers.empty()) {
    m_state = State::Disabled;
    return;
  }
  GList *list = nullptr;
  for (auto *barrier : m_barriers)
    list = g_list_append(list, g_object_ref(barrier));
  m_state = State::Configuring;
  struct BarrierRequest { PortalPhysicalEdge *self; uint64_t generation; GList *list; };
  auto *request = new BarrierRequest{this, m_generation, list};
  xdp_input_capture_session_set_pointer_barriers(m_session, list, nullptr,
      [](GObject *, GAsyncResult *result, gpointer data) {
        std::unique_ptr<BarrierRequest> request(static_cast<BarrierRequest *>(data));
        request->self->barriersReady(result, request->generation);
        g_list_free_full(request->list, g_object_unref);
      }, request);
}

void PortalPhysicalEdge::barriersReady(GAsyncResult *result, uint64_t generation)
{
  g_autoptr(GError) error = nullptr;
  auto *failed = xdp_input_capture_session_set_pointer_barriers_finish(m_session, result, &error);
  const bool okay = !error && !failed;
  g_list_free_full(failed, g_object_unref);
  if (generation != m_generation || m_state != State::Configuring || !m_oneShot.canArm())
    return;
  if (m_configuredShapeRevision != m_shapeRevision.load()) {
    releaseAndDisable();
    m_state = State::WaitingForShape;
    return;
  }
  if (!okay) {
    LOG_WARN("physical-edge portal rejected a barrier; validation remains disabled");
    m_state = State::Disabled;
    return;
  }
  if (!m_serverConnected.load()) {
    m_state = State::WaitingForServer;
    return;
  }
  xdp_input_capture_session_enable(m_session);
  m_enabled = true;
  m_state = State::Armed;
}

void PortalPhysicalEdge::activated(uint32_t id, GVariant *options)
{
  if (m_state != State::Armed || !m_oneShot.canArm())
    return;
  if (!m_serverConnected.load()) {
    m_activationId = id;
    m_active = true;
    releaseAndDisable();
    m_state = State::WaitingForServer;
    return;
  }
  if (m_configuredShapeRevision != m_shapeRevision.load()) {
    m_activationId = id;
    m_active = true;
    releaseAndDisable();
    m_state = State::WaitingForShape;
    return;
  }
  guint barrierId = 0;
  gdouble x = 0, y = 0;
  const bool fields = options && g_variant_lookup(options, "barrier_id", "u", &barrierId) &&
      g_variant_lookup(options, "cursor_position", "(dd)", &x, &y);
  auto fraction = fields ? m_geometry.fraction(barrierId, x, y) : std::nullopt;
  if (!fraction) {
    m_activationId = id;
    m_active = true;
    releaseAndDisable();
    m_state = State::Disabled;
    return;
  }
  m_activationId = id;
  m_active = true;
  if (!m_oneShot.begin()) {
    releaseAndDisable();
    return;
  }
  m_state = State::Pending;
  auto *info = new PhysicalEdgeActivatedInfo;
  info->side = static_cast<uint32_t>(m_geometry.find(barrierId)->side);
  info->fraction = static_cast<uint32_t>(std::round(*fraction * 1000000.0));
  m_events->addEvent(Event(EventTypes::PhysicalEdgeActivated, m_target, info));
  auto *timeout = g_timeout_source_new_seconds(3);
  g_source_set_callback(timeout, pendingTimeout, this, nullptr);
  m_timeoutSource = g_source_attach(timeout, m_context);
  g_source_unref(timeout);
}

gboolean PortalPhysicalEdge::pendingTimeout(gpointer data)
{
  auto *self = static_cast<PortalPhysicalEdge *>(data);
  self->m_timeoutSource = 0;
  if (self->m_state == State::Pending)
    self->finish(false);
  return G_SOURCE_REMOVE;
}

void PortalPhysicalEdge::finish(bool approved)
{
  if (m_state != State::Pending)
    return;
  if (m_timeoutSource) {
    removeSource(m_timeoutSource);
  }
  releaseAndDisable();
  m_oneShot.finish();
  m_state = State::Complete;
  LOG_INFO("physical-edge route validation %s; capture is one-shot and now disarmed",
           approved ? "approved" : "rejected/timed out");
}

void PortalPhysicalEdge::releaseAndDisable()
{
  if (m_active && m_session) {
    xdp_input_capture_session_release(m_session, m_activationId);
    m_active = false;
  }
  if (m_enabled && m_session) {
    xdp_input_capture_session_disable(m_session);
    m_enabled = false;
  }
}

void PortalPhysicalEdge::deactivated(uint32_t id)
{
  if (id == m_activationId)
    m_active = false;
}

void PortalPhysicalEdge::disabled()
{
  if (m_state == State::Pending)
    finish(false);
  if (m_active && m_session)
    xdp_input_capture_session_release(m_session, m_activationId);
  m_enabled = false;
  m_active = false;
  if (m_state != State::Complete)
    m_state = State::WaitingForPermission;
}

void PortalPhysicalEdge::closed()
{
  if (m_state == State::Pending)
    finish(false);
  m_enabled = false;
  m_active = false;
  m_state = State::WaitingForPermission;
}

void PortalPhysicalEdge::serverDisconnected()
{
  m_serverConnected = false;
  post([](gpointer data) -> gboolean {
    auto *self = static_cast<PortalPhysicalEdge *>(data);
    if (self->m_state == State::Pending)
      self->finish(false);
    self->releaseAndDisable();
    if (self->m_state != State::Complete)
      self->m_state = State::WaitingForServer;
    return G_SOURCE_REMOVE;
  });
}

void PortalPhysicalEdge::serverReady()
{
  m_serverConnected = true;
  post([](gpointer data) -> gboolean {
    auto *self = static_cast<PortalPhysicalEdge *>(data);
    if (self->m_state == State::WaitingForServer && self->m_session)
      self->rebuild();
    return G_SOURCE_REMOVE;
  });
}

void PortalPhysicalEdge::shapeChanged(physical_edge::Rect shape, std::vector<physical_edge::Rect> eisRegions)
{
  {
    std::lock_guard lock(m_shapeMutex);
    m_shape = shape;
    m_eisRegions = std::move(eisRegions);
    ++m_shapeRevision;
  }
  post([](gpointer data) -> gboolean {
    auto *self = static_cast<PortalPhysicalEdge *>(data);
    if (self->m_state == State::Armed || self->m_state == State::WaitingForShape ||
        self->m_state == State::Configuring)
      self->rebuild();
    else if (self->m_state == State::Pending)
      self->finish(false);
    return G_SOURCE_REMOVE;
  });
}

void PortalPhysicalEdge::complete(bool approved)
{
  m_result = approved ? 1 : 2;
  post([](gpointer data) -> gboolean {
    auto *self = static_cast<PortalPhysicalEdge *>(data);
    const int result = self->m_result.exchange(0);
    if (result)
      self->finish(result == 1);
    return G_SOURCE_REMOVE;
  });
}

} // namespace deskflow

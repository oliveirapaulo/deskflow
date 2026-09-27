/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "server/Server.h"

#include <cstring>

ClientProxy1_9::ClientProxy1_9(const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events)
    : ClientProxy1_8(name, stream, server, events)
{
}

void ClientProxy1_9::offerPhysicalEdgeValidation()
{
  if (!m_offered) {
    m_offered = true;
    ProtocolUtil::writef(getStream(), kMsgCPhysicalEdgeCapability);
  }
}

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDPhysicalEdgeCapability, 4) == 0) {
    m_clientEnabled = m_offered;
    return m_offered;
  }
  if (memcmp(code, kMsgDPhysicalEdgeRequest, 4) == 0) {
    uint32_t id = 0;
    uint8_t side = 0;
    uint32_t fraction = 0;
    if (!ProtocolUtil::readf(getStream(), kMsgDPhysicalEdgeRequest + 4, &id, &side, &fraction))
      return false;
    // Sequence comparison is modulo 2^32. A repeated or stale request must
    // never cause another approval, even though approval itself is noncommittal.
    const bool fresh = !m_hasRequest || static_cast<int32_t>(id - m_lastRequest) > 0;
    Server::PhysicalEdgeRoute route;
    if (m_offered && m_clientEnabled && fresh) {
      m_hasRequest = true;
      m_lastRequest = id;
      route = m_server->validatePhysicalEdgeRoute(this, side, fraction);
    } else {
      route.reason = Server::PhysicalEdgeReject::Unavailable;
    }
    if (route.reason == Server::PhysicalEdgeReject::None)
      ProtocolUtil::writef(getStream(), kMsgCPhysicalEdgeApproved, id, route.x, route.y);
    else
      ProtocolUtil::writef(getStream(), kMsgCPhysicalEdgeRejected, id, static_cast<uint8_t>(route.reason));
    return true;
  }
  return ClientProxy1_8::parseMessage(code);
}

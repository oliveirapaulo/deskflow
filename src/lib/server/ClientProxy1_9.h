/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_8.h"

// Protocol 1.9 adds diagnostic-only physical-edge route validation.
class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *stream, Server *server, IEventQueue *events);
  bool parseMessage(const uint8_t *code) override;
  void offerPhysicalEdgeValidation();

private:
  bool m_offered = false;
  bool m_clientEnabled = false;
  bool m_hasRequest = false;
  uint32_t m_lastRequest = 0;
};

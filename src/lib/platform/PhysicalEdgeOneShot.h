/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

namespace deskflow::physical_edge {

// A validation controller is single-use. Only constructing a new controller
// explicitly resets it; elapsed time and repeated portal signals cannot.
class OneShot
{
public:
  enum class State { Ready, Pending, Complete };
  [[nodiscard]] bool begin()
  {
    if (m_state != State::Ready)
      return false;
    m_state = State::Pending;
    return true;
  }
  void finish()
  {
    if (m_state == State::Pending)
      m_state = State::Complete;
  }
  [[nodiscard]] bool canArm() const { return m_state == State::Ready; }
  [[nodiscard]] State state() const { return m_state; }

private:
  State m_state = State::Ready;
};

} // namespace deskflow::physical_edge

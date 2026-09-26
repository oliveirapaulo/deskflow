/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/MxKeysHandoff.h"

#include "base/Log.h"

#ifdef DESKFLOW_NATIVE_MX_KEYS_HANDOFF
#include <hidapi.h>

#include <QByteArray>
#include <QThreadPool>
#include <QString>

#include <atomic>

namespace {
std::atomic<bool> s_switchInProgress{false};

void sendMxKeysHostSwitch(int hostSlot, const char *transition)
{
  // These are the exact seven bytes validated with hidapitester on both hosts.
  // 0x01 selects the MX Keys paired to receiver device 1; the final host byte
  // is zero-based, while Settings uses Easy-Switch slots numbered 1-3.
  const unsigned char report[7] = {0x10, 0x01, 0x09, 0x1e, static_cast<unsigned char>(hostSlot - 1), 0x00, 0x00};

  if (hid_init() != 0) {
    LOG_ERR("%s: HIDAPI initialization failed", transition);
    return;
  }

  auto *devices = hid_enumerate(0x046d, 0xc52b);
  QByteArray receiverPath;
  int matchingReceivers = 0;
  for (auto *device = devices; device; device = device->next) {
    if (device->usage_page == 0xff00 && device->usage == 0x0001 && device->path) {
      receiverPath = device->path;
      ++matchingReceivers;
    }
  }
  hid_free_enumeration(devices);

  if (matchingReceivers != 1) {
    LOG_ERR("%s: expected one Logitech 046d:c52b receiver HID interface (ff00/0001), found %d",
            transition, matchingReceivers);
    hid_exit();
    return;
  }

  auto *receiver = hid_open_path(receiverPath.constData());
  if (!receiver) {
    const auto *hidError = hid_error(nullptr);
    const auto error = hidError ? QString::fromWCharArray(hidError) : QStringLiteral("unknown HID error");
    LOG_ERR("%s: could not open Logitech receiver HID interface: %s", transition, qPrintable(error));
    hid_exit();
    return;
  }

  const int written = hid_write(receiver, report, sizeof(report));
  if (written != static_cast<int>(sizeof(report))) {
    const auto *hidError = hid_error(receiver);
    const auto error = hidError ? QString::fromWCharArray(hidError) : QStringLiteral("unknown HID error");
    LOG_ERR("%s: MX Keys host %d HID write failed (%d bytes): %s", transition, hostSlot, written,
            qPrintable(error));
  } else {
    LOG_NOTE("%s: sent MX Keys host %d HID report; mouse was not switched", transition, hostSlot);
  }

  hid_close(receiver);
  hid_exit();
}
} // namespace
#endif

bool queueMxKeysHostSwitch(int hostSlot, const char *transition)
{
  if (hostSlot < 1 || hostSlot > 3) {
    LOG_ERR("%s: MX Keys host slot must be 1, 2, or 3 (got %d)", transition, hostSlot);
    return false;
  }

#ifdef DESKFLOW_NATIVE_MX_KEYS_HANDOFF
  bool expected = false;
  if (!s_switchInProgress.compare_exchange_strong(expected, true)) {
    LOG_WARN("%s: MX Keys host switch already in progress; skipped", transition);
    return false;
  }

  QThreadPool::globalInstance()->start([hostSlot, transition] {
    sendMxKeysHostSwitch(hostSlot, transition);
    s_switchInProgress.store(false);
  });
  return true;
#else
  LOG_ERR("%s: native MX Keys handoff was not enabled at build time", transition);
  return false;
#endif
}

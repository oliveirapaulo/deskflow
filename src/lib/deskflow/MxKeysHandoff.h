/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

// Queues the proven MX Keys HID++ host change without blocking a screen
// transition. Host slots are the physical Easy-Switch slots 1, 2, and 3.
// Returns false if the feature is unavailable, the slot is invalid, or a
// previous switch is still in progress. A queued write reports its own result.
bool queueMxKeysHostSwitch(int hostSlot, const char *transition);

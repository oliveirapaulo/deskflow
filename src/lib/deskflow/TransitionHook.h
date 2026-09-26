/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

// Starts a trusted, locally configured script without waiting for it. The
// script owns its logging, verification, and duplicate-run protection.
bool startTransitionHook(const QString &scriptPath, const char *transition);

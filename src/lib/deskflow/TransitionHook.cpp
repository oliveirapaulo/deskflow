/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/TransitionHook.h"

#include "base/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

bool startTransitionHook(const QString &scriptPath, const char *transition)
{
  if (scriptPath.isEmpty()) {
    return false;
  }

  const QFileInfo script(scriptPath);
  if (!script.isAbsolute() || !script.isFile()) {
    LOG_ERR("%s hook is not an existing absolute file: %s", transition, qPrintable(scriptPath));
    return false;
  }

#ifndef Q_OS_WIN
  if (!script.isExecutable()) {
    LOG_ERR("%s hook is not executable: %s", transition, qPrintable(scriptPath));
    return false;
  }
#endif

  QProcess process;
#ifdef Q_OS_WIN
  // cmd.exe does not use Qt's normal Windows argument parsing. /S removes
  // the outer pair of quotes; the inner pair protects paths with spaces.
  const auto systemRoot = qEnvironmentVariable("SystemRoot");
  if (systemRoot.isEmpty()) {
    LOG_ERR("cannot locate command processor for %s hook", transition);
    return false;
  }
  process.setProgram(QDir::toNativeSeparators(systemRoot + QStringLiteral("/System32/cmd.exe")));
  process.setNativeArguments(QStringLiteral("/D /S /C \"\"%1\"\"").arg(QDir::toNativeSeparators(scriptPath)));
#else
  process.setProgram(scriptPath);
#endif

  qint64 pid = 0;
  if (!process.startDetached(&pid)) {
    LOG_ERR("failed to start %s hook: %s", transition, qPrintable(scriptPath));
    return false;
  }

  LOG_NOTE("started %s hook, pid=%lld: %s", transition, static_cast<long long>(pid), qPrintable(scriptPath));
  return true;
}

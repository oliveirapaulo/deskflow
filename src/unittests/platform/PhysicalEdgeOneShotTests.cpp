/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/PhysicalEdgeOneShot.h"
#include <QtTest>

using deskflow::physical_edge::OneShot;

class PhysicalEdgeOneShotTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void approvedRejectedAndTimeoutAreTerminal()
  {
    for (int outcome = 0; outcome < 3; ++outcome) {
      OneShot gate;
      QVERIFY(gate.canArm());
      QVERIFY(gate.begin());
      QVERIFY(!gate.begin()); // repeated Activated while request is pending
      gate.finish(); // approval, rejection and timeout use the same terminal path
      QCOMPARE(gate.state(), OneShot::State::Complete);
      QVERIFY(!gate.canArm());
      QVERIFY(!gate.begin());
    }
  }
  void timeAloneDoesNotReset()
  {
    OneShot gate;
    QVERIFY(gate.begin());
    gate.finish();
    for (int elapsed = 0; elapsed < 10000; ++elapsed)
      QVERIFY(!gate.canArm());
    OneShot explicitNewController;
    QVERIFY(explicitNewController.canArm());
  }
};

QTEST_GUILESS_MAIN(PhysicalEdgeOneShotTests)
#include "PhysicalEdgeOneShotTests.moc"

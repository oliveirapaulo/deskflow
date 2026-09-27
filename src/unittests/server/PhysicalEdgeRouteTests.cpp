/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/PhysicalEdgeRouteMath.h"
#include <QtTest>

class PhysicalEdgeRouteTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void wholeEdgePositions()
  {
    auto left = physicalEdgePoint(-100, 30, 300, 200, 0, 250000);
    QVERIFY(left);
    QCOMPARE(left->x, -101);
    QCOMPARE(left->y, 80);
    auto right = physicalEdgePoint(-100, 30, 300, 200, 1, 750000);
    QVERIFY(right);
    QCOMPARE(right->x, 200);
    QCOMPARE(right->y, 180);
    auto top = physicalEdgePoint(-100, 30, 300, 200, 2, 500000);
    QVERIFY(top);
    QCOMPARE(top->x, 50);
    QCOMPARE(top->y, 29);
    auto bottom = physicalEdgePoint(-100, 30, 300, 200, 3, 1000000);
    QVERIFY(bottom);
    QCOMPARE(bottom->x, 199);
    QCOMPARE(bottom->y, 230);
  }

  void rejectsInvalidInput()
  {
    QVERIFY(!physicalEdgePoint(0, 0, 0, 100, 0, 500000));
    QVERIFY(!physicalEdgePoint(0, 0, 100, 100, 4, 500000));
    QVERIFY(!physicalEdgePoint(0, 0, 100, 100, 0, 1000001));
    QVERIFY(!physicalEdgePoint(INT32_MIN, 0, 100, 100, 0, 0));
    QVERIFY(!physicalEdgePoint(1, 0, INT32_MAX, 100, 1, 0));
  }
};

QTEST_GUILESS_MAIN(PhysicalEdgeRouteTests)
#include "PhysicalEdgeRouteTests.moc"

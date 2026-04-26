#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtTest/QTest>

#include "io/io7indicator.h"
#include "io/iobase.h"
#include "io/iokeyboard.h"
#include "io/iomouse.h"

using namespace Ripes;

namespace {

/**
 * IOBase::unregister() blocks until something acknowledges aboutToDelete by
 * setting the passed atomic<bool> to 1. In production that's IOManager; in
 * tests we wire a trivial acknowledger so peripherals can be destroyed.
 */
template <typename T>
T *makePeripheral() {
  auto *p = new T(nullptr);
  QObject::connect(p, &IOBase::aboutToDelete, p,
                   [](std::atomic<bool> &ok) { ok = 1; });
  return p;
}

} // namespace

class tst_io : public QObject {
  Q_OBJECT

private slots:
  // ---------------- IO7Indicator ----------------

  void sevenSegment_writeReadRoundTrip() {
    auto *ind = makePeripheral<IO7Indicator>();

    // Default 4 digits, each 4 bytes wide.
    QCOMPARE(ind->byteSize(), 4u * 4u);
    QCOMPARE(ind->registers().size(), size_t{4});

    // Write a different segment pattern to every digit, then read back.
    const std::vector<uint8_t> patterns = {0x3F, 0x06, 0x5B, 0x4F};
    for (size_t i = 0; i < patterns.size(); ++i) {
      ind->ioWrite(static_cast<AInt>(i * 4), patterns[i], 4);
    }
    for (size_t i = 0; i < patterns.size(); ++i) {
      QCOMPARE(static_cast<uint8_t>(ind->ioRead(static_cast<AInt>(i * 4), 4)),
               patterns[i]);
    }

    // Only the low byte is stored.
    ind->ioWrite(0, 0xFFFFFF7E, 4);
    QCOMPARE(static_cast<uint8_t>(ind->ioRead(0, 4)), uint8_t{0x7E});

    // Misaligned / wrong-size accesses are ignored on write and return 0 on
    // read.
    ind->ioWrite(1, 0xAA, 4);
    QCOMPARE(static_cast<uint8_t>(ind->ioRead(0, 4)), uint8_t{0x7E});
    QCOMPARE(ind->ioRead(1, 4), VInt{0});

    // Out-of-range digit access returns 0.
    QCOMPARE(ind->ioRead(static_cast<AInt>(patterns.size() * 4), 4), VInt{0});

    // reset() clears all digits.
    ind->reset();
    for (size_t i = 0; i < patterns.size(); ++i) {
      QCOMPARE(ind->ioRead(static_cast<AInt>(i * 4), 4), VInt{0});
    }

    delete ind;
  }

  void sevenSegment_numDigitsParameterResizes() {
    auto *ind = makePeripheral<IO7Indicator>();
    // Parameter id 0 is NUM_DIGITS.
    QVERIFY(ind->setParameter(0, 8));
    QCOMPARE(ind->byteSize(), 8u * 4u);
    QCOMPARE(ind->registers().size(), size_t{8});
    delete ind;
  }

  // ---------------- IOKeyboard ----------------

  void keyboard_keyPressEnqueuesAndStatusReflectsBuffer() {
    auto *kb = makePeripheral<IOKeyboard>();
    constexpr AInt KEY_DATA = 0;
    constexpr AInt KEY_STATUS = 4;

    // Empty buffer: status == 0, data read returns 0.
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{0});
    QCOMPARE(kb->ioRead(KEY_DATA, 1), VInt{0});

    // Send three keys via real Qt events (covers the actual key handler).
    const QList<QPair<int, char>> keys = {
        {Qt::Key_A, 'A'}, {Qt::Key_B, 'B'}, {Qt::Key_5, '5'}};
    for (const auto &k : keys) {
      QKeyEvent ev(QEvent::KeyPress, k.first, Qt::NoModifier);
      QCoreApplication::sendEvent(kb, &ev);
    }
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{3});

    // Reading KEY_DATA dequeues in FIFO order.
    for (const auto &k : keys) {
      QCOMPARE(static_cast<char>(kb->ioRead(KEY_DATA, 1)), k.second);
    }
    QCOMPARE(kb->ioRead(KEY_DATA, 1), VInt{0});

    // Unmapped keys must not enqueue anything.
    QKeyEvent ignored(QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier);
    QCoreApplication::sendEvent(kb, &ignored);
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{0});

    delete kb;
  }

  void keyboard_writeStatusClearsBuffer() {
    auto *kb = makePeripheral<IOKeyboard>();
    constexpr AInt KEY_STATUS = 4;

    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier);
    QCoreApplication::sendEvent(kb, &ev);
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{1});

    // Non-zero write to KEY_STATUS clears the FIFO.
    kb->ioWrite(KEY_STATUS, 1, 4);
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{0});

    // reset() also clears.
    QCoreApplication::sendEvent(kb, &ev);
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{1});
    kb->reset();
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{0});

    delete kb;
  }

  void keyboard_bufferOverflowIsBounded() {
    auto *kb = makePeripheral<IOKeyboard>();
    constexpr AInt KEY_STATUS = 4;
    // Parameter id 0 is BUFSIZE.
    kb->setParameter(0, 4);

    QKeyEvent ev(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier);
    for (int i = 0; i < 10; ++i) {
      QCoreApplication::sendEvent(kb, &ev);
    }
    QCOMPARE(kb->ioRead(KEY_STATUS, 4), VInt{4});
    delete kb;
  }

  // ---------------- IOMouse ----------------

  void mouse_moveClampsAndUpdatesRegisters() {
    auto *m = makePeripheral<IOMouse>();
    // Defaults: WIDTH=200, HEIGHT=150.
    constexpr AInt X = 0;
    constexpr AInt Y = 4;

    QMouseEvent move(QEvent::MouseMove, QPointF(50, 75), QPointF(50, 75),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m, &move);
    QCOMPARE(m->ioRead(X, 4), VInt{50});
    QCOMPARE(m->ioRead(Y, 4), VInt{75});

    // Out-of-range coordinates are clamped to [0, w-1] / [0, h-1].
    QMouseEvent moveOOB(QEvent::MouseMove, QPointF(9999, -10),
                        QPointF(9999, -10), Qt::NoButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(m, &moveOOB);
    QCOMPARE(m->ioRead(X, 4), VInt{199});
    QCOMPARE(m->ioRead(Y, 4), VInt{0});
  delete m;
  }

  void mouse_buttonsAndScroll() {
    auto *m = makePeripheral<IOMouse>();
    constexpr AInt LBUTTON = 8;
    constexpr AInt RBUTTON = 12;
    constexpr AInt SCROLL = 16;

    QPointF p(10, 10);

    // Initial state.
    QCOMPARE(m->ioRead(LBUTTON, 4), VInt{0});
    QCOMPARE(m->ioRead(RBUTTON, 4), VInt{0});

    // Press / release left.
    QMouseEvent lpress(QEvent::MouseButtonPress, p, p, Qt::LeftButton,
                       Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m, &lpress);
    QCOMPARE(m->ioRead(LBUTTON, 4), VInt{1});

    QMouseEvent lrelease(QEvent::MouseButtonRelease, p, p, Qt::LeftButton,
                         Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m, &lrelease);
    QCOMPARE(m->ioRead(LBUTTON, 4), VInt{0});

    // Right button is independent.
    QMouseEvent rpress(QEvent::MouseButtonPress, p, p, Qt::RightButton,
                       Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m, &rpress);
    QCOMPARE(m->ioRead(RBUTTON, 4), VInt{1});
    QCOMPARE(m->ioRead(LBUTTON, 4), VInt{0});

    // SCROLL register is software-writable (e.g. for ack/clear from program).
    m->ioWrite(SCROLL, 42, 4);
    QCOMPARE(m->ioRead(SCROLL, 4), VInt{42});

    m->reset();
    QCOMPARE(m->ioRead(LBUTTON, 4), VInt{0});
    QCOMPARE(m->ioRead(RBUTTON, 4), VInt{0});
    QCOMPARE(m->ioRead(SCROLL, 4), VInt{0});
  delete m;
  }
};

QTEST_MAIN(tst_io)
#include "tst_io.moc"

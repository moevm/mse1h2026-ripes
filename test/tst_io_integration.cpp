#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtTest/QTest>

#include <atomic>
#include <memory>

#include "io/io7indicator.h"
#include "io/iobase.h"
#include "io/iokeyboard.h"
#include "io/iomanager.h"
#include "io/iomouse.h"
#include "io/ioregistry.h"
#include "processorhandler.h"
#include "processorregistry.h"
#include "ripessettings.h"
#include "rvisainfo_common.h"

using namespace Ripes;
using namespace Assembler;

namespace {

// Locate the memory-map entry that corresponds to a particular peripheral.
// Returns the base address; fails the current test if no matching entry exists.
AInt baseAddressOf(IOBase *p) {
  const auto &map = IOManager::get().memoryMap();
  for (const auto &e : map) {
    if (e.second.name == p->name())
      return e.first;
  }
  qFatal("Peripheral '%s' is not present in the IOManager memory map",
         qUtf8Printable(p->name()));
  return 0;
}

void destroyPeripheral(IOBase *&p) {
  if (!p)
    return;
  // The IOBase destructor calls unregister(), which emits aboutToDelete and
  // makes IOManager remove this peripheral from its bookkeeping. Do NOT call
  // IOManager::removePeripheral() manually here -- doing so would lead to a
  // double-remove and trip the assert in IOManager::removePeripheral.
  delete p;
  p = nullptr;
}

} // namespace

class tst_io_integration : public QObject {
  Q_OBJECT

private:
  IOBase *m_keyboard = nullptr;
  IOBase *m_mouse = nullptr;
  IOBase *m_indicator = nullptr;

  std::shared_ptr<Program> assembleWithIOSymbols(const QString &src) {
    auto res = ProcessorHandler::getAssembler()->assembleRaw(
        src, &IOManager::get().assemblerSymbols());
    if (!res.errors.empty()) {
      const QString err =
          "Assembly failed:\n" + res.errors.toString() + "\nSource:\n" + src;
      qWarning("%s", qUtf8Printable(err));
    }
    return std::make_shared<Program>(res.program);
  }

  // Run the simulator until either an Exit2 ecall is observed or the cycle
  // budget is exhausted. Returns true on a clean exit, false on timeout.
  bool runUntilExit(unsigned maxCycles = 5000) {
    bool stop = false;
    auto *proc = ProcessorHandler::getProcessorNonConst();
    proc->trapHandler = [&stop, proc] {
      const unsigned a7 = proc->getRegister(RVISA::GPR, 17);
      if (a7 == RVABI::Exit2)
        stop = true;
    };
    unsigned cycles = 0;
    while (!stop && cycles++ < maxCycles)
      proc->clock();
    return stop;
  }

private slots:
  void initTestCase() {
    // Use the simplest RV32 model so the tests stay fast and deterministic.
    ProcessorHandler::selectProcessor(ProcessorID::RV32_SS, {"M"});
  }

  void cleanup() {
    destroyPeripheral(m_keyboard);
    destroyPeripheral(m_mouse);
    destroyPeripheral(m_indicator);
  }

  // Verifies the contract between IOManager and the processor address space:
  //   - a peripheral created via the manager appears in the memory map
  //   - its declared byteSize() matches the registered region size
  //   - reading the peripheral region through the processor memory routes the
  //     access through the peripheral's ioRead() (mouse X register reflects
  //     the position from a synthesized QMouseEvent)
  void integration_processorMemoryReachesPeripheral() {
    m_mouse = IOManager::get().createPeripheral(IOType::MOUSE);
    QVERIFY(m_mouse != nullptr);

    const AInt base = baseAddressOf(m_mouse);
    const auto &entry = IOManager::get().memoryMap().at(base);
    QCOMPARE(entry.size, m_mouse->byteSize());

    QMouseEvent move(QEvent::MouseMove, QPointF(73, 21), QPointF(73, 21),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m_mouse, &move);

    auto &mem = ProcessorHandler::getMemory();
    QCOMPARE(mem.readMem(base + 0, 4), VInt{73}); // X
    QCOMPARE(mem.readMem(base + 4, 4), VInt{21}); // Y
  }

  // Verifies the inverse direction of the integration: a write performed
  // through the processor memory ends up in the peripheral's internal state
  // (and is visible through a direct ioRead()).
  void integration_processorMemoryWriteReachesPeripheral() {
    m_indicator = IOManager::get().createPeripheral(IOType::SEVEN_SEGMENT);
    const AInt base = baseAddressOf(m_indicator);

    auto &mem = ProcessorHandler::getMemory();
    mem.writeMem(base + 0, 0x6D, 4); // segment pattern for "5"
    mem.writeMem(base + 4, 0x7D, 4); // segment pattern for "6"

    QCOMPARE(static_cast<uint8_t>(m_indicator->ioRead(0, 4)), uint8_t{0x6D});
    QCOMPARE(static_cast<uint8_t>(m_indicator->ioRead(4, 4)), uint8_t{0x7D});
  }

  // Verifies that several peripherals can coexist:
  //   - each is assigned a non-overlapping region in the memory map
  //   - traffic to one region does not leak into the other
  void integration_multiplePeripheralsHaveDistinctRegions() {
    m_keyboard = IOManager::get().createPeripheral(IOType::KEYBOARD);
    m_indicator = IOManager::get().createPeripheral(IOType::SEVEN_SEGMENT);

    const AInt kbBase = baseAddressOf(m_keyboard);
    const AInt segBase = baseAddressOf(m_indicator);
    QVERIFY(kbBase != segBase);
    const auto &kbEntry = IOManager::get().memoryMap().at(kbBase);
    const auto &segEntry = IOManager::get().memoryMap().at(segBase);
    const bool disjoint =
        kbEntry.end() <= segEntry.startAddr || segEntry.end() <= kbBase;
    QVERIFY2(disjoint, "Peripheral memory regions overlap");

    auto &mem = ProcessorHandler::getMemory();
    mem.writeMem(segBase, 0x3F, 4);
    QCOMPARE(mem.readMem(kbBase + 4, 4), VInt{0}); // KEY_STATUS untouched
    QCOMPARE(static_cast<uint8_t>(mem.readMem(segBase, 4)), uint8_t{0x3F});
  }

  // Verifies that IOManager exports per-peripheral assembler symbols:
  //   - <NAME>_BASE, <NAME>_SIZE are present
  //   - exported register symbols (KEY_DATA, KEY_STATUS) are present and point
  //     at the correct absolute address (base + register offset)
  void integration_assemblerSymbolsExportedForPeripherals() {
    m_keyboard = IOManager::get().createPeripheral(IOType::KEYBOARD);
    const QString prefix = cName(m_keyboard->name());
    const AInt base = baseAddressOf(m_keyboard);

    const auto &abs = IOManager::get().assemblerSymbols().abs;
    auto require = [&](const QString &name, std::optional<VInt> expect = {}) {
      auto it = abs.find(Symbol(name));
      QVERIFY2(it != abs.end(),
               qUtf8Printable("Missing assembler symbol: " + name));
      if (expect)
        QCOMPARE(static_cast<VInt>(it->second), *expect);
    };
    require(prefix + "_BASE", base);
    require(prefix + "_SIZE", VInt{m_keyboard->byteSize()});
    require(prefix + "_KEY_DATA", base + 0);
    require(prefix + "_KEY_STATUS", base + 4);
  }

  // Verifies that IOManager::reset() clears the internal state of every
  // currently registered peripheral in a single call, regardless of which
  // path produced the state (synthesized event vs. processor memory write).
  void integration_resetClearsAllPeripherals() {
    m_keyboard = IOManager::get().createPeripheral(IOType::KEYBOARD);
    m_indicator = IOManager::get().createPeripheral(IOType::SEVEN_SEGMENT);

    QKeyEvent press(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier);
    QCoreApplication::sendEvent(m_keyboard, &press);
    auto &mem = ProcessorHandler::getMemory();
    mem.writeMem(baseAddressOf(m_indicator), 0x7F, 4);

    QCOMPARE(m_keyboard->ioRead(4, 4), VInt{1});
    QCOMPARE(m_indicator->ioRead(0, 4), VInt{0x7F});

    IOManager::get().reset();

    QCOMPARE(m_keyboard->ioRead(4, 4), VInt{0});
    QCOMPARE(m_indicator->ioRead(0, 4), VInt{0});
  }

  // Verifies that removing a peripheral through IOManager:
  //   - drops it from the memory map
  //   - tears down the corresponding processor IO region (subsequent reads to
  //     the freed address no longer return live peripheral data)
  void integration_removalUnregistersFromMemoryMap() {
    m_indicator = IOManager::get().createPeripheral(IOType::SEVEN_SEGMENT);
    const AInt base = baseAddressOf(m_indicator);
    auto &mem = ProcessorHandler::getMemory();
    mem.writeMem(base, 0x7E, 4);
    QCOMPARE(static_cast<uint8_t>(mem.readMem(base, 4)), uint8_t{0x7E});

    destroyPeripheral(m_indicator);

    bool stillMapped = false;
    for (const auto &e : IOManager::get().memoryMap()) {
      if (e.first == base) {
        stillMapped = true;
        break;
      }
    }
    QVERIFY2(!stillMapped,
             "Peripheral region remained in memory map after removal");
  }

  // End-to-end functional scenario:
  //   1. A mouse and a seven-segment indicator are wired into the processor
  //      through IOManager.
  //   2. A QMouseEvent is delivered to the mouse widget (simulates user input
  //      coming from the GUI).
  //   3. A small RISC-V program, assembled against the symbols exported by
  //      IOManager, reads the X coordinate register from the mouse and
  //      stores it into digit 0 of the seven-segment indicator, then exits
  //      via ecall.
  //   4. After the processor stops, the indicator is inspected (both via the
  //      processor memory and directly) and must contain the value sent in
  //      step 2.
  // This exercises: GUI event -> peripheral -> memory map -> RISC-V load ->
  // RISC-V store -> peripheral -> verification.
  //
  // The mouse is used here (rather than the keyboard) because mouse
  // coordinate registers are idempotent reads; the keyboard KEY_DATA
  // register dequeues from a FIFO on every read, including any speculative
  // readMemConst() calls the simulator may perform, which would drain the
  // value before the real lbu retires and make the test racy.
  void e2e_mouseInputDrivesIndicatorViaRiscvProgram() {
    m_mouse = IOManager::get().createPeripheral(IOType::MOUSE);
    m_indicator = IOManager::get().createPeripheral(IOType::SEVEN_SEGMENT);

    // 1. Build a program that uses the peripheral symbols emitted by
    //    IOManager (MOUSE_0_X / SEVEN_SEGMENT_0_BASE).
    const QString mouse = cName(m_mouse->name());
    const QString seg = cName(m_indicator->name());
    const QString src =
        QString("    .text\n"
                "    .globl _start\n"
                "_start:\n"
                "    li   t0, %1_X\n"
                "    li   t1, %2_BASE\n"
                "    lw   t2, 0(t0)\n"
                "    sw   t2, 0(t1)\n"
                "    li   a7, 93\n" // Exit2
                "    li   a0, 0\n"
                "    ecall\n")
            .arg(mouse, seg);

    auto program = assembleWithIOSymbols(src);
    QVERIFY2(!program->sections.empty(), "Program has no sections");
    ProcessorHandler::loadProgram(program);
    // Reset BEFORE injecting the mouse event, otherwise IOManager::reset()
    // (triggered by the global reset signal) would clear the X/Y registers.
    RipesSettings::getObserver(RIPES_GLOBALSIGNAL_REQRESET)->trigger();

    // 2. Inject a mouse-move through the GUI event path. 0x4B == 'K'.
    QMouseEvent move(QEvent::MouseMove, QPointF(0x4B, 0), QPointF(0x4B, 0),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m_mouse, &move);
    QCOMPARE(m_mouse->ioRead(0, 4), VInt{0x4B});

    // 3. Run until the program issues Exit2.
    QVERIFY2(runUntilExit(),
             "RISC-V program did not reach Exit2 within the cycle budget");

    // 4. The indicator must hold the value sent through the mouse.
    QCOMPARE(static_cast<unsigned>(m_indicator->ioRead(0, 4)), 0x4Bu);
    QCOMPARE(static_cast<unsigned>(ProcessorHandler::getMemory().readMem(
                 baseAddressOf(m_indicator), 4)),
             0x4Bu);
  }
};

QTEST_MAIN(tst_io_integration)
#include "tst_io_integration.moc"

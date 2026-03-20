#pragma once
#include "iobase.h"
#include <QFrame>
#include <QLabel>
#include <QMap>
#include <QMutex>
#include <QPushButton>
#include <QQueue>
#include <QVBoxLayout>
#include <QWidget>

namespace Ripes {

class IOKeyboard : public IOBase {
  Q_OBJECT

  enum Parameters { BUFSIZE };

public:
  IOKeyboard(QWidget *parent);
  ~IOKeyboard() { unregister(); }

  unsigned byteSize() const override { return 8; }
  QString description() const override { return QString(); } // TODO
  QString baseName() const override { return "Keyboard"; }

  const std::vector<RegDesc> &registers() const override { return m_regDescs; }
  const std::vector<IOSymbol> *extraSymbols() const override {
    return &m_extraSymbols;
  }

  VInt ioRead(AInt offset, unsigned size) override;
  void ioWrite(AInt offset, VInt value, unsigned size) override;
  void reset() override;

protected:
  void parameterChanged(unsigned) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void updateLayout();
  void enqueueKey(uint8_t ascii);
  void refreshStatusLabel();

  QHBoxLayout *addKeyRow(QVBoxLayout *parent);
  QPushButton *createKey(const QString &label, uint8_t ascii, int w = 36,
                         int h = 36);
  void flashKey(uint8_t ascii);
  void clearFlash();

  QQueue<uint8_t> m_keyBuffer;
  mutable QMutex m_bufMutex;
  uint8_t m_lastKey = 0;

  QLabel *m_lblData = nullptr;
  QLabel *m_lblChar = nullptr;
  QLabel *m_lblFifoCount = nullptr;
  QMap<uint8_t, QPushButton *> m_keys;
  QPushButton *m_flashedBtn = nullptr;

  QGridLayout *m_mainLayout = nullptr;
  QLabel *m_statusLabel = nullptr;

  std::vector<RegDesc> m_regDescs;
  std::vector<IOSymbol> m_extraSymbols;
};

} // namespace Ripes
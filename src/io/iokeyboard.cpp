#include "iokeyboard.h"
#include "ioregistry.h"
#include <QKeyEvent>
#include <QLabel>
#include <QMutexLocker>
#include <QPushButton>

namespace Ripes {
// Stylesheets
static const char *kStyleKey =
    "QPushButton {"
    "  background-color: #3a3a4a;"
    "  color: #cccccc;"
    "  border: 1px solid #555;"
    "  border-radius: 5px;"
    "  font: bold 11px;"
    "}"
    "QPushButton:hover   { background-color: #50506a; }"
    "QPushButton:pressed { background-color: #d4b0e8; color: #333; }";

static const char *kStyleKeyActive = "QPushButton {"
                                     "  background-color: #d4b0e8;"
                                     "  color: #333;"
                                     "  border: 1px solid #b090c8;"
                                     "  border-radius: 5px;"
                                     "  font: bold 11px;"
                                     "}";

static const char *kStyleBadge = "QLabel {"
                                 "  background-color: #3a3a5a;"
                                 "  color: #eeeeee;"
                                 "  border-radius: 3px;"
                                 "  padding: 1px 5px;"
                                 "  font: bold 11px;"
                                 "}";

static const char *kStyleCaption = "QLabel { color: #999; font: 10px; }";

static const char *kStyleFifoFull =
    "QFrame { background-color: #4488cc; border-radius: 2px; }";

static const char *kStyleFifoEmpty =
    "QFrame { background-color: #555555; border-radius: 2px; }";

static const char *kStylePanel = "QWidget#keyboardPanel {"
                                 "  background-color: #2a2a3a;"
                                 "  border-radius: 8px;"
                                 "}";

IOKeyboard::IOKeyboard(QWidget *parent) : IOBase(IOType::KEYBOARD, parent) {
  m_parameters[BUFSIZE] = IOParam(BUFSIZE, "Buffer size", 16, true, 1, 256);
  setObjectName("keyboardPanel");
  setStyleSheet(kStylePanel);
  setFocusPolicy(Qt::StrongFocus);
  updateLayout();
}

QHBoxLayout *IOKeyboard::addKeyRow(QVBoxLayout *parent) {
  auto *row = new QHBoxLayout();
  row->setSpacing(4);
  row->setAlignment(Qt::AlignCenter);
  parent->addLayout(row);
  return row;
}

QPushButton *IOKeyboard::createKey(const QString &label, uint8_t ascii, int w,
                                   int h) {
  auto *btn = new QPushButton(label, this);
  btn->setFixedSize(w, h);
  btn->setFocusPolicy(Qt::NoFocus);
  btn->setStyleSheet(kStyleKey);
  connect(btn, &QPushButton::clicked, this,
          [this, ascii]() { enqueueKey(ascii); });
  m_keys[ascii] = btn;
  return btn;
}

void IOKeyboard::updateLayout() {
  QLayoutItem *item;
  while ((item = m_mainLayout->takeAt(0)) != nullptr) {
    if (item->widget())
      item->widget()->deleteLater();
    delete item;
  }

  auto addKey = [&](const QString &label, uint8_t ascii, int row, int col) {
    auto *btn = new QPushButton(label, this);
    btn->setFixedSize(30, 30);
    btn->setFocusPolicy(Qt::NoFocus);
    connect(btn, &QPushButton::clicked, this,
            [this, ascii]() { enqueueKey(ascii); });
    m_mainLayout->addWidget(btn, row, col, Qt::AlignCenter);
  };

  int row = 0;

  m_statusLabel = new QLabel("Buffer: 0", this);
  m_statusLabel->setAlignment(Qt::AlignCenter);
  m_mainLayout->addWidget(m_statusLabel, row, 0, 1, 9, Qt::AlignCenter);
  row++;

  for (int i = 0; i < 9; i++)
    addKey(QString::number(i + 1), static_cast<uint8_t>('1' + i), row, i);
  row++;

  for (int i = 0; i < 26; i++) {
    char ch = 'A' + i;
    addKey(QString(ch), static_cast<uint8_t>(ch), row + i / 9, i % 9);
  }

  m_regDescs.clear();
  m_regDescs.push_back(RegDesc{"KEY_DATA", RegDesc::RW::R, 8, 0, true});
  m_regDescs.push_back(RegDesc{"KEY_STATUS", RegDesc::RW::RW, 32, 4, true});

  unsigned bufSize = m_parameters.at(BUFSIZE).value.toUInt();
  m_extraSymbols.clear();
  m_extraSymbols.push_back(IOSymbol{"BUF_SIZE", bufSize});

  updateGeometry();
  emit regMapChanged();
}

void IOKeyboard::enqueueKey(uint8_t ascii) {
  QMutexLocker lock(&m_bufMutex);
  unsigned maxSize = m_parameters.at(BUFSIZE).value.toUInt();
  if (static_cast<unsigned>(m_keyBuffer.size()) < maxSize)
    m_keyBuffer.enqueue(ascii);
  lock.unlock();
  refreshStatusLabel();
}

void IOKeyboard::refreshStatusLabel() {
  if (!m_statusLabel)
    return;
  QMutexLocker lock(&m_bufMutex);
  int count = m_keyBuffer.size();
  lock.unlock();
  m_statusLabel->setText(QString("Buffer: %1").arg(count));
}

void IOKeyboard::keyPressEvent(QKeyEvent *event) {
  if (event->isAutoRepeat()) {
    event->ignore();
    return;
  }

  int key = event->key();
  uint8_t ascii = 0;

  if (key >= Qt::Key_A && key <= Qt::Key_Z)
    ascii = static_cast<uint8_t>('A' + (key - Qt::Key_A));
  else if (key >= Qt::Key_1 && key <= Qt::Key_9)
    ascii = static_cast<uint8_t>('1' + (key - Qt::Key_1));

  if (ascii != 0) {
    enqueueKey(ascii);
    event->accept();
  } else {
    event->ignore();
  }
}

VInt IOKeyboard::ioRead(AInt offset, unsigned) {
  if (offset == 0) {
    QMutexLocker lock(&m_bufMutex);
    uint8_t val = m_keyBuffer.isEmpty() ? 0 : m_keyBuffer.dequeue();
    lock.unlock();
    QMetaObject::invokeMethod(
        this, [this]() { refreshStatusLabel(); }, Qt::QueuedConnection);
    return val;
  }
  if (offset == 4) {
    QMutexLocker lock(&m_bufMutex);
    return static_cast<VInt>(m_keyBuffer.size());
  }
  return 0;
}

void IOKeyboard::ioWrite(AInt offset, VInt value, unsigned) {
  if (offset == 4 && value != 0) {
    QMutexLocker lock(&m_bufMutex);
    m_keyBuffer.clear();
    lock.unlock();
    QMetaObject::invokeMethod(
        this, [this]() { refreshStatusLabel(); }, Qt::QueuedConnection);
  }
}

void IOKeyboard::reset() {
  QMutexLocker lock(&m_bufMutex);
  m_keyBuffer.clear();
  lock.unlock();
  refreshStatusLabel();
}

} // namespace Ripes
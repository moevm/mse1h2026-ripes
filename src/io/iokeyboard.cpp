#include "iokeyboard.h"
#include "ioregistry.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMutexLocker>
#include <QPushButton>
#include <QVBoxLayout>

namespace Ripes {

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

static const char *kStyleKeyActive =
    "QPushButton {"
    "  background-color: #d4b0e8;"
    "  color: #333;"
    "  border: 1px solid #b090c8;"
    "  border-radius: 5px;"
    "  font: bold 11px;"
    "}";

static const char *kStyleBadge =
    "QLabel {"
    "  background-color: #3a3a5a;"
    "  color: #eeeeee;"
    "  border-radius: 3px;"
    "  padding: 1px 5px;"
    "  font: bold 11px;"
    "}";

static const char *kStyleCaption =
    "QLabel { color: #999; font: 10px; }";

static const char *kStyleFifoFull =
    "QFrame { background-color: #4488cc; border-radius: 2px; }";

static const char *kStyleFifoEmpty =
    "QFrame { background-color: #555555; border-radius: 2px; }";

static const char *kStylePanel =
    "QWidget#keyboardPanel {"
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

QString IOKeyboard::description() const {
  return "Memory-mapped keyboard with configurable FIFO buffer.\n\n"
         "Register map (8 bytes):\n"
         "  0x00  KEY_DATA    R    — dequeue next ASCII code (0 if empty)\n"
         "  0x04  KEY_STATUS  R/W  — R: buffer count, W: write !=0 to clear";
}

QHBoxLayout *IOKeyboard::addKeyRow(QVBoxLayout *parent) {
  auto *row = new QHBoxLayout();
  row->setSpacing(4);
  row->setAlignment(Qt::AlignCenter);
  parent->addLayout(row);
  return row;
}

QPushButton *IOKeyboard::createKey(const QString &label, uint8_t ascii,
                                   int w, int h) {
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
  m_keys.clear();
  m_fifoDots.clear();
  m_flashedBtn = nullptr;
  m_statusLabel = nullptr;
  m_lblData = nullptr;
  m_lblChar = nullptr;
  m_lblFifoCount = nullptr;

  if (layout()) {
    QWidget dummy;
    dummy.setLayout(layout());
  }

  auto *root = new QVBoxLayout(this);
  root->setSpacing(4);
  root->setContentsMargins(10, 10, 10, 10);

  {
    auto *r = addKeyRow(root);
    for (int i = 1; i <= 9; ++i)
      r->addWidget(createKey(QString::number(i),
                              static_cast<uint8_t>('0' + i)));
    r->addWidget(createKey("0", static_cast<uint8_t>('0')));
  }

  struct RowDef { const char *keys; };
  const RowDef rows[] = {
    {"QWERTYUIOP"},
    {"ASDFGHJKL"},
    {"ZXCVBNM"}
  };
  for (const auto &rd : rows) {
    auto *r = addKeyRow(root);
    for (int i = 0; rd.keys[i]; ++i) {
      char ch = rd.keys[i];
      r->addWidget(createKey(QString(QChar(ch)),
                              static_cast<uint8_t>(ch)));
    }
  }

  {
    auto *r = addKeyRow(root);
    r->addWidget(createKey("SPACE", static_cast<uint8_t>(' '), 220, 36));
  }

  {
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("QFrame { color: #555; }");
    root->addWidget(line);
  }

  {
    auto *bar = new QHBoxLayout();
    bar->setSpacing(8);
    bar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto addField = [&](const QString &caption, QLabel **valOut,
                        const QString &init) {
      auto *cap = new QLabel(caption, this);
      cap->setStyleSheet(kStyleCaption);
      bar->addWidget(cap);
      *valOut = new QLabel(init, this);
      (*valOut)->setStyleSheet(kStyleBadge);
      (*valOut)->setMinimumWidth(36);
      (*valOut)->setAlignment(Qt::AlignCenter);
      bar->addWidget(*valOut);
    };

    addField("STATUS", &m_statusLabel, "0");
    addField("DATA",   &m_lblData,     "0x00");
    addField("CHAR",   &m_lblChar,     " ");

    m_lblChar->setMinimumWidth(20);

    bar->addStretch(1);

    auto *fifoLabel = new QLabel("FIFO", this);
    fifoLabel->setStyleSheet(kStyleCaption);
    bar->addWidget(fifoLabel);

    const unsigned bufSize = m_parameters.at(BUFSIZE).value.toUInt();
    for (unsigned i = 0; i < bufSize; ++i) {
      auto *dot = new QFrame(this);
      dot->setFixedSize(12, 12);
      dot->setStyleSheet(kStyleFifoEmpty);
      bar->addWidget(dot);
      m_fifoDots.append(dot);
    }

    m_lblFifoCount = new QLabel(QString("0/%1").arg(bufSize), this);
    m_lblFifoCount->setStyleSheet(kStyleCaption);
    bar->addWidget(m_lblFifoCount);

    root->addLayout(bar);
  }

  m_regDescs.clear();
  m_regDescs.push_back(RegDesc{"KEY_DATA",   RegDesc::RW::R,  8,  0, true});
  m_regDescs.push_back(RegDesc{"KEY_STATUS", RegDesc::RW::RW, 32, 4, true});

  unsigned bufSize = m_parameters.at(BUFSIZE).value.toUInt();
  m_extraSymbols.clear();
  m_extraSymbols.push_back(IOSymbol{"BUF_SIZE", bufSize});

  updateGeometry();
  refreshStatusLabel();
  emit regMapChanged();
}

void IOKeyboard::flashKey(uint8_t ascii) {
  clearFlash();
  auto it = m_keys.find(ascii);
  if (it != m_keys.end()) {
    m_flashedBtn = it.value();
    m_flashedBtn->setStyleSheet(kStyleKeyActive);
  }
}

void IOKeyboard::clearFlash() {
  if (m_flashedBtn) {
    m_flashedBtn->setStyleSheet(kStyleKey);
    m_flashedBtn = nullptr;
  }
}

void IOKeyboard::updateFifoDots(int used) {
  for (int i = 0; i < m_fifoDots.size(); ++i)
    m_fifoDots[i]->setStyleSheet(i < used ? kStyleFifoFull
                                           : kStyleFifoEmpty);
}

void IOKeyboard::refreshStatusLabel() {
  QMutexLocker lock(&m_bufMutex);
  int count = m_keyBuffer.size();
  uint8_t ch = m_lastKey;
  lock.unlock();

  if (m_statusLabel)
    m_statusLabel->setText(QString::number(count));

  if (m_lblData)
    m_lblData->setText(
        QString("0x%1").arg(ch, 2, 16, QChar('0')));

  if (m_lblChar) {
    if (ch >= 0x20 && ch < 0x7F)
      m_lblChar->setText(QString(QChar(ch)));
    else
      m_lblChar->setText(" ");
  }

  updateFifoDots(count);

  const unsigned bufSize = m_parameters.at(BUFSIZE).value.toUInt();
  if (m_lblFifoCount)
    m_lblFifoCount->setText(QString("%1/%2").arg(count).arg(bufSize));
}

void IOKeyboard::enqueueKey(uint8_t ascii) {
  flashKey(ascii);

  {
    QMutexLocker lock(&m_bufMutex);
    unsigned maxSize = m_parameters.at(BUFSIZE).value.toUInt();
    if (static_cast<unsigned>(m_keyBuffer.size()) < maxSize)
      m_keyBuffer.enqueue(ascii);
    m_lastKey = ascii;
  }

  refreshStatusLabel();
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
  else if (key >= Qt::Key_0 && key <= Qt::Key_9)
    ascii = static_cast<uint8_t>('0' + (key - Qt::Key_0));
  else if (key == Qt::Key_Space)
    ascii = static_cast<uint8_t>(' ');

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
  {
    QMutexLocker lock(&m_bufMutex);
    m_keyBuffer.clear();
    m_lastKey = 0;
  }
  clearFlash();
  refreshStatusLabel();
}

void IOKeyboard::parameterChanged(unsigned) {
  {
    QMutexLocker lock(&m_bufMutex);
    unsigned maxSize = m_parameters.at(BUFSIZE).value.toUInt();
    while (static_cast<unsigned>(m_keyBuffer.size()) > maxSize)
      m_keyBuffer.dequeue();
  }
  updateLayout();
}

}
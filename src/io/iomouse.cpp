#include "iomouse.h"
#include "ioregistry.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>

namespace Ripes {

IOMouse::IOMouse(QWidget *parent) : IOBase(IOType::MOUSE, parent) {
  m_parameters[WIDTH] = IOParam(WIDTH, "Width", 200, true, 100, 800);
  m_parameters[HEIGHT] = IOParam(HEIGHT, "Height", 150, true, 100, 600);

  m_regDescs.push_back(RegDesc{"X", RegDesc::RW::R, 32, X * 4, true});
  m_regDescs.push_back(RegDesc{"Y", RegDesc::RW::R, 32, Y * 4, true});
  m_regDescs.push_back(
      RegDesc{"LBUTTON", RegDesc::RW::R, 1, LBUTTON * 4, true});
  m_regDescs.push_back(
      RegDesc{"RBUTTON", RegDesc::RW::R, 1, RBUTTON * 4, true});
  m_regDescs.push_back(
      RegDesc{"SCROLL", RegDesc::RW::RW, 32, SCROLL * 4, true});

<<<<<<< Updated upstream
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // ---- Width spin box ----
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(310, 800);
    m_widthSpin->setValue(static_cast<int>(m_width));
    m_widthSpin->setFixedSize(68, 22);
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) {
                m_width = static_cast<unsigned>(v);
                if (m_mouseX >= static_cast<int>(m_width))
                    m_mouseX = static_cast<int>(m_width) - 1;
                m_extraSymbols.clear();
                m_extraSymbols.push_back(IOSymbol{"WIDTH", m_width});
                m_extraSymbols.push_back(IOSymbol{"HEIGHT", m_height});
                applySize();
            });

    // ---- Height spin box ----
    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(100, 600);
    m_heightSpin->setValue(static_cast<int>(m_height));
    m_heightSpin->setFixedSize(68, 22);
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) {
                m_height = static_cast<unsigned>(v);
                if (m_mouseY >= static_cast<int>(m_height))
                    m_mouseY = static_cast<int>(m_height) - 1;
                m_extraSymbols.clear();
                m_extraSymbols.push_back(IOSymbol{"WIDTH", m_width});
                m_extraSymbols.push_back(IOSymbol{"HEIGHT", m_height});
                applySize();
            });

    // ---- Reset button ----
    m_resetBtn = new QPushButton("Reset", this);
    m_resetBtn->setFixedSize(52, 22);
    connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
        reset();
        update();
    });

    // Force initial size
    applySize();
=======
  updateExtraSymbols();
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
>>>>>>> Stashed changes
}

void IOMouse::updateExtraSymbols() {
  const unsigned w = m_parameters.at(WIDTH).value.toUInt();
  const unsigned h = m_parameters.at(HEIGHT).value.toUInt();
  m_extraSymbols.clear();
  m_extraSymbols.push_back(IOSymbol{"WIDTH", w});
  m_extraSymbols.push_back(IOSymbol{"HEIGHT", h});
}

void IOMouse::parameterChanged(unsigned) {
  updateExtraSymbols();
  updateGeometry();
  update();
}

void IOMouse::reset() {
  m_mouseX = 0;
  m_mouseY = 0;
  m_lButton = 0;
  m_rButton = 0;
  m_scroll = 0;
}

QString IOMouse::description() const {
  QStringList desc;
  desc << "Tracks mouse position, button state, and scroll within the "
          "widget area.";
  desc << "X, Y = coordinates; LBUTTON, RBUTTON = press state (0/1); "
          "SCROLL = cumulative scroll delta.";
  return desc.join('\n');
}

VInt IOMouse::ioRead(AInt offset, unsigned) {
  switch (offset) {
  case X * 4:
    return m_mouseX;
  case Y * 4:
    return m_mouseY;
  case LBUTTON * 4:
    return m_lButton;
  case RBUTTON * 4:
    return m_rButton;
  case SCROLL * 4:
    return m_scroll;
  }
  return 0;
}

void IOMouse::ioWrite(AInt offset, VInt value, unsigned) {
  if (offset == SCROLL * 4) {
    m_scroll = value;
  }
}

void IOMouse::mouseMoveEvent(QMouseEvent *event) {
  const int w = m_parameters.at(WIDTH).value.toInt();
  const int h = m_parameters.at(HEIGHT).value.toInt();
  m_mouseX = qBound(0, event->pos().x(), w - 1);
  m_mouseY = qBound(0, event->pos().y(), h - 1);
  emit scheduleUpdate();
}

void IOMouse::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton)
    m_lButton = 1;
  if (event->button() == Qt::RightButton)
    m_rButton = 1;
  emit scheduleUpdate();
}

void IOMouse::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton)
    m_lButton = 0;
  if (event->button() == Qt::RightButton)
    m_rButton = 0;
  emit scheduleUpdate();
}

void IOMouse::wheelEvent(QWheelEvent *event) {
  m_scroll += event->angleDelta().y() / 120;
  emit scheduleUpdate();
}

QSize IOMouse::minimumSizeHint() const {
  const int w = m_parameters.at(WIDTH).value.toInt();
  const int h = m_parameters.at(HEIGHT).value.toInt();
  return QSize(w, h);
}

void IOMouse::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int w = m_parameters.at(WIDTH).value.toInt();
  const int h = m_parameters.at(HEIGHT).value.toInt();

  // Background
  painter.fillRect(rect(), QColor(240, 240, 240));
  painter.fillRect(0, 0, w, h, Qt::white);

  // Border
  painter.setPen(QPen(Qt::black, 1));
  painter.drawRect(0, 0, w - 1, h - 1);

  // Crosshair guides
  painter.setPen(QPen(Qt::gray, 1, Qt::DashLine));
  painter.drawLine(m_mouseX, 0, m_mouseX, h);
  painter.drawLine(0, m_mouseY, w, m_mouseY);

  // Cursor dot (color indicates button state)
  painter.setPen(Qt::NoPen);
  QColor dotColor = Qt::black;
  if (m_lButton)
    dotColor = Qt::red;
  else if (m_rButton)
    dotColor = Qt::blue;
  painter.setBrush(dotColor);
  painter.drawEllipse(QPoint(m_mouseX, m_mouseY), 4, 4);

  // Coordinate info
  painter.setPen(Qt::black);
  QString info =
      QString("(%1, %2) SCR:%3").arg(m_mouseX).arg(m_mouseY).arg(m_scroll);
  painter.drawText(4, h - 4, info);

  painter.end();
}

} // namespace Ripes
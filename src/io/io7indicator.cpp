#include "io7indicator.h"
#include "ioregistry.h"

#include <QPainter>
#include <QSizePolicy>

namespace Ripes {

namespace {

struct SegColor {
  QColor on;
  QColor off;
};

static const SegColor s_segColors[] = {
    {{255, 30, 30}, {50, 10, 10}},
    {{30, 255, 30}, {10, 50, 10}},
    {{50, 130, 255}, {15, 25, 55}},
    {{255, 220, 30}, {55, 48, 10}},
    {{230, 230, 240}, {45, 45, 50}},
};

static constexpr int s_numColors =
    static_cast<int>(sizeof(s_segColors) / sizeof(s_segColors[0]));
static constexpr qreal s_digitAspect = 0.62;
static constexpr qreal s_gapRatio = 0.14;

QPolygonF horizontalSegment(const QRectF &rect) {
  const qreal bevel = qMin(rect.height() * 0.8, rect.width() / 3.0);
  const qreal cy = rect.center().y();

  QPolygonF polygon;
  polygon << QPointF(rect.left() + bevel, rect.top())
          << QPointF(rect.right() - bevel, rect.top())
          << QPointF(rect.right(), cy)
          << QPointF(rect.right() - bevel, rect.bottom())
          << QPointF(rect.left() + bevel, rect.bottom())
          << QPointF(rect.left(), cy);
  return polygon;
}

QPolygonF verticalSegment(const QRectF &rect) {
  const qreal bevel = qMin(rect.width() * 0.8, rect.height() / 3.0);
  const qreal cx = rect.center().x();

  QPolygonF polygon;
  polygon << QPointF(rect.left(), rect.top() + bevel)
          << QPointF(cx, rect.top())
          << QPointF(rect.right(), rect.top() + bevel)
          << QPointF(rect.right(), rect.bottom() - bevel)
          << QPointF(cx, rect.bottom())
          << QPointF(rect.left(), rect.bottom() - bevel);
  return polygon;
}

}

IO7Indicator::IO7Indicator(QWidget *parent)
    : IOBase(IOType::SEVEN_SEGMENT, parent) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  m_parameters[DIGIT_SIZE] =
      IOParam(DIGIT_SIZE, "Digit size", 64, true, 30, 120);
  m_parameters[COLOR] =
      IOParam(COLOR, "Color", 0, true, 0, s_numColors - 1);

  m_digitValues.assign(NUM_DIGITS, 0);
  setMinimumSize(minimumSizeHint());
  updateRegDescs();
}

QString IO7Indicator::description() const {
  QStringList desc;
  desc << "7-segment display with " + QString::number(NUM_DIGITS) +
              " digits.";
  desc << "Each digit is a 4-byte word (offset = index * 4).";
  desc << "Bits 0-6 = segments a-g, bit 7 = decimal point.";
  return desc.join('\n');
}

unsigned IO7Indicator::byteSize() const {
  return NUM_DIGITS * 4;
}

void IO7Indicator::updateRegDescs() {
  m_regDescs.clear();
  for (unsigned i = 0; i < NUM_DIGITS; ++i) {
    m_regDescs.push_back(
        RegDesc{QString("Digit %1").arg(i), RegDesc::RW::RW, 8,
                static_cast<AInt>(i * 4), true});
  }

  m_extraSymbols.clear();
  m_extraSymbols.push_back(IOSymbol{"N_DIGITS", NUM_DIGITS});

  updateGeometry();
  emit regMapChanged();
}

void IO7Indicator::parameterChanged(unsigned) {
  setMinimumSize(minimumSizeHint());
  updateGeometry();
  update();
}

VInt IO7Indicator::ioRead(AInt offset, unsigned size) {
  if (size != 4 || (offset % 4) != 0)
    return 0;

  const unsigned idx = static_cast<unsigned>(offset / 4);
  if (idx >= m_digitValues.size())
    return 0;

  return static_cast<VInt>(m_digitValues[idx]);
}

void IO7Indicator::ioWrite(AInt offset, VInt value, unsigned size) {
  if (size != 4 || (offset % 4) != 0)
    return;

  const unsigned idx = static_cast<unsigned>(offset / 4);
  if (idx >= m_digitValues.size())
    return;

  m_digitValues[idx] = static_cast<uint8_t>(value & 0xFF);
  emit scheduleUpdate();
}

void IO7Indicator::reset() {
  std::fill(m_digitValues.begin(), m_digitValues.end(), 0);
  emit scheduleUpdate();
}

QSize IO7Indicator::minimumSizeHint() const {
  const int digitH = m_parameters.at(DIGIT_SIZE).value.toInt();
  const int digitW = qRound(digitH * s_digitAspect);
  const int gap = qMax(4, digitH / 7);
  const int margin = qMax(8, digitH / 6);
  const int n = static_cast<int>(NUM_DIGITS);

  return QSize(n * digitW + (n - 1) * gap + margin * 2,
               digitH + margin * 2);
}

void IO7Indicator::drawDigit(QPainter &p, int x, int y, int w, int h,
                             uint8_t seg) {
  if (w <= 0 || h <= 0)
    return;

  const int ci =
      qBound(0, m_parameters.at(COLOR).value.toInt(), s_numColors - 1);
  const QColor &on = s_segColors[ci].on;
  const QColor &off = s_segColors[ci].off;

  const qreal thickness = qMax<qreal>(2.0, qMin(w, h) * 0.09);
  const qreal xPad = thickness * 0.75;
  const qreal yPad = thickness * 0.45;
  const qreal midY = y + h / 2.0;

  const qreal topY = y + yPad;
  const qreal bottomY = y + h - yPad - thickness;
  const qreal horizLeft = x + xPad + thickness * 0.2;
  const qreal horizRight = x + w - xPad - thickness * 0.2;
  const qreal horizWidth = qMax<qreal>(1.0, horizRight - horizLeft);

  const qreal leftX = x + xPad;
  const qreal rightX = x + w - xPad - thickness;

  const qreal upperTop = topY + thickness * 0.7;
  const qreal upperBottom = midY - thickness * 0.7;
  const qreal lowerTop = midY + thickness * 0.2;
  const qreal lowerBottom = bottomY - thickness * 0.2;

  const qreal upperHeight = qMax<qreal>(1.0, upperBottom - upperTop);
  const qreal lowerHeight = qMax<qreal>(1.0, lowerBottom - lowerTop);

  const QRectF segments[7] = {
      QRectF(horizLeft, topY, horizWidth, thickness),
      QRectF(rightX, upperTop, thickness, upperHeight),
      QRectF(rightX, lowerTop, thickness, lowerHeight),
      QRectF(horizLeft, bottomY, horizWidth, thickness),
      QRectF(leftX, lowerTop, thickness, lowerHeight),
      QRectF(leftX, upperTop, thickness, upperHeight),
      QRectF(horizLeft, midY - thickness / 2.0, horizWidth, thickness),
  };

  p.setPen(Qt::NoPen);

  for (int i = 0; i < 7; ++i) {
    p.setBrush((seg >> i) & 1 ? on : off);
    if (i == 0 || i == 3 || i == 6)
      p.drawPolygon(horizontalSegment(segments[i]));
    else
      p.drawPolygon(verticalSegment(segments[i]));
  }

  const qreal dotRadius = thickness * 0.34;
  const QPointF dotCenter(x + w - dotRadius * 1.8, y + h - dotRadius * 1.8);

  p.setBrush((seg >> 7) & 1 ? on : off);
  p.drawEllipse(dotCenter, dotRadius, dotRadius);
}

void IO7Indicator::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), Qt::black);

  const int n = static_cast<int>(m_digitValues.size());
  if (n == 0)
    return;

  const int margin = qMax(6, qMin(width(), height()) / 18);
  const QRectF area = rect().adjusted(margin, margin, -margin, -margin);
  if (area.width() <= 0 || area.height() <= 0)
    return;

  const qreal digitHFromWidth =
      area.width() / (n * s_digitAspect + (n - 1) * s_gapRatio);
  const qreal digitH = qMax<qreal>(1.0, qMin(area.height(), digitHFromWidth));
  const qreal digitW = digitH * s_digitAspect;
  const qreal gap = qMax<qreal>(2.0, digitH * s_gapRatio);
  const qreal totalW = n * digitW + (n - 1) * gap;
  const qreal startX = area.left() + (area.width() - totalW) / 2.0;
  const qreal startY = area.top() + (area.height() - digitH) / 2.0;

  for (int i = 0; i < n; ++i) {
    const qreal x = startX + i * (digitW + gap);
    drawDigit(painter, qRound(x), qRound(startY), qRound(digitW),
              qRound(digitH), m_digitValues[i]);
  }
}

}

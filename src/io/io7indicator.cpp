#include "io7indicator.h"
#include "ioregistry.h"

#include <QPainter>
<<<<<<< Updated upstream
#include <QPushButton>
#include <QSpinBox>
#include <algorithm>
=======
#include <QSizePolicy>
>>>>>>> Stashed changes

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

<<<<<<< Updated upstream
static constexpr int NUM_COLORS =
    static_cast<int>(sizeof(s_colors) / sizeof(s_colors[0]));
static constexpr unsigned MAX_DIGITS = 8;
static std::vector<IOSymbol> s_extraSymbols;
=======
static constexpr int s_numColors =
    static_cast<int>(sizeof(s_segColors) / sizeof(s_segColors[0]));
static constexpr qreal s_digitAspect = 0.62;
static constexpr qreal s_gapRatio = 0.14;
>>>>>>> Stashed changes

QPolygonF horizontalSegment(const QRectF &rect) {
  const qreal bevel = qMin(rect.height() * 0.8, rect.width() / 3.0);
  const qreal cy = rect.center().y();

<<<<<<< Updated upstream
static const char *const kSpinStyle =
    "QSpinBox { background: #FFFFFF; color: #333; border: 1px solid #CCC;"
    " border-radius: 3px; padding: 2px 4px; font-size: 11px; }"
    "QSpinBox:focus { border-color: #6699CC; }";

static const char *const kComboStyle =
    "QComboBox { background: #FFFFFF; color: #333; border: 1px solid #CCC;"
    " border-radius: 3px; padding: 2px 8px; font-size: 11px; }"
    "QComboBox:hover { border-color: #AAA; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #FFFFFF; color: #333;"
    " selection-background-color: #D0E0F0; selection-color: #333;"
    " border: 1px solid #CCC; }";

static const char *const kBtnStyle =
    "QPushButton { background: #FFFFFF; color: #444; border: 1px solid #CCC;"
    " border-radius: 3px; padding: 2px 10px;"
    " font-family: monospace; font-size: 11px; }"
    "QPushButton:hover { background: #E8E8E8; border-color: #AAA; }"
    "QPushButton:pressed { background: #D0D0D0; }";

static const char *const kHexLabelStyle =
    "QLabel { background: #FFFFFF; color: #555; border: 1px solid #DDD;"
    " border-radius: 10px; padding: 2px 8px;"
    " font-family: monospace; font-size: 10px; }";

// SegmentDisplayWidget – renders display
class SegmentDisplayWidget : public QWidget {
public:
  explicit SegmentDisplayWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }

  void setDigitValues(const std::vector<uint8_t> &v) {
    m_vals = v;
    update();
  }

  void setColorIndex(int i) {
    m_ci = qBound(0, i, NUM_COLORS - 1);
    update();
  }

  void setNumDigits(int n) {
    m_n = qMax(1, n);
    updateGeometry();
    update();
  }

protected:
  QSize sizeHint() const override { return {340, 130}; }
  QSize minimumSizeHint() const override { return {140, 70}; }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(QColor(40, 40, 52), 1));
    p.setBrush(QColor(15, 15, 22));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

    if (m_n <= 0)
      return;

    const int pad = 10;
    const int regH = 16;
    int availH = height() - 2 * pad - regH;
    int availW = width() - 2 * pad;
    int digitH = qMax(24, availH);
    int digitW = digitH * 2 / 3;
    int gap = qMax(4, digitH / 7);
    int totalW = m_n * digitW + (m_n - 1) * gap;

    if (totalW > availW && m_n > 0) {
      qreal d = m_n * (2.0 / 3.0) + (m_n - 1) * (1.0 / 7.0);
      if (d > 0)
        digitH = qMin(digitH, static_cast<int>(availW / d));
      digitH = qMax(24, digitH);
      digitW = digitH * 2 / 3;
      gap = qMax(4, digitH / 7);
      totalW = m_n * digitW + (m_n - 1) * gap;
    }

    const int x0 = (width() - totalW) / 2;
    const int y0 = pad + qMax(0, (availH - digitH) / 2);

    // each digit
    for (int i = 0; i < m_n; i++) {
      const int dx = x0 + i * (digitW + gap);
      uint8_t v = (i < static_cast<int>(m_vals.size())) ? m_vals[i] : 0;
      drawDigit(p, dx, y0, digitW, digitH, v);
    }

    // REG
    QFont f = font();
    f.setPixelSize(qBound(8, digitH / 8, 11));
    p.setFont(f);
    p.setPen(QColor(110, 110, 125));
    for (int i = 0; i < m_n; i++) {
      const int dx = x0 + i * (digitW + gap);
      QRect lr(dx, y0 + digitH + 2, digitW, regH);
      p.drawText(lr, Qt::AlignHCenter | Qt::AlignTop,
                 QStringLiteral("REG%1").arg(i));
    }
  }

private:
  void drawDigit(QPainter &p, int x, int y, int w, int h, uint8_t seg) {
    const auto &c = s_colors[m_ci];

    const qreal t = qMin(w, h) * 0.11;
    const qreal g = t * 0.25;

    const qreal lx = x + t, rx = x + w - t;
    const qreal ty = y + t, my = y + h / 2.0, by = y + h - t;

    auto hSeg = [t](qreal x1, qreal yc, qreal x2) -> QPolygonF {
      const qreal ht = t / 2.0;
      return {{x1, yc}, {x1 + ht, yc - ht}, {x2 - ht, yc - ht},
              {x2, yc}, {x2 - ht, yc + ht}, {x1 + ht, yc + ht}};
    };
    auto vSeg = [t](qreal xc, qreal y1, qreal y2) -> QPolygonF {
      const qreal ht = t / 2.0;
      return {{xc, y1}, {xc + ht, y1 + ht}, {xc + ht, y2 - ht},
              {xc, y2}, {xc - ht, y2 - ht}, {xc - ht, y1 + ht}};
    };

    const QPolygonF polys[7] = {
        hSeg(lx + g, ty, rx - g), // a  top
        vSeg(rx, ty + g, my - g), // b  right-top
        vSeg(rx, my + g, by - g), // c  right-bot
        hSeg(lx + g, by, rx - g), // d  bottom
        vSeg(lx, my + g, by - g), // e  left-bot
        vSeg(lx, ty + g, my - g), // f  left-top
        hSeg(lx + g, my, rx - g), // g  middle
    };

    p.setPen(Qt::NoPen);

    for (int i = 0; i < 7; i++) {
      if (!((seg >> i) & 1))
        continue;
      QColor gc = c.glow;
      gc.setAlpha(35);
      p.setBrush(gc);
      QPointF cen;
      for (const auto &pt : polys[i])
        cen += pt;
      cen /= polys[i].size();
      QPolygonF gp;
      for (const auto &pt : polys[i])
        gp << cen + (pt - cen) * 1.5;
      p.drawPolygon(gp);
    }

    for (int i = 0; i < 7; i++) {
      p.setBrush((seg >> i) & 1 ? c.on : c.off);
      p.drawPolygon(polys[i]);
    }

    const qreal dpR = t * 0.55;
    const QPointF dpC(x + w + t * 0.4, by);
    const bool dpOn = (seg >> 7) & 1;
    if (dpOn) {
      QColor gc = c.glow;
      gc.setAlpha(35);
      p.setBrush(gc);
      p.drawEllipse(dpC, dpR * 1.8, dpR * 1.8);
    }
    p.setBrush(dpOn ? c.on : c.off);
    p.drawEllipse(dpC, dpR, dpR);
  }

  std::vector<uint8_t> m_vals;
  int m_ci = 0;
  int m_n = 4;
};

unsigned IO7Indicator::numDigits() const {
  auto it = m_parameters.find(DIGITS);
  if (it == m_parameters.end())
    return 4;
  unsigned n = it->second.value.toUInt();
  return (n >= 1 && n <= 8) ? n : 4;
}

void IO7Indicator::rebuildRegDescs() {
  const unsigned n = numDigits();
  m_regDescs.clear();
  m_regDescs.reserve(n);
  for (unsigned i = 0; i < n; i++) {
    m_regDescs.push_back(RegDesc{QString("Digit %1").arg(i), RegDesc::RW::RW, 8,
                                 static_cast<AInt>(i * 4), true});
  }
=======
  QPolygonF polygon;
  polygon << QPointF(rect.left() + bevel, rect.top())
          << QPointF(rect.right() - bevel, rect.top())
          << QPointF(rect.right(), cy)
          << QPointF(rect.right() - bevel, rect.bottom())
          << QPointF(rect.left() + bevel, rect.bottom())
          << QPointF(rect.left(), cy);
  return polygon;
>>>>>>> Stashed changes
}

QPolygonF verticalSegment(const QRectF &rect) {
  const qreal bevel = qMin(rect.width() * 0.8, rect.height() / 3.0);
  const qreal cx = rect.center().x();

<<<<<<< Updated upstream
    s_extraSymbols.push_back(IOSymbol{"SEVENSEG_BASE", 0});
    s_extraSymbols.push_back(
        IOSymbol{"SEVENSEG_SIZE", static_cast<AInt>(MAX_DIGITS * 4)});
    s_extraSymbols.push_back(
        IOSymbol{"SEVENSEG_N_DIGITS", static_cast<AInt>(MAX_DIGITS)});

    for (unsigned i = 0; i < MAX_DIGITS; ++i) {
      s_extraSymbols.push_back(IOSymbol{QStringLiteral("SEVENSEG_%1").arg(i),
                                        static_cast<AInt>(i * 4)});
    }
  }
=======
  QPolygonF polygon;
  polygon << QPointF(rect.left(), rect.top() + bevel)
          << QPointF(cx, rect.top())
          << QPointF(rect.right(), rect.top() + bevel)
          << QPointF(rect.right(), rect.bottom() - bevel)
          << QPointF(cx, rect.bottom())
          << QPointF(rect.left(), rect.bottom() - bevel);
  return polygon;
>>>>>>> Stashed changes
}

}

IO7Indicator::IO7Indicator(QWidget *parent)
    : IOBase(IOType::SEVEN_SEGMENT, parent) {
<<<<<<< Updated upstream
  m_parameters[DIGITS] = IOParam(DIGITS, "# Digits", 4, true, 1, 8);
=======
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

>>>>>>> Stashed changes
  m_parameters[DIGIT_SIZE] =
      IOParam(DIGIT_SIZE, "Digit size", 64, true, 30, 120);
  m_parameters[COLOR] =
      IOParam(COLOR, "Color", 0, true, 0, s_numColors - 1);

<<<<<<< Updated upstream
  m_digitValues.assign(numDigits(), 0);
  rebuildRegDescs();
  initExtraSymbols();
  buildUI();
  connect(this, &IOBase::scheduleUpdate, this, [this]() { refreshDisplay(); });
=======
  m_digitValues.assign(NUM_DIGITS, 0);
  setMinimumSize(minimumSizeHint());
  updateRegDescs();
>>>>>>> Stashed changes
}

QString IO7Indicator::description() const {
  QStringList desc;
  desc << "7-segment display with " + QString::number(NUM_DIGITS) +
              " digits.";
  desc << "Each digit is a 4-byte word (offset = index * 4).";
  desc << "Bits 0-6 = segments a-g, bit 7 = decimal point.";
  return desc.join('\n');
}

<<<<<<< Updated upstream
unsigned IO7Indicator::byteSize() const { return numDigits() * 4; }

void IO7Indicator::parameterChanged(unsigned /*ID*/) {
  if (m_updating)
    return;
  m_updating = true;

  const unsigned n = numDigits();
  m_digitValues.resize(n, 0);
  rebuildRegDescs();

  if (m_displayWidget) {
    m_displayWidget->setNumDigits(static_cast<int>(n));
    m_displayWidget->setColorIndex(
        m_parameters.count(COLOR) ? m_parameters.at(COLOR).value.toInt() : 0);
    m_displayWidget->setDigitValues(m_digitValues);
  }

  if (m_spinDigits && m_spinDigits->value() != static_cast<int>(n)) {
    m_spinDigits->blockSignals(true);
    m_spinDigits->setValue(static_cast<int>(n));
    m_spinDigits->blockSignals(false);
  }
  if (m_comboColor) {
    int ci =
        m_parameters.count(COLOR) ? m_parameters.at(COLOR).value.toInt() : 0;
    if (m_comboColor->currentIndex() != ci) {
      m_comboColor->blockSignals(true);
      m_comboColor->setCurrentIndex(ci);
      m_comboColor->blockSignals(false);
    }
  }
=======
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
>>>>>>> Stashed changes

  rebuildHexLabels();

  updateGeometry();
<<<<<<< Updated upstream
  update();
  emit regMapChanged();
  emit sizeChanged();
=======
  emit regMapChanged();
}
>>>>>>> Stashed changes

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
<<<<<<< Updated upstream
  auto it = m_parameters.find(DIGIT_SIZE);
  const int h = (it != m_parameters.end()) ? it->second.value.toInt() : 64;
  const int w = h * 2 / 3;
  const int gap = 6;
  const int n = static_cast<int>(numDigits());
  const int totalW = n * w + (n - 1) * gap;
  const int pad = 16;
  return QSize(totalW + pad, h + pad);
=======
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

  const qreal thickness = qMax<qreal>(2.0, qMin(w, h) * 0.10);
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
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
void IO7Indicator::drawDigit(QPainter &p, int x, int y, int w, int h,
                             uint8_t seg) {
  const QColor on(255, 30, 30), off(50, 10, 10);
  const qreal t = qMin(w, h) * 0.12;

  const qreal l = x + t, r = x + w - t;
  const qreal top = y + t, mid = y + h / 2.0, bot = y + h - t;

  const QLineF lines[7] = {
      {l, top, r, top}, // a — top
      {r, top, r, mid}, // b — right top
      {r, mid, r, bot}, // c — right bot
      {l, bot, r, bot}, // d — bot
      {l, mid, l, bot}, // e — left bot
      {l, top, l, mid}, // f — left top
      {l, mid, r, mid}, // g — mid
  };

  for (int i = 0; i < 7; i++) {
    p.setPen(QPen((seg >> i) & 1 ? on : off, t, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(lines[i]);
  }
  p.setPen(Qt::NoPen);
  p.setBrush((seg >> 7) & 1 ? on : off);
  p.drawEllipse(QPointF(x + w + t * 0.5, bot), t * 0.5, t * 0.5);
}

void IO7Indicator::buildUI() {
  setMinimumSize(380, 230);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(30, 30, 42));
  setPalette(pal);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(5);

  auto *topBar = new QHBoxLayout;
  topBar->setSpacing(8);

  auto *lblDig = new QLabel(QStringLiteral("# Digits"));
  lblDig->setStyleSheet("color:#bbb; font-size:11px;");
  topBar->addWidget(lblDig);

  m_spinDigits = new QSpinBox;
  m_spinDigits->setRange(1, 8);
  m_spinDigits->setValue(static_cast<int>(numDigits()));
  m_spinDigits->setFixedWidth(60);
  m_spinDigits->setStyleSheet(kSpinStyle);
  topBar->addWidget(m_spinDigits);
  connect(m_spinDigits, QOverload<int>::of(&QSpinBox::valueChanged), this,
          [this](int v) { setParameter(DIGITS, v); });

  topBar->addSpacing(16);

  auto *lblCol = new QLabel(QStringLiteral("Color"));
  lblCol->setStyleSheet("color:#bbb; font-size:11px;");
  topBar->addWidget(lblCol);

  m_comboColor = new QComboBox;
  for (int i = 0; i < NUM_COLORS; i++)
    m_comboColor->addItem(s_colors[i].name);
  m_comboColor->setCurrentIndex(0);
  m_comboColor->setFixedWidth(100);
  m_comboColor->setStyleSheet(kComboStyle);
  topBar->addWidget(m_comboColor);
  connect(m_comboColor, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int i) { setParameter(COLOR, i); });

  topBar->addStretch();
  root->addLayout(topBar);

  m_displayWidget = new SegmentDisplayWidget(this);
  m_displayWidget->setNumDigits(static_cast<int>(numDigits()));
  m_displayWidget->setColorIndex(0);
  m_displayWidget->setDigitValues(m_digitValues);
  root->addWidget(m_displayWidget, /*stretch=*/1);

  auto *testBar = new QHBoxLayout;
  testBar->setSpacing(6);

  auto *lblTest = new QLabel(QStringLiteral("Quick test:"));
  lblTest->setStyleSheet("color:#999; font-size:11px;");
  testBar->addWidget(lblTest);

  auto addBtn = [&](const QString &label, auto slot) {
    auto *b = new QPushButton(label);
    b->setFixedHeight(24);
    b->setStyleSheet(kBtnStyle);
    testBar->addWidget(b);
    connect(b, &QPushButton::clicked, this, slot);
  };

  addBtn(QStringLiteral("1234"), [this]() {
    std::vector<uint8_t> v(numDigits(), 0);
    const uint8_t d[] = {SEG_MAP[1], SEG_MAP[2], SEG_MAP[3], SEG_MAP[4]};
    for (unsigned i = 0; i < qMin(numDigits(), 4u); i++)
      v[i] = d[i];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("-5"), [this]() {
    std::vector<uint8_t> v(numDigits(), 0);
    if (numDigits() >= 1)
      v[0] = SEG_MINUS;
    if (numDigits() >= 2)
      v[1] = SEG_MAP[5];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("12.34"), [this]() {
    std::vector<uint8_t> v(numDigits(), 0);
    if (numDigits() >= 1)
      v[0] = SEG_MAP[1];
    if (numDigits() >= 2)
      v[1] = SEG_MAP[2] | SEG_DP;
    if (numDigits() >= 3)
      v[2] = SEG_MAP[3];
    if (numDigits() >= 4)
      v[3] = SEG_MAP[4];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("dEAd"), [this]() {
    std::vector<uint8_t> v(numDigits(), 0);
    const uint8_t d[] = {SEG_MAP[0xD], SEG_MAP[0xE], SEG_MAP[0xA],
                         SEG_MAP[0xD]};
    for (unsigned i = 0; i < qMin(numDigits(), 4u); i++)
      v[i] = d[i];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("Clear"),
         [this]() { applyQuickTest(std::vector<uint8_t>(numDigits(), 0)); });

  testBar->addStretch();
  root->addLayout(testBar);

  m_hexBarLayout = new QHBoxLayout;
  m_hexBarLayout->setSpacing(6);
  root->addLayout(m_hexBarLayout);

  rebuildHexLabels();
}

void IO7Indicator::refreshDisplay() {
  if (m_displayWidget) {
    m_displayWidget->setDigitValues(m_digitValues);
    m_displayWidget->setNumDigits(static_cast<int>(numDigits()));
  }
  updateRegLabels();
}

void IO7Indicator::rebuildHexLabels() {
  // old labels
  for (auto *lbl : m_regHexLabels)
    lbl->deleteLater();
  m_regHexLabels.clear();

  while (m_hexBarLayout->count() > 0) {
    auto *item = m_hexBarLayout->takeAt(0);
    delete item;
  }

  // new labels
  for (unsigned i = 0; i < numDigits(); i++) {
    auto *lbl = new QLabel;
    lbl->setMinimumWidth(80);
    lbl->setMinimumHeight(20);
    lbl->setStyleSheet(kHexLabelStyle);
    lbl->setAlignment(Qt::AlignCenter);
    m_hexBarLayout->addWidget(lbl);
    m_regHexLabels.push_back(lbl);
  }
  m_hexBarLayout->addStretch();

  updateRegLabels();
}

void IO7Indicator::updateRegLabels() {
  for (unsigned i = 0; i < m_regHexLabels.size(); i++) {
    unsigned offset = i * 4;
    uint8_t val = (i < m_digitValues.size()) ? m_digitValues[i] : 0;
    QString oStr = QString("%1").arg(offset, 2, 16, QChar('0')); // lowercase
    QString vStr = QString("%1").arg(val, 2, 16, QChar('0')).toUpper();
    m_regHexLabels[i]->setText(QStringLiteral("[%1] 0x%2").arg(oStr, vStr));
  }
}

void IO7Indicator::applyQuickTest(const std::vector<uint8_t> &values) {
  const unsigned n = numDigits();
  for (unsigned i = 0; i < n; i++)
    m_digitValues[i] = (i < values.size()) ? values[i] : 0;
  emit scheduleUpdate();
}

} // namespace Ripes
=======
  for (int i = 0; i < n; ++i) {
    const qreal x = startX + i * (digitW + gap);
    drawDigit(painter, qRound(x), qRound(startY), qRound(digitW),
              qRound(digitH), m_digitValues[i]);
  }
}

}
>>>>>>> Stashed changes

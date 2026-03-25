#include "io7indicator.h"
#include "ioregistry.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <algorithm>

namespace Ripes {

struct ColorDef {
  QString name;
  QColor on, off, glow;
};

static const ColorDef s_colors[] = {
    {"Red", {255, 30, 30}, {50, 10, 10}, {255, 80, 80}},
    {"Green", {30, 255, 30}, {10, 50, 10}, {80, 255, 80}},
    {"Blue", {50, 130, 255}, {15, 25, 55}, {100, 170, 255}},
    {"Yellow", {255, 220, 30}, {55, 48, 10}, {255, 235, 100}},
    {"White", {230, 230, 240}, {45, 45, 50}, {250, 250, 255}},
};

static constexpr int NUM_COLORS =
    static_cast<int>(sizeof(s_colors) / sizeof(s_colors[0]));
static constexpr unsigned MAX_DIGITS = IO7Indicator::NUM_DIGITS;
static std::vector<IOSymbol> s_extraSymbols;

// quick test buttons
static const uint8_t SEG_MAP[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
};
static constexpr uint8_t SEG_MINUS = 0x40;
static constexpr uint8_t SEG_DP = 0x80;
static const char *const kWidgetBg = "background: #F0F0F0;";

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

void IO7Indicator::rebuildRegDescs() {
  const unsigned n = NUM_DIGITS;
  m_regDescs.clear();
  m_regDescs.reserve(n);
  for (unsigned i = 0; i < n; i++) {
    m_regDescs.push_back(RegDesc{QString("Digit %1").arg(i), RegDesc::RW::RW, 8,
                                 static_cast<AInt>(i * 4), true});
  }
}

void IO7Indicator::initExtraSymbols() {
  if (s_extraSymbols.empty()) {
    s_extraSymbols.reserve(3 + MAX_DIGITS);

    s_extraSymbols.push_back(IOSymbol{"SEVENSEG_BASE", 0});
    s_extraSymbols.push_back(
        IOSymbol{"SEVENSEG_SIZE", static_cast<AInt>(MAX_DIGITS * 4)});
    s_extraSymbols.push_back(
        IOSymbol{"SEVENSEG_N_DIGITS", static_cast<AInt>(NUM_DIGITS)});

    for (unsigned i = 0; i < NUM_DIGITS; ++i) {
      s_extraSymbols.push_back(IOSymbol{QStringLiteral("SEVENSEG_%1").arg(i),
                                        static_cast<AInt>(i * 4)});
    }
  }
}

const std::vector<IOSymbol> *IO7Indicator::extraSymbols() const {
  return &s_extraSymbols;
}

IO7Indicator::IO7Indicator(QWidget *parent)
    : IOBase(IOType::SEVEN_SEGMENT, parent) {
  m_parameters[DIGIT_SIZE] =
      IOParam(DIGIT_SIZE, "Digit size", 64, true, 30, 120);
  m_parameters[COLOR] = IOParam(COLOR, "Color", 0, true, 0, NUM_COLORS - 1);

  m_digitValues.assign(NUM_DIGITS, 0);
  rebuildRegDescs();
  initExtraSymbols();
  buildUI();
  connect(this, &IOBase::scheduleUpdate, this, [this]() { refreshDisplay(); });
}

QString IO7Indicator::description() const {
  return QStringLiteral("7-segment indicator.\n"
                        "Each digit is a 4-byte word (offset = index * 4).\n"
                        "Bits 0-6 = segments a-g, bit 7 = decimal point.");
}

unsigned IO7Indicator::byteSize() const { return NUM_DIGITS * 4; }

void IO7Indicator::parameterChanged(unsigned /*ID*/) {
  if (m_updating)
    return;
  m_updating = true;

  if (m_displayWidget) {
    m_displayWidget->setColorIndex(
        m_parameters.count(COLOR) ? m_parameters.at(COLOR).value.toInt() : 0);
    m_displayWidget->setDigitValues(m_digitValues);
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

  updateGeometry();
  update();

  m_updating = false;
}

VInt IO7Indicator::ioRead(AInt offset, unsigned size) {
  if (size != 4 || (offset % 4) != 0)
    return static_cast<VInt>(0);

  const unsigned idx = static_cast<unsigned>(offset / 4);

  if (idx >= m_digitValues.size())
    return static_cast<VInt>(0);

  return static_cast<VInt>(m_digitValues[idx]);
}

void IO7Indicator::ioWrite(AInt offset, VInt value, unsigned size) {
  if (size != 4 || (offset % 4) != 0)
    return;

  const unsigned idx = static_cast<unsigned>(offset / 4);

  if (idx >= m_digitValues.size())
    return;

  const uint8_t regVal = static_cast<uint8_t>(value & static_cast<VInt>(0xFF));

  m_digitValues[idx] = regVal;

  emit scheduleUpdate();
}

void IO7Indicator::reset() {
  std::fill(m_digitValues.begin(), m_digitValues.end(), 0);
  emit scheduleUpdate();
}

QSize IO7Indicator::minimumSizeHint() const {
  auto it = m_parameters.find(DIGIT_SIZE);
  const int h = (it != m_parameters.end()) ? it->second.value.toInt() : 64;
  const int w = h * 2 / 3;
  const int gap = 6;
  const int n = static_cast<int>(NUM_DIGITS);
  const int totalW = n * w + (n - 1) * gap;
  const int pad = 16;
  return QSize(totalW + pad, h + pad);
}

void IO7Indicator::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), QColor(240, 240, 240));

  const int n = static_cast<int>(m_digitValues.size());
  if (n == 0)
    return;

  auto it = m_parameters.find(DIGIT_SIZE);
  const int h = (it != m_parameters.end()) ? it->second.value.toInt() : 64;
  const int w = h * 2 / 3;
  const int gap = 6;
  const int totalW = n * w + (n - 1) * gap;
  const int x0 = (width() - totalW) / 2;
  const int y0 = (height() - h) / 2;

  for (int i = 0; i < n; i++)
    drawDigit(p, x0 + i * (w + gap), y0, w, h, m_digitValues[i]);
}

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
  m_displayWidget->setNumDigits(static_cast<int>(NUM_DIGITS));
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
    std::vector<uint8_t> v(NUM_DIGITS, 0);
    const uint8_t d[] = {SEG_MAP[1], SEG_MAP[2], SEG_MAP[3], SEG_MAP[4]};
    for (unsigned i = 0; i < NUM_DIGITS; i++)
      v[i] = d[i];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("-5"), [this]() {
    std::vector<uint8_t> v(NUM_DIGITS, 0);
    v[0] = SEG_MINUS;
    v[1] = SEG_MAP[5];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("12.34"), [this]() {
    std::vector<uint8_t> v(NUM_DIGITS, 0);
    v[0] = SEG_MAP[1];
    v[1] = SEG_MAP[2] | SEG_DP;
    v[2] = SEG_MAP[3];
    v[3] = SEG_MAP[4];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("dEAd"), [this]() {
    std::vector<uint8_t> v(NUM_DIGITS, 0);
    const uint8_t d[] = {SEG_MAP[0xD], SEG_MAP[0xE], SEG_MAP[0xA],
                         SEG_MAP[0xD]};
    for (unsigned i = 0; i < NUM_DIGITS; i++)
      v[i] = d[i];
    applyQuickTest(v);
  });

  addBtn(QStringLiteral("Clear"),
         [this]() { applyQuickTest(std::vector<uint8_t>(NUM_DIGITS, 0)); });

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
  for (unsigned i = 0; i < NUM_DIGITS; i++) {
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
  for (unsigned i = 0; i < NUM_DIGITS; i++)
    m_digitValues[i] = (i < values.size()) ? values[i] : 0;
  emit scheduleUpdate();
}

} // namespace Ripes
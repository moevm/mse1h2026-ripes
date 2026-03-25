#include "iomouse.h"
#include "ioregistry.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QWheelEvent>

namespace Ripes {

//  Constructor
IOMouse::IOMouse(QWidget *parent) : IOBase(IOType::MOUSE, parent) {
    // ---- Register descriptors (unchanged) ----
    m_regDescs.push_back(RegDesc{"X", RegDesc::RW::R, 32, X * 4, true});
    m_regDescs.push_back(RegDesc{"Y", RegDesc::RW::R, 32, Y * 4, true});
    m_regDescs.push_back(
        RegDesc{"LBUTTON", RegDesc::RW::R, 1, LBUTTON * 4, true});
    m_regDescs.push_back(
        RegDesc{"RBUTTON", RegDesc::RW::R, 1, RBUTTON * 4, true});
    m_regDescs.push_back(
        RegDesc{"SCROLL", RegDesc::RW::RW, 32, SCROLL * 4, true});

    m_extraSymbols.push_back(IOSymbol{"WIDTH", m_width});
    m_extraSymbols.push_back(IOSymbol{"HEIGHT", m_height});

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // ---- Width spin box ----
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(310, 800);
    m_widthSpin->setValue(static_cast<int>(m_width));
    m_widthSpin->setFixedSize(80, 22);
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
    m_heightSpin->setFixedSize(80, 22);
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
}

//  Helper: apply new size to widget and reposition children
void IOMouse::applySize() {
    int totalW = static_cast<int>(m_width);
    int totalH = kTopH + static_cast<int>(m_height) + kBotH;

    setFixedSize(totalW, totalH);
    repositionWidgets();
    update();

    for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
        if (w->isWindow()) break;
        if (w->layout()) {
            w->layout()->update();
        }
        w->adjustSize();
    }
}

//  Helper: position child widgets
void IOMouse::repositionWidgets() {
    m_widthSpin->move(48, 4);
    m_heightSpin->move(176, 4);
    m_resetBtn->move(static_cast<int>(m_width) - 58, 4);
}

//  Size hints
QSize IOMouse::minimumSizeHint() const {
    return QSize(m_width, kTopH + m_height + kBotH);
}

QSize IOMouse::sizeHint() const {
    return QSize(m_width, kTopH + m_height + kBotH);
}

//  Logic
void IOMouse::reset() {
    m_mouseX = 0;
    m_mouseY = 0;
    m_lButton = 0;
    m_rButton = 0;
    m_mButton = 0;
    m_scroll = 0;
}

QString IOMouse::description() const {
    return "Mouse: X, Y, LBUTTON, RBUTTON, SCROLL";
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

//  Input events
void IOMouse::mouseMoveEvent(QMouseEvent *event) {
    m_mouseX = event->pos().x();
    m_mouseY = event->pos().y() - kTopH;
    if (m_mouseX < 0)
        m_mouseX = 0;
    if (m_mouseY < 0)
        m_mouseY = 0;
    if (m_mouseX >= (int)m_width)
        m_mouseX = m_width - 1;
    if (m_mouseY >= (int)m_height)
        m_mouseY = m_height - 1;
    emit scheduleUpdate();
}

void IOMouse::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        m_lButton = 1;
    if (event->button() == Qt::MiddleButton)
        m_mButton = 1;
    if (event->button() == Qt::RightButton)
        m_rButton = 1;
    emit scheduleUpdate();
}

void IOMouse::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton)
        m_lButton = 0;
    if (event->button() == Qt::MiddleButton)
        m_mButton = 0;
    if (event->button() == Qt::RightButton)
        m_rButton = 0;
    emit scheduleUpdate();
}

void IOMouse::wheelEvent(QWheelEvent *event) {
    m_scroll += event->angleDelta().y() / 120;
    emit scheduleUpdate();
}

//  Paint
void IOMouse::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int padW = static_cast<int>(m_width);
    const int padH = static_cast<int>(m_height);
    const int totalW = padW;
    const int totalH = kTopH + padH + kBotH;

    // Fonts
    QFont sansFont;
    sansFont.setPixelSize(11);

    QFont monoFont;
    monoFont.setFamily("Consolas");
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPixelSize(11);

    QFont monoSmall = monoFont;
    monoSmall.setPixelSize(10);

    // Top panel
    p.fillRect(0, 0, totalW, kTopH, QColor(237, 238, 243));
    p.setPen(QPen(QColor(205, 207, 213), 1));
    p.drawLine(0, kTopH - 1, totalW, kTopH - 1);

    p.setFont(sansFont);
    p.setPen(QColor(80, 82, 90));
    p.drawText(8, 18, "Width:");
    p.drawText(130, 18, "Height:");

    // Pad area
    p.fillRect(0, kTopH, padW, padH, QColor(250, 250, 252));

    // Grid dots
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(215, 218, 224));
    const int gridStep = 20;
    for (int gy = gridStep; gy < padH; gy += gridStep)
        for (int gx = gridStep; gx < padW; gx += gridStep)
            p.drawEllipse(QPoint(gx, kTopH + gy), 1, 1);

    // Crosshair position in widget coords
    int cx = m_mouseX;
    int cy = kTopH + m_mouseY;

    // Guide lines (faint, full-width)
    p.setPen(QPen(QColor(0, 175, 175, 45), 1, Qt::DashLine));
    p.drawLine(0, cy, padW, cy);
    p.drawLine(cx, kTopH, cx, kTopH + padH);

    // Bright arms near cursor
    const int arm = 14, gap = 5;
    p.setPen(QPen(QColor(0, 180, 180), 2));
    p.drawLine(cx - arm, cy, cx - gap, cy);
    p.drawLine(cx + gap, cy, cx + arm, cy);
    p.drawLine(cx, cy - arm, cx, cy - gap);
    p.drawLine(cx, cy + gap, cx, cy + arm);

    // Center dot
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(QColor(0, 180, 180));
    p.drawEllipse(QPoint(cx, cy), 4, 4);

    // Coordinate label near cursor
    p.setFont(monoFont);
    QFontMetrics cfm(monoFont);
    QString coordStr =
        QString("(%1, %2)").arg(m_mouseX).arg(m_mouseY);
    int tw = cfm.horizontalAdvance(coordStr);
    int th = cfm.height();

    int lx = cx + 14;
    int ly = cy - 8;
    if (lx + tw + 6 > padW)
        lx = cx - tw - 20;
    if (lx < 2)
        lx = 2;
    if (ly - th < kTopH)
        ly = cy + th + 8;

    // Label background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 215));
    p.drawRoundedRect(lx - 4, ly - th + 2, tw + 8, th + 2, 3, 3);

    p.setPen(QColor(0, 155, 155));
    p.drawText(lx, ly, coordStr);

    // Pad border
    p.setPen(QPen(QColor(190, 193, 200), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(0, kTopH, padW - 1, padH - 1);

    // Bottom panel
    int botY = kTopH + padH;
    p.fillRect(0, botY, totalW, kBotH, QColor(237, 238, 243));
    p.setPen(QPen(QColor(205, 207, 213), 1));
    p.drawLine(0, botY, totalW, botY);

    // Row 1 — coordinates, button indicators, scroll
    p.setFont(monoFont);
    int r1 = botY + 18;

    p.setPen(QColor(55, 57, 65));
    p.drawText(8, r1,
               QString("X:%1 Y:%2").arg(m_mouseX, 4).arg(m_mouseY, 4));

    // L / M / R indicators
    int btnAreaX = 120;
    int btnY0 = botY + 4;
    int btnSz = 22;
    int btnGap = 3;

    auto drawBtn = [&](int bx, const QString &text, bool active) {
        QColor bg = active ? QColor(72, 192, 118) : QColor(218, 220, 226);
        QColor fg = active ? Qt::white : QColor(100, 103, 112);
        QColor bd = active ? QColor(56, 162, 98) : QColor(195, 197, 204);

        p.setPen(QPen(bd, 1));
        p.setBrush(bg);
        p.drawRoundedRect(bx, btnY0, btnSz, btnSz, 3, 3);

        QFont bf = monoFont;
        bf.setPixelSize(10);
        bf.setBold(active);
        p.setFont(bf);
        p.setPen(fg);
        p.drawText(QRect(bx, btnY0, btnSz, btnSz), Qt::AlignCenter, text);
        p.setFont(monoFont);
    };

    drawBtn(btnAreaX, "L", m_lButton);
    drawBtn(btnAreaX + btnSz + btnGap, "M", m_mButton);
    drawBtn(btnAreaX + 2 * (btnSz + btnGap), "R", m_rButton);

    // Scroll value
    int scrX = btnAreaX + 3 * (btnSz + btnGap) + 10;
    p.setPen(QColor(55, 57, 65));
    p.drawText(scrX, r1, QString("SCR:%1").arg(m_scroll));

    // Row 2 — hex values ------------------------------------
    int r2 = botY + 42;
    p.setFont(monoSmall);
    p.setPen(QColor(130, 133, 145));

    auto hex = [](uint32_t v, int w) -> QString {
        return QString("%1").arg(v, w, 16, QChar('0')).toUpper();
    };

    uint32_t btnVal = (m_lButton ? 1u : 0u) | (m_mButton ? 2u : 0u) |
                      (m_rButton ? 4u : 0u);

    p.drawText(8, r2,
               "X:0x" + hex(m_mouseX, 4) + " Y:0x" + hex(m_mouseY, 4) +
               " BTN:0x" + hex(btnVal, 2) + " SCR:0x" +
               hex(static_cast<uint32_t>(m_scroll), 8));

    // Outer border
    p.setPen(QPen(QColor(190, 193, 200), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(0, 0, totalW - 1, totalH - 1);

    p.end();
}
} // namespace Ripes
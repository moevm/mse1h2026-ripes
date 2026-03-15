#pragma once

#include <QWidget>
#include <vector>

#include "iobase.h"

class QLabel;
class QSpinBox;
class QComboBox;
class QHBoxLayout;

namespace Ripes {

class SegmentDisplayWidget;

class IO7Indicator : public IOBase {
  Q_OBJECT

  enum Parameters { DIGITS, DIGIT_SIZE, COLOR };

public:
  explicit IO7Indicator(QWidget *parent);
  ~IO7Indicator() override { unregister(); }

  unsigned byteSize() const override;
  QString description() const override;
  QString baseName() const override { return "Seven Segment"; }

  const std::vector<RegDesc> &registers() const override { return m_regDescs; }

  VInt ioRead(AInt offset, unsigned size) override;
  void ioWrite(AInt offset, VInt value, unsigned size) override;
  void reset() override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void parameterChanged(unsigned ID) override;
  QSize minimumSizeHint() const override;

private:
  unsigned numDigits() const;
  void rebuildRegDescs();
  void drawDigit(QPainter &p, int x, int y, int w, int h, uint8_t segments);

  void buildUI();
  void refreshDisplay();

  std::vector<uint8_t> m_digitValues;
  std::vector<RegDesc> m_regDescs;
  bool m_updating = false;

  SegmentDisplayWidget *m_displayWidget = nullptr;
  QSpinBox *m_spinDigits = nullptr;
  QComboBox *m_comboColor = nullptr;
  QHBoxLayout *m_hexBarLayout = nullptr;
  std::vector<QLabel *> m_regHexLabels;
};

} // namespace Ripes
#pragma once

#include <QWidget>
#include <vector>

#include "iobase.h"

<<<<<<< Updated upstream
class QLabel;
class QSpinBox;
class QComboBox;
class QHBoxLayout;

=======
>>>>>>> Stashed changes
namespace Ripes {

class IO7Indicator : public IOBase {
  Q_OBJECT

  enum Parameters { DIGITS, DIGIT_SIZE, COLOR };

public:
<<<<<<< Updated upstream
  explicit IO7Indicator(QWidget *parent);
  ~IO7Indicator() override { unregister(); }
=======
  static constexpr unsigned NUM_DIGITS = 4;

  IO7Indicator(QWidget *parent);
  ~IO7Indicator() { unregister(); };
>>>>>>> Stashed changes

  virtual unsigned byteSize() const override;
  virtual QString description() const override;
  virtual QString baseName() const override { return "Seven Segment"; }

  virtual const std::vector<RegDesc> &registers() const override {
    return m_regDescs;
  };
  virtual const std::vector<IOSymbol> *extraSymbols() const override {
    return &m_extraSymbols;
  }

  virtual VInt ioRead(AInt offset, unsigned size) override;
  virtual void ioWrite(AInt offset, VInt value, unsigned size) override;
  virtual void reset() override;

protected:
  virtual void parameterChanged(unsigned) override;

  void paintEvent(QPaintEvent *event) override;
  QSize minimumSizeHint() const override;

private:
<<<<<<< Updated upstream
  unsigned numDigits() const;
  void rebuildRegDescs();
=======
  void updateRegDescs();
>>>>>>> Stashed changes
  void drawDigit(QPainter &p, int x, int y, int w, int h, uint8_t segments);

  std::vector<uint8_t> m_digitValues;
  std::vector<RegDesc> m_regDescs;
<<<<<<< Updated upstream
  bool m_updating = false;

  SegmentDisplayWidget *m_displayWidget = nullptr;
  QSpinBox *m_spinDigits = nullptr;
  QComboBox *m_comboColor = nullptr;
  QHBoxLayout *m_hexBarLayout = nullptr;
  std::vector<QLabel *> m_regHexLabels;
=======
  std::vector<IOSymbol> m_extraSymbols;
>>>>>>> Stashed changes
};

} // namespace Ripes
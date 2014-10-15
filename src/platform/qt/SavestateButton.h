#ifndef QGBA_SAVESTATE_BUTTON
#define QGBA_SAVESTATE_BUTTON

#include <QAbstractButton>

namespace QGBA {

class SavestateButton : public QAbstractButton {
public:
	SavestateButton(QWidget* parent = nullptr);

protected:
	virtual void paintEvent(QPaintEvent *e) override;
};

}

#endif

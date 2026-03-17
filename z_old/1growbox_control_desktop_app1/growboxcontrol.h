#ifndef GROWBOXCONTROL_H
#define GROWBOXCONTROL_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class GrowboxControl;
}
QT_END_NAMESPACE

class GrowboxControl : public QMainWindow
{
    Q_OBJECT

public:
    GrowboxControl(QWidget *parent = nullptr);
    ~GrowboxControl();

private:
    Ui::GrowboxControl *ui;
};
#endif // GROWBOXCONTROL_H

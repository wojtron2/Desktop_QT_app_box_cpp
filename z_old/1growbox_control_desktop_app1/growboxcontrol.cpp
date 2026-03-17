#include "growboxcontrol.h"
#include "./ui_growboxcontrol.h"

GrowboxControl::GrowboxControl(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::GrowboxControl)
{
    ui->setupUi(this);
}

GrowboxControl::~GrowboxControl()
{
    delete ui;
}

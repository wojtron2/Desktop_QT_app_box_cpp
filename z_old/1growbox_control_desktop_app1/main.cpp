#include "growboxcontrol.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GrowboxControl w;
    w.show();
    return a.exec();
}

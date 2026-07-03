#include "mainwindow.h"

#include <QIcon>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setStyle("fusion");   // nowoczesny styl Qt
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/new/img/plant_icon.png"));

    MainWindow w;

    w.show();

    return app.exec();
}

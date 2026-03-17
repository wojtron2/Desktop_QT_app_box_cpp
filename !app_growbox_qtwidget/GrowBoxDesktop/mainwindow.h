#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

inline constexpr auto VERSION_SW = "0.3.3";


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void logMessage(const QString &msg);  // deklaracja funkcji logowania


private slots:
    //NETWORK
    void onHttpFinished(QNetworkReply *reply);

    /*
    void on_LED1ON_clicked();
    void on_LED1OFF_clicked();
*/


    // old
    void on_pushButton_CONNECT_clicked();

    void on_pushButton_CLEAR_LOG_clicked(); // deklaracja funkcji przycisku czyszczenia logow

private:
    Ui::MainWindow *ui;

    QNetworkAccessManager *m_netManager;


    void sendGet(const QString &path);   // wspolna funkcja HTTP GET
    void setupLedButtons();              // podpina wszystkie LED-y


    // zapisywanie i odczyt ustawien
    void loadSettings();   // save settings
    void saveSettings();   // load settings

};
#endif // MAINWINDOW_H

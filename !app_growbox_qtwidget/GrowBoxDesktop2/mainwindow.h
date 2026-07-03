#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QVector>

inline constexpr auto VERSION_SW = "0.4.0";

class QCheckBox;
class QComboBox;
class QFrame;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimeEdit;
class QTimer;
class QWidget;
class QJsonObject;

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
    QTimer *m_ageTimer;

    struct OutputUi
    {
        QFrame *card = nullptr;
        QLabel *stateLabel = nullptr;
        QLabel *manualLabel = nullptr;
        QLabel *scheduleLabel = nullptr;
        QTimeEdit *autoOnTime = nullptr;
        QTimeEdit *autoOffTime = nullptr;
    };

    struct SoundAlarmUi
    {
        QCheckBox *enabled = nullptr;
        QTimeEdit *time = nullptr;
        QLineEdit *file = nullptr;
        QSpinBox *volume = nullptr;
        QLabel *statusLabel = nullptr;
    };

    struct BacklightRuleUi
    {
        QCheckBox *enabled = nullptr;
        QTimeEdit *start = nullptr;
        QTimeEdit *end = nullptr;
        QLabel *statusLabel = nullptr;
    };

    QDateTime m_lastStatusUpdate;
    QDateTime m_lastSoundUpdate;
    QDateTime m_lastDisplayUpdate;

    QLabel *m_mainUpdatedLabel = nullptr;
    QLabel *m_statusUpdatedLabel = nullptr;
    QLabel *m_deviceTimeLabel = nullptr;
    QLabel *m_globalAutoLabel = nullptr;
    QLabel *m_soundAutoLabel = nullptr;
    QLabel *m_soundVolumeLabel = nullptr;
    QLabel *m_soundPlaybackModeLabel = nullptr;
    QLabel *m_displayModeLabel = nullptr;
    QLabel *m_backlightLabel = nullptr;
    QLabel *m_lcdLabel = nullptr;
    QLabel *m_outputSummaryLabel = nullptr;
    QLabel *m_sensorSummaryLabel = nullptr;
    QLabel *m_soundStatusLabel = nullptr;
    QLabel *m_displayStatusLabel = nullptr;
    QPlainTextEdit *m_statusJsonPreview = nullptr;

    QSlider *m_soundVolumeSlider = nullptr;
    QSpinBox *m_soundVolumeSpin = nullptr;
    QCheckBox *m_soundMultipleCheckBox = nullptr;
    QComboBox *m_soundFileCombo = nullptr;
    QLineEdit *m_soundFileEdit = nullptr;
    QSpinBox *m_soundPlayVolumeSpin = nullptr;
    QLineEdit *m_displayClockTextEdit = nullptr;

    QMap<int, OutputUi> m_outputUi;
    QMap<int, SoundAlarmUi> m_soundAlarmUi;
    QMap<int, BacklightRuleUi> m_backlightRuleUi;
    QStringList m_availableSoundFiles;
    QHBoxLayout *m_quickPlayGroupsLayout = nullptr;
    QVector<QLabel*> m_mainOutputLabels;
    QVector<QLabel*> m_sensorLabels;
    QVector<QLabel*> m_oldSensorUpdateLabels;

    void sendGet(const QString &path);   // wspolna funkcja HTTP GET
    void setupLedButtons();              // podpina wszystkie LED-y
    void sendApiGet(const QString &path,
                    const QString &kind = QString(),
                    const QString &refreshAfter = QString());
    void requestStatus();
    void requestSoundStatus();
    void requestDisplayStatus();
    void requestAllStatus();

    void setupCentralLayout();
    void setupExistingStatusTab();
    void setupMainTab();
    void setupSensorTab();
    void setupOutputsTab();
    void setupScheduleTab();
    void setupSoundTab();
    void setupDisplayTab();

    QFrame *createCard(const QString &title, QWidget *parent = nullptr) const;
    QPushButton *createApiButton(const QString &text,
                                 const QString &path,
                                 const QString &refreshAfter = QString(),
                                 QWidget *parent = nullptr);
    QTimeEdit *createTimeEdit(QWidget *parent = nullptr) const;
    QString timeQuery(const QTimeEdit *edit) const;
    QString encoded(const QString &value) const;
    QStringList fallbackSoundFiles() const;
    void setAvailableSoundFiles(const QStringList &files);
    void rebuildQuickSoundButtons();

    void parseStatus(const QJsonObject &root, const QByteArray &body);
    void parseSoundStatus(const QJsonObject &root);
    void parseDisplayStatus(const QJsonObject &root);
    void updateOutputState(int id,
                           bool state,
                           int autoOnHour,
                           int autoOnMinute,
                           int autoOffHour,
                           int autoOffMinute,
                           bool manualMode);
    void updateSensorWidgets(const QJsonObject &root);
    void updateAgeLabels();
    void setStateChip(QLabel *label, bool on, const QString &onText = "ON", const QString &offText = "OFF") const;
    void setGroupBoxState(QGroupBox *groupBox, bool on) const;


    // zapisywanie i odczyt ustawien
    void loadSettings();   // save settings
    void saveSettings();   // load settings

};
#endif // MAINWINDOW_H

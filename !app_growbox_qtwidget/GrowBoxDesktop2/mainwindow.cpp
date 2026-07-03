#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int OutputCount = 16;
constexpr int SoundAlarmCount = 10;
constexpr int BacklightRuleCount = 3;

QString cardStyle()
{
    return QStringLiteral(
        "QFrame[card=\"true\"] {"
        " background-color: rgba(0, 70, 0, 150);"
        " border-radius: 8px;"
        " border: 4px solid #44ff07;"
        " color: white;"
        "}"
        "QFrame[card=\"true\"] QLabel { color: white; }"
        "QFrame[card=\"true\"] QCheckBox { color: white; }"
        "QFrame[card=\"true\"] QPushButton { min-height: 24px; }"
        "QFrame[card=\"true\"] QLineEdit,"
        "QFrame[card=\"true\"] QSpinBox,"
        "QFrame[card=\"true\"] QTimeEdit {"
        " background: rgba(255, 255, 255, 230);"
        " color: #101510;"
        " border: 1px solid #44ff07;"
        " border-radius: 4px;"
        " padding: 2px 4px;"
        "}");
}

QString fernPageStyle(const QString &objectName)
{
    return QStringLiteral(
               "#%1 {"
               " background-image:url(:/new/img/fern.jpg);"
               " background-repeat: no-repeat;"
               " background-position: center;"
               " background-color: #333333;"
               "}")
        .arg(objectName);
}

QString chipStyle(bool on)
{
    return on
        ? QStringLiteral("QLabel { background-color: rgba(0, 120, 24, 185); color: white; border-radius: 6px; padding: 4px 8px; font-weight: 700; }")
        : QStringLiteral("QLabel { background-color: rgba(150, 25, 25, 185); color: white; border-radius: 6px; padding: 4px 8px; font-weight: 700; }");
}

QString mutedChipStyle()
{
    return QStringLiteral("QLabel { background-color: rgba(0, 0, 0, 55); color: white; border-radius: 6px; padding: 4px 8px; }");
}

QString groupBoxStateStyle(bool on, const QString &name)
{
    const QString background = on ? QStringLiteral("rgba(0, 70, 0, 150)") : QStringLiteral("rgba(80, 0, 0, 150)");
    const QString border = on ? QStringLiteral("#44ff07") : QStringLiteral("#ff4242");

    return QStringLiteral(
               "#%1 {"
               " background-color: %2;"
               " border-radius: 8px;"
               " border: 4px solid %3;"
               " color: white;"
               " padding: 8px;"
               "}")
        .arg(name, background, border);
}

QString timeText(int hour, int minute)
{
    if (hour < 0) {
        return QStringLiteral("--:--");
    }

    return QStringLiteral("%1:%2")
        .arg(hour, 2, 10, QLatin1Char('0'))
        .arg(minute, 2, 10, QLatin1Char('0'));
}

QString ageText(const QDateTime &stamp)
{
    if (!stamp.isValid()) {
        return QStringLiteral("Updated - ago (s)");
    }

    const qint64 seconds = stamp.secsTo(QDateTime::currentDateTime());
    return QStringLiteral("Updated %1 ago (s)").arg(qMax<qint64>(0, seconds));
}

QVBoxLayout *cardLayout(QFrame *card)
{
    return qobject_cast<QVBoxLayout*>(card->layout());
}

QLabel *makeTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 700; color: white;"));
    label->setWordWrap(true);
    return label;
}

QLabel *makeValueLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(mutedChipStyle());
    return label;
}

QPushButton *makeButton(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return button;
}

QWidget *makeScrollPage(QWidget *parent, const QString &objectName, bool fernBackground)
{
    auto *page = new QWidget(parent);
    page->setObjectName(objectName);
    if (fernBackground) {
        page->setStyleSheet(fernPageStyle(objectName));
    }

    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(0);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; } QScrollArea > QWidget > QWidget { background: transparent; }"));

    auto *content = new QWidget(scroll);
    content->setObjectName(objectName + QStringLiteral("_content"));
    content->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);
    scroll->setWidget(content);

    root->addWidget(scroll);
    return page;
}

void clearPage(QWidget *page)
{
    if (!page) {
        return;
    }

    const auto children = page->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children) {
        delete child;
    }

    delete page->layout();
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_netManager(new QNetworkAccessManager(this))
    , m_ageTimer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("GrowBox Desktop"));
    setMinimumSize(900, 650);

    ui->label_INFO->setText(ui->label_INFO->text().arg(VERSION_SW));
    ui->plainTextEditLog->document()->setMaximumBlockCount(1000);

    setupCentralLayout();
    loadSettings();

    setupExistingStatusTab();
    setupMainTab();
    setupSensorTab();
    setupOutputsTab();
    setupScheduleTab();
    setupSoundTab();
    setupDisplayTab();
    setupLedButtons();

    connect(m_netManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onHttpFinished);
    connect(m_ageTimer, &QTimer::timeout, this, &MainWindow::updateAgeLabels);
    m_ageTimer->start(1000);

    logMessage(QStringLiteral("INFO: Aplikacja uruchomiona."));
    logMessage(QStringLiteral("Wersja: %1").arg(VERSION_SW));

    if (ui->checkBox_AUTOCONNECT->isChecked()) {
        QTimer::singleShot(300, this, &MainWindow::requestAllStatus);
    }
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::setupCentralLayout()
{
    auto *layout = new QVBoxLayout(ui->centralwidget);
    layout->setContentsMargins(10, 0, 10, 4);
    layout->setSpacing(2);

    ui->tabWidget->setParent(ui->centralwidget);
    ui->tabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->tabWidget->setMinimumSize(760, 540);
    layout->addWidget(ui->tabWidget, 1);

    ui->label_5->setParent(ui->centralwidget);
    layout->addWidget(ui->label_5, 0, Qt::AlignRight);
}

QFrame *MainWindow::createCard(const QString &title, QWidget *parent) const
{
    auto *card = new QFrame(parent);
    card->setProperty("card", true);
    card->setStyleSheet(cardStyle());
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    layout->addWidget(makeTitle(title, card));

    return card;
}

QPushButton *MainWindow::createApiButton(const QString &text,
                                         const QString &path,
                                         const QString &refreshAfter,
                                         QWidget *parent)
{
    auto *button = makeButton(text, parent);
    connect(button, &QPushButton::clicked, this, [this, path, refreshAfter]() {
        sendApiGet(path, QStringLiteral("command"), refreshAfter);
    });
    return button;
}

QTimeEdit *MainWindow::createTimeEdit(QWidget *parent) const
{
    auto *edit = new QTimeEdit(parent);
    edit->setDisplayFormat(QStringLiteral("HH:mm"));
    edit->setTime(QTime(8, 0));
    edit->setMinimumWidth(76);
    return edit;
}

QString MainWindow::timeQuery(const QTimeEdit *edit) const
{
    const QTime time = edit ? edit->time() : QTime(0, 0);
    return QStringLiteral("hour=%1&minute=%2").arg(time.hour()).arg(time.minute());
}

QString MainWindow::encoded(const QString &value) const
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value, "/"));
}

QStringList MainWindow::fallbackSoundFiles() const
{
    return {
        QStringLiteral("sc1_sounds/terran_base_under_attack.wav"),
        QStringLiteral("sc1_sounds/dropship_load.wav"),
        QStringLiteral("sc1_sounds/dropship_unload.wav"),
        QStringLiteral("sc1_sounds/Transmission.wav"),
        QStringLiteral("sc1_sounds/TRescue.wav"),
        QStringLiteral("alarm.mp3"),
        QStringLiteral("click.mp3")
    };
}

void MainWindow::setAvailableSoundFiles(const QStringList &files)
{
    QStringList cleanFiles;
    for (const QString &file : files) {
        const QString trimmed = file.trimmed();
        if (!trimmed.isEmpty() && !cleanFiles.contains(trimmed)) {
            cleanFiles << trimmed;
        }
    }

    if (cleanFiles.isEmpty()) {
        cleanFiles = fallbackSoundFiles();
    }

    if (cleanFiles == m_availableSoundFiles) {
        return;
    }

    const QString current = m_soundFileEdit ? m_soundFileEdit->text().trimmed() : QString();
    m_availableSoundFiles = cleanFiles;

    if (m_soundFileCombo) {
        QSignalBlocker block(m_soundFileCombo);
        m_soundFileCombo->clear();
        m_soundFileCombo->addItems(m_availableSoundFiles);
        const int idx = m_availableSoundFiles.indexOf(current);
        m_soundFileCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    if (m_soundFileEdit && current.isEmpty() && !m_availableSoundFiles.isEmpty()) {
        m_soundFileEdit->setText(m_availableSoundFiles.first());
    }

    rebuildQuickSoundButtons();
}

void MainWindow::rebuildQuickSoundButtons()
{
    if (!m_quickPlayGroupsLayout) {
        return;
    }

    while (QLayoutItem *item = m_quickPlayGroupsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    QMap<QString, QStringList> groups;
    for (const QString &file : m_availableSoundFiles.isEmpty() ? fallbackSoundFiles() : m_availableSoundFiles) {
        const QString folder = file.contains(QLatin1Char('/'))
                                   ? file.section(QLatin1Char('/'), 0, 0)
                                   : QStringLiteral("TODO");
        const QString groupName = (folder == QStringLiteral("sc1_sounds"))
                                      ? QStringLiteral("SC1")
                                      : folder.toUpper();
        groups[groupName].append(file);
    }

    if (!groups.contains(QStringLiteral("TODO"))) {
        groups[QStringLiteral("TODO")] = {};
    }

    const QString groupStyle = QStringLiteral(
        "QGroupBox {"
        " color: white;"
        " border: 1px solid rgba(68, 255, 7, 190);"
        " border-radius: 6px;"
        " margin-top: 10px;"
        " padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        " subcontrol-origin: margin;"
        " left: 8px;"
        " padding: 0 4px;"
        "}");

    QStringList orderedGroups;
    if (groups.contains(QStringLiteral("SC1"))) {
        orderedGroups << QStringLiteral("SC1");
    }
    for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
        if (it.key() != QStringLiteral("SC1") && it.key() != QStringLiteral("TODO")) {
            orderedGroups << it.key();
        }
    }
    orderedGroups << QStringLiteral("TODO");

    for (const QString &groupName : orderedGroups) {
        auto *box = new QGroupBox(groupName);
        box->setStyleSheet(groupStyle);
        auto *row = new QHBoxLayout(box);
        row->setContentsMargins(8, 8, 8, 8);
        row->setSpacing(6);

        const QStringList files = groups.value(groupName);
        for (const QString &file : files) {
            QString buttonText = file.section(QLatin1Char('/'), -1);
            if (buttonText.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive) ||
                buttonText.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)) {
                buttonText.chop(4);
            }

            auto *button = makeButton(buttonText, box);
            button->setMinimumHeight(24);
            button->setMaximumHeight(26);
            const int buttonWidth = qBound(90, button->fontMetrics().horizontalAdvance(buttonText) + 22, 210);
            button->setFixedWidth(buttonWidth);
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            connect(button, &QPushButton::clicked, this, [this, file]() {
                const int volume = m_soundPlayVolumeSpin ? m_soundPlayVolumeSpin->value() : 100;
                sendApiGet(QStringLiteral("api/sound/play?file=%1&volume=%2")
                               .arg(encoded(file))
                               .arg(volume),
                           QStringLiteral("command"),
                           QString());
            });
            row->addWidget(button);
        }

        row->addStretch(1);
        if (files.isEmpty()) {
            box->setMinimumWidth(130);
        }
        m_quickPlayGroupsLayout->addWidget(box, 0, Qt::AlignTop);
    }

    m_quickPlayGroupsLayout->addStretch(1);
}

void MainWindow::setupExistingStatusTab()
{
    const int statusIndex = ui->tabWidget->indexOf(ui->tab1_status);
    if (statusIndex >= 0) {
        ui->tabWidget->setTabText(statusIndex, QStringLiteral("Status"));
    }

    const QVector<QPushButton*> sensorButtons = {
        ui->pushButton_SENSOR1_UPDATE,
        ui->pushButton_SENSOR2_UPDATE,
        ui->pushButton_SENSOR3_UPDATE,
        ui->pushButton_SENSOR4_UPDATE
    };

    m_oldSensorUpdateLabels.clear();
    for (QPushButton *button : sensorButtons) {
        if (!button || !button->parentWidget()) {
            continue;
        }

        auto *label = new QLabel(ageText(QDateTime()), button->parentWidget());
        label->setGeometry(24, 20, 160, 24);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(mutedChipStyle());
        label->show();
        button->hide();
        m_oldSensorUpdateLabels.push_back(label);
    }

    connect(ui->pushButton_UPDATE_STATUS, &QPushButton::clicked,
            this, &MainWindow::requestAllStatus);

    const QVector<QGroupBox*> translucentBoxes = {
        ui->groupBoxLED1,
        ui->groupBoxLED2,
        ui->groupBoxLED3,
        ui->groupBoxLED4,
        ui->groupBoxLED5,
        ui->groupBoxLED6,
        ui->groupBoxLED7,
        ui->groupBoxLED8,
        ui->groupBoxLED_ALL,
        ui->groupBoxSENSOR1,
        ui->groupBoxSENSOR2,
        ui->groupBoxSENSOR3,
        ui->groupBoxSENSOR4,
        ui->groupBoxInfo
    };

    for (QGroupBox *box : translucentBoxes) {
        setGroupBoxState(box, true);
    }

    ui->tab1_status->setStyleSheet(QStringLiteral(
        "#tab1_status {"
        " background-color: #333333;"
        "}"
        "#statusResponsiveContent {"
        " border-image: url(:/new/img/fern.jpg) 0 0 0 0 stretch stretch;"
        " background-color: #333333;"
        "}"));

    auto *rootLayout = new QVBoxLayout(ui->tab1_status);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(0);

    auto *scroll = new QScrollArea(ui->tab1_status);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("statusResponsiveContent"));
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(22, 22, 22, 22);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    auto prepareBox = [](QGroupBox *box) {
        if (!box) {
            return;
        }
        box->setMinimumHeight(58);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    };

    auto layoutLedBox = [prepareBox](QGroupBox *box, QPushButton *onButton, QPushButton *offButton) {
        prepareBox(box);
        if (!box || !onButton || !offButton) {
            return;
        }
        auto *layout = new QHBoxLayout(box);
        layout->setContentsMargins(22, 10, 10, 8);
        layout->setSpacing(10);
        onButton->setMinimumWidth(68);
        offButton->setMinimumWidth(68);
        onButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        offButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(onButton);
        layout->addWidget(offButton);
    };

    layoutLedBox(ui->groupBoxLED1, ui->pushButton_LED1ON, ui->pushButton_LED1OFF);
    layoutLedBox(ui->groupBoxLED2, ui->pushButton_LED2ON, ui->pushButton_LED2OFF);
    layoutLedBox(ui->groupBoxLED3, ui->pushButton_LED3ON, ui->pushButton_LED3OFF);
    layoutLedBox(ui->groupBoxLED4, ui->pushButton_LED4ON, ui->pushButton_LED4OFF);
    layoutLedBox(ui->groupBoxLED5, ui->pushButton_LED5ON, ui->pushButton_LED5OFF);
    layoutLedBox(ui->groupBoxLED6, ui->pushButton_LED6ON, ui->pushButton_LED6OFF);
    layoutLedBox(ui->groupBoxLED7, ui->pushButton_LED7ON, ui->pushButton_LED7OFF);
    layoutLedBox(ui->groupBoxLED8, ui->pushButton_LED8ON, ui->pushButton_LED8OFF);
    layoutLedBox(ui->groupBoxLED_ALL, ui->pushButton_LED_ALL_ON, ui->pushButton_LED_ALL_OFF);

    const QVector<QGroupBox*> ledBoxes = {
        ui->groupBoxLED1,
        ui->groupBoxLED2,
        ui->groupBoxLED3,
        ui->groupBoxLED4,
        ui->groupBoxLED5,
        ui->groupBoxLED6,
        ui->groupBoxLED7,
        ui->groupBoxLED8
    };

    for (int i = 0; i < ledBoxes.size(); ++i) {
        grid->addWidget(ledBoxes.at(i), i, 0);
    }

    grid->addWidget(ui->groupBoxLED_ALL, 0, 1);

    const QVector<QGroupBox*> sensorBoxes = {
        ui->groupBoxSENSOR1,
        ui->groupBoxSENSOR2,
        ui->groupBoxSENSOR3,
        ui->groupBoxSENSOR4
    };

    for (int i = 0; i < sensorBoxes.size(); ++i) {
        QGroupBox *box = sensorBoxes.at(i);
        prepareBox(box);
        if (box && i < m_oldSensorUpdateLabels.size()) {
            auto *layout = new QHBoxLayout(box);
            layout->setContentsMargins(22, 10, 10, 8);
            layout->addWidget(m_oldSensorUpdateLabels.at(i));
        }
        grid->addWidget(box, i + 2, 1);
    }

    auto *actionsCard = createCard(QStringLiteral("Actions"), content);
    auto *actionsLayout = cardLayout(actionsCard);
    ui->pushButton_UPDATE_STATUS->setMinimumHeight(32);
    ui->pushButton_SHUTDOWN_LINUX->setMinimumHeight(32);
    actionsLayout->addWidget(ui->pushButton_UPDATE_STATUS);
    actionsLayout->addWidget(ui->pushButton_SHUTDOWN_LINUX);
    actionsLayout->addStretch(1);
    grid->addWidget(actionsCard, 0, 2, 4, 1);

    auto *spacer = new QWidget(content);
    spacer->setMinimumHeight(220);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid->addWidget(spacer, 8, 0, 1, 3);

    scroll->setWidget(content);
    rootLayout->addWidget(scroll);
}

void MainWindow::setupMainTab()
{
    QWidget *page = makeScrollPage(nullptr, QStringLiteral("tabMain"), true);
    QWidget *content = page->findChild<QWidget*>(QStringLiteral("tabMain_content"));
    auto *contentLayout = qobject_cast<QVBoxLayout*>(content->layout());

    auto *topGrid = new QGridLayout();
    topGrid->setContentsMargins(0, 0, 0, 0);
    topGrid->setHorizontalSpacing(10);
    topGrid->setVerticalSpacing(10);
    contentLayout->addLayout(topGrid);

    auto *infoCard = createCard(QStringLiteral("GrowBox Desktop"), content);
    auto *infoLayout = cardLayout(infoCard);
    infoLayout->addWidget(makeValueLabel(QStringLiteral("LAN controller, version %1").arg(VERSION_SW), infoCard));
    m_mainUpdatedLabel = makeValueLabel(ageText(QDateTime()), infoCard);
    infoLayout->addWidget(m_mainUpdatedLabel);
    auto *refreshRow = new QHBoxLayout();
    refreshRow->addWidget(createApiButton(QStringLiteral("Refresh"), QStringLiteral("api/status"), QString(), infoCard));
    auto *refreshAllButton = makeButton(QStringLiteral("Refresh all"), infoCard);
    connect(refreshAllButton, &QPushButton::clicked, this, &MainWindow::requestAllStatus);
    refreshRow->addWidget(refreshAllButton);
    infoLayout->addLayout(refreshRow);
    topGrid->addWidget(infoCard, 0, 0);

    auto *statusCard = createCard(QStringLiteral("Status"), content);
    auto *statusLayout = cardLayout(statusCard);
    m_deviceTimeLabel = makeValueLabel(QStringLiteral("Device time: -"), statusCard);
    m_globalAutoLabel = makeValueLabel(QStringLiteral("System auto: -"), statusCard);
    m_soundAutoLabel = makeValueLabel(QStringLiteral("Sound auto: -"), statusCard);
    m_soundVolumeLabel = makeValueLabel(QStringLiteral("Sound volume: -"), statusCard);
    m_displayModeLabel = makeValueLabel(QStringLiteral("LCD mode: -"), statusCard);
    m_backlightLabel = makeValueLabel(QStringLiteral("Backlight: -"), statusCard);
    m_lcdLabel = makeValueLabel(QStringLiteral("LCD init: -"), statusCard);
    statusLayout->addWidget(m_deviceTimeLabel);
    statusLayout->addWidget(m_globalAutoLabel);
    statusLayout->addWidget(m_soundAutoLabel);
    statusLayout->addWidget(m_soundVolumeLabel);
    statusLayout->addWidget(m_displayModeLabel);
    statusLayout->addWidget(m_backlightLabel);
    statusLayout->addWidget(m_lcdLabel);
    topGrid->addWidget(statusCard, 0, 1);

    auto *controlCard = createCard(QStringLiteral("Quick control"), content);
    auto *controlLayout = cardLayout(controlCard);
    auto *row1 = new QGridLayout();
    row1->setHorizontalSpacing(6);
    row1->setVerticalSpacing(6);
    row1->addWidget(createApiButton(QStringLiteral("All ON"), QStringLiteral("api/outputs/on_all"), QStringLiteral("status"), controlCard), 0, 0);
    row1->addWidget(createApiButton(QStringLiteral("All OFF"), QStringLiteral("api/outputs/off_all"), QStringLiteral("status"), controlCard), 0, 1);
    row1->addWidget(createApiButton(QStringLiteral("Manual ALL ON"), QStringLiteral("api/outputs/manual/on_all"), QStringLiteral("status"), controlCard), 1, 0);
    row1->addWidget(createApiButton(QStringLiteral("Manual ALL OFF"), QStringLiteral("api/outputs/manual/off_all"), QStringLiteral("status"), controlCard), 1, 1);
    row1->addWidget(createApiButton(QStringLiteral("System AUTO ON"), QStringLiteral("api/system/auto/on"), QStringLiteral("status"), controlCard), 2, 0);
    row1->addWidget(createApiButton(QStringLiteral("System AUTO OFF"), QStringLiteral("api/system/auto/off"), QStringLiteral("status"), controlCard), 2, 1);
    row1->addWidget(createApiButton(QStringLiteral("Sound AUTO ON"), QStringLiteral("api/sound/auto/on"), QStringLiteral("sound"), controlCard), 3, 0);
    row1->addWidget(createApiButton(QStringLiteral("Sound AUTO OFF"), QStringLiteral("api/sound/auto/off"), QStringLiteral("sound"), controlCard), 3, 1);
    row1->addWidget(createApiButton(QStringLiteral("LCD light ON"), QStringLiteral("api/display/backlight/on"), QStringLiteral("display"), controlCard), 4, 0);
    row1->addWidget(createApiButton(QStringLiteral("LCD light OFF"), QStringLiteral("api/display/backlight/off"), QStringLiteral("display"), controlCard), 4, 1);
    controlLayout->addLayout(row1);
    topGrid->addWidget(controlCard, 0, 2);

    auto *outputsCard = createCard(QStringLiteral("Outputs"), content);
    auto *outputsLayout = cardLayout(outputsCard);
    m_outputSummaryLabel = makeValueLabel(QStringLiteral("Outputs ON: - / 16"), outputsCard);
    outputsLayout->addWidget(m_outputSummaryLabel);
    auto *outputsGrid = new QGridLayout();
    outputsGrid->setHorizontalSpacing(6);
    outputsGrid->setVerticalSpacing(6);
    m_mainOutputLabels.clear();
    for (int id = 1; id <= OutputCount; ++id) {
        auto *label = makeValueLabel(QStringLiteral("OUT %1: -").arg(id), outputsCard);
        label->setMinimumHeight(28);
        m_mainOutputLabels.push_back(label);
        outputsGrid->addWidget(label, (id - 1) / 4, (id - 1) % 4);
    }
    outputsLayout->addLayout(outputsGrid);
    contentLayout->addWidget(outputsCard);

    auto *sensorCard = createCard(QStringLiteral("Sensors / temperature"), content);
    auto *sensorLayout = cardLayout(sensorCard);
    m_statusUpdatedLabel = makeValueLabel(ageText(QDateTime()), sensorCard);
    m_sensorSummaryLabel = makeValueLabel(QStringLiteral("No sensor data in last status response"), sensorCard);
    sensorLayout->addWidget(m_statusUpdatedLabel);
    sensorLayout->addWidget(m_sensorSummaryLabel);
    auto *sensorGrid = new QGridLayout();
    sensorGrid->setHorizontalSpacing(6);
    sensorGrid->setVerticalSpacing(6);
    m_sensorLabels.clear();
    for (int i = 0; i < 4; ++i) {
        auto *label = makeValueLabel(QStringLiteral("Sensor %1: n/a").arg(i + 1), sensorCard);
        m_sensorLabels.push_back(label);
        sensorGrid->addWidget(label, i / 2, i % 2);
    }
    sensorLayout->addLayout(sensorGrid);
    contentLayout->addWidget(sensorCard);

    auto *jsonCard = createCard(QStringLiteral("Status JSON"), content);
    auto *jsonLayout = cardLayout(jsonCard);
    m_statusJsonPreview = new QPlainTextEdit(jsonCard);
    m_statusJsonPreview->setReadOnly(true);
    m_statusJsonPreview->setMaximumBlockCount(400);
    m_statusJsonPreview->setMinimumHeight(120);
    m_statusJsonPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    jsonLayout->addWidget(m_statusJsonPreview);
    contentLayout->addWidget(jsonCard);

    contentLayout->addStretch(1);
    ui->tabWidget->insertTab(0, page, QStringLiteral("Main"));
    ui->tabWidget->setCurrentIndex(0);
}

void MainWindow::setupSensorTab()
{
    const int oldStatusIndex = ui->tabWidget->indexOf(ui->tab1_status);
    if (oldStatusIndex < 0) {
        return;
    }

    ui->tabWidget->removeTab(oldStatusIndex);

    auto *sensorPage = new QWidget(ui->tabWidget);
    sensorPage->setObjectName(QStringLiteral("tabSensor"));
    sensorPage->setStyleSheet(fernPageStyle(QStringLiteral("tabSensor")));

    auto *layout = new QVBoxLayout(sensorPage);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);

    auto *sensorTabs = new QTabWidget(sensorPage);
    sensorTabs->setObjectName(QStringLiteral("tabWidgetSensor"));
    sensorTabs->addTab(ui->tab1_status, QStringLiteral("Status"));
    layout->addWidget(sensorTabs);

    ui->tabWidget->insertTab(1, sensorPage, QStringLiteral("Sensor"));
}

void MainWindow::setupOutputsTab()
{
    QWidget *page = makeScrollPage(nullptr, QStringLiteral("tabOutputs"), true);
    QWidget *content = page->findChild<QWidget*>(QStringLiteral("tabOutputs_content"));
    auto *contentLayout = qobject_cast<QVBoxLayout*>(content->layout());

    auto *actionsCard = createCard(QStringLiteral("Outputs all"), content);
    auto *actionsLayout = cardLayout(actionsCard);
    auto *actionsGrid = new QGridLayout();
    actionsGrid->setHorizontalSpacing(6);
    actionsGrid->setVerticalSpacing(6);
    actionsGrid->addWidget(createApiButton(QStringLiteral("Refresh status"), QStringLiteral("api/status"), QString(), actionsCard), 0, 0);
    actionsGrid->addWidget(createApiButton(QStringLiteral("All ON"), QStringLiteral("api/outputs/on_all"), QStringLiteral("status"), actionsCard), 0, 1);
    actionsGrid->addWidget(createApiButton(QStringLiteral("All OFF"), QStringLiteral("api/outputs/off_all"), QStringLiteral("status"), actionsCard), 0, 2);
    actionsGrid->addWidget(createApiButton(QStringLiteral("Manual ALL ON"), QStringLiteral("api/outputs/manual/on_all"), QStringLiteral("status"), actionsCard), 1, 0);
    actionsGrid->addWidget(createApiButton(QStringLiteral("Manual ALL OFF"), QStringLiteral("api/outputs/manual/off_all"), QStringLiteral("status"), actionsCard), 1, 1);
    actionsGrid->addWidget(createApiButton(QStringLiteral("System AUTO ON"), QStringLiteral("api/system/auto/on"), QStringLiteral("status"), actionsCard), 2, 0);
    actionsGrid->addWidget(createApiButton(QStringLiteral("System AUTO OFF"), QStringLiteral("api/system/auto/off"), QStringLiteral("status"), actionsCard), 2, 1);
    actionsLayout->addLayout(actionsGrid);
    contentLayout->addWidget(actionsCard);

    auto *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    contentLayout->addLayout(grid);

    for (int id = 1; id <= OutputCount; ++id) {
        auto *card = createCard(QStringLiteral("Output %1").arg(id), content);
        card->setMinimumWidth(200);
        auto *layout = cardLayout(card);

        OutputUi outputUi;
        outputUi.card = card;
        outputUi.stateLabel = makeValueLabel(QStringLiteral("State: -"), card);
        outputUi.manualLabel = makeValueLabel(QStringLiteral("Manual: -"), card);
        outputUi.scheduleLabel = makeValueLabel(QStringLiteral("Auto ON --:-- / OFF --:--"), card);
        outputUi.autoOnTime = createTimeEdit(card);
        outputUi.autoOffTime = createTimeEdit(card);

        layout->addWidget(outputUi.stateLabel);
        layout->addWidget(outputUi.manualLabel);
        layout->addWidget(outputUi.scheduleLabel);

        auto *setRow = new QHBoxLayout();
        setRow->addWidget(createApiButton(QStringLiteral("ON"), QStringLiteral("api/output/%1/on").arg(id), QStringLiteral("status"), card));
        setRow->addWidget(createApiButton(QStringLiteral("OFF"), QStringLiteral("api/output/%1/off").arg(id), QStringLiteral("status"), card));
        layout->addLayout(setRow);

        auto *manualRow = new QHBoxLayout();
        manualRow->addWidget(createApiButton(QStringLiteral("Manual ON"), QStringLiteral("api/output/%1/manual/on").arg(id), QStringLiteral("status"), card));
        manualRow->addWidget(createApiButton(QStringLiteral("Manual OFF"), QStringLiteral("api/output/%1/manual/off").arg(id), QStringLiteral("status"), card));
        layout->addLayout(manualRow);

        auto *autoOnRow = new QHBoxLayout();
        autoOnRow->addWidget(new QLabel(QStringLiteral("Auto ON"), card));
        autoOnRow->addWidget(outputUi.autoOnTime);
        auto *autoOnButton = makeButton(QStringLiteral("Set"), card);
        connect(autoOnButton, &QPushButton::clicked, this, [this, id, outputUi]() {
            sendApiGet(QStringLiteral("api/output/%1/autoon?%2").arg(id).arg(timeQuery(outputUi.autoOnTime)),
                       QStringLiteral("command"),
                       QStringLiteral("status"));
        });
        autoOnRow->addWidget(autoOnButton);
        layout->addLayout(autoOnRow);

        auto *autoOffRow = new QHBoxLayout();
        autoOffRow->addWidget(new QLabel(QStringLiteral("Auto OFF"), card));
        autoOffRow->addWidget(outputUi.autoOffTime);
        auto *autoOffButton = makeButton(QStringLiteral("Set"), card);
        connect(autoOffButton, &QPushButton::clicked, this, [this, id, outputUi]() {
            sendApiGet(QStringLiteral("api/output/%1/autooff?%2").arg(id).arg(timeQuery(outputUi.autoOffTime)),
                       QStringLiteral("command"),
                       QStringLiteral("status"));
        });
        autoOffRow->addWidget(autoOffButton);
        layout->addLayout(autoOffRow);

        m_outputUi.insert(id, outputUi);
        grid->addWidget(card, (id - 1) / 4, (id - 1) % 4);
    }

    contentLayout->addStretch(1);

    ui->tabWidget->insertTab(qMin(2, ui->tabWidget->count()), page, QStringLiteral("Outputs"));
}

void MainWindow::setupScheduleTab()
{
    const int index = ui->tabWidget->indexOf(ui->tab3_harm);
    if (index >= 0) {
        ui->tabWidget->setTabText(index, QStringLiteral("Schedule"));
    }

    clearPage(ui->tab3_harm);
    auto *root = new QVBoxLayout(ui->tab3_harm);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(0);

    auto *scroll = new QScrollArea(ui->tab3_harm);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);
    scroll->setWidget(content);
    root->addWidget(scroll);

    auto *systemCard = createCard(QStringLiteral("System auto"), content);
    auto *systemLayout = cardLayout(systemCard);
    systemLayout->addWidget(makeValueLabel(QStringLiteral("Global scheduler switch for all outputs"), systemCard));
    auto *systemRow = new QHBoxLayout();
    systemRow->addWidget(createApiButton(QStringLiteral("AUTO ON"), QStringLiteral("api/system/auto/on"), QStringLiteral("status"), systemCard));
    systemRow->addWidget(createApiButton(QStringLiteral("AUTO OFF"), QStringLiteral("api/system/auto/off"), QStringLiteral("status"), systemCard));
    systemLayout->addLayout(systemRow);
    contentLayout->addWidget(systemCard);

    auto *allCard = createCard(QStringLiteral("All outputs schedule"), content);
    auto *allLayout = cardLayout(allCard);
    auto *allGrid = new QGridLayout();
    allGrid->setHorizontalSpacing(8);
    allGrid->setVerticalSpacing(8);
    auto *allOnTime = createTimeEdit(allCard);
    auto *allOffTime = createTimeEdit(allCard);
    allOffTime->setTime(QTime(20, 0));
    allGrid->addWidget(new QLabel(QStringLiteral("Auto ON time"), allCard), 0, 0);
    allGrid->addWidget(allOnTime, 0, 1);
    auto *setAllOn = makeButton(QStringLiteral("Set ON for all"), allCard);
    connect(setAllOn, &QPushButton::clicked, this, [this, allOnTime]() {
        sendApiGet(QStringLiteral("api/outputs/autoon_all?%1").arg(timeQuery(allOnTime)),
                   QStringLiteral("command"),
                   QStringLiteral("status"));
    });
    allGrid->addWidget(setAllOn, 0, 2);
    allGrid->addWidget(new QLabel(QStringLiteral("Auto OFF time"), allCard), 1, 0);
    allGrid->addWidget(allOffTime, 1, 1);
    auto *setAllOff = makeButton(QStringLiteral("Set OFF for all"), allCard);
    connect(setAllOff, &QPushButton::clicked, this, [this, allOffTime]() {
        sendApiGet(QStringLiteral("api/outputs/autooff_all?%1").arg(timeQuery(allOffTime)),
                   QStringLiteral("command"),
                   QStringLiteral("status"));
    });
    allGrid->addWidget(setAllOff, 1, 2);
    allLayout->addLayout(allGrid);
    contentLayout->addWidget(allCard);

    auto *hintCard = createCard(QStringLiteral("Per output schedule"), content);
    auto *hintLayout = cardLayout(hintCard);
    hintLayout->addWidget(makeValueLabel(QStringLiteral("Use the Outputs tab to set Auto ON/OFF time for each output separately."), hintCard));
    contentLayout->addWidget(hintCard);

    contentLayout->addStretch(1);
}

void MainWindow::setupSoundTab()
{
    const int index = ui->tabWidget->indexOf(ui->tab);
    if (index >= 0) {
        ui->tabWidget->setTabText(index, QStringLiteral("Sound"));
    }

    clearPage(ui->tab);
    auto *root = new QVBoxLayout(ui->tab);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(0);

    auto *scroll = new QScrollArea(ui->tab);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);
    scroll->setWidget(content);
    root->addWidget(scroll);

    auto *mainCard = createCard(QStringLiteral("Sound status and volume"), content);
    auto *mainLayout = cardLayout(mainCard);
    m_soundStatusLabel = makeValueLabel(ageText(QDateTime()), mainCard);
    mainLayout->addWidget(m_soundStatusLabel);

    auto *autoRow = new QHBoxLayout();
    autoRow->addWidget(createApiButton(QStringLiteral("Refresh sound"), QStringLiteral("api/sound/status"), QString(), mainCard));
    autoRow->addWidget(createApiButton(QStringLiteral("Refresh files"), QStringLiteral("api/sound/files"), QString(), mainCard));
    autoRow->addWidget(createApiButton(QStringLiteral("AUTO ON"), QStringLiteral("api/sound/auto/on"), QStringLiteral("sound"), mainCard));
    autoRow->addWidget(createApiButton(QStringLiteral("AUTO OFF"), QStringLiteral("api/sound/auto/off"), QStringLiteral("sound"), mainCard));
    mainLayout->addLayout(autoRow);

    auto *modeRow = new QHBoxLayout();
    m_soundMultipleCheckBox = new QCheckBox(QStringLiteral("multiple sounds"), mainCard);
    m_soundPlaybackModeLabel = makeValueLabel(QStringLiteral("Playback mode: single"), mainCard);
    connect(m_soundMultipleCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        sendApiGet(QStringLiteral("api/sound/mode/set?multiple=%1").arg(checked ? 1 : 0),
                   QStringLiteral("command"),
                   QStringLiteral("sound"));
    });
    modeRow->addWidget(m_soundMultipleCheckBox);
    modeRow->addWidget(m_soundPlaybackModeLabel, 1);
    mainLayout->addLayout(modeRow);

    auto *volumeRow = new QHBoxLayout();
    m_soundVolumeSlider = new QSlider(Qt::Horizontal, mainCard);
    m_soundVolumeSlider->setRange(0, 100);
    m_soundVolumeSlider->setValue(70);
    m_soundVolumeSpin = new QSpinBox(mainCard);
    m_soundVolumeSpin->setRange(0, 100);
    m_soundVolumeSpin->setValue(70);
    connect(m_soundVolumeSlider, &QSlider::valueChanged, m_soundVolumeSpin, &QSpinBox::setValue);
    connect(m_soundVolumeSpin, qOverload<int>(&QSpinBox::valueChanged), m_soundVolumeSlider, &QSlider::setValue);
    auto *setVolumeButton = makeButton(QStringLiteral("Set volume"), mainCard);
    connect(setVolumeButton, &QPushButton::clicked, this, [this]() {
        sendApiGet(QStringLiteral("api/sound/volume/set?value=%1").arg(m_soundVolumeSpin->value()),
                   QStringLiteral("command"),
                   QStringLiteral("sound"));
    });
    volumeRow->addWidget(new QLabel(QStringLiteral("Volume"), mainCard));
    volumeRow->addWidget(m_soundVolumeSlider, 1);
    volumeRow->addWidget(m_soundVolumeSpin);
    volumeRow->addWidget(createApiButton(QStringLiteral("Get"), QStringLiteral("api/sound/volume/get"), QStringLiteral("sound"), mainCard));
    volumeRow->addWidget(setVolumeButton);
    mainLayout->addLayout(volumeRow);
    contentLayout->addWidget(mainCard);

    auto *playCard = createCard(QStringLiteral("Play sound"), content);
    auto *playLayout = cardLayout(playCard);
    auto *playGrid = new QGridLayout();
    playGrid->setHorizontalSpacing(8);
    playGrid->setVerticalSpacing(8);
    setAvailableSoundFiles(fallbackSoundFiles());
    m_soundFileCombo = new QComboBox(playCard);
    m_soundFileCombo->addItems(m_availableSoundFiles);
    m_soundFileCombo->setMinimumWidth(260);
    m_soundFileEdit = new QLineEdit(m_availableSoundFiles.value(0), playCard);
    connect(m_soundFileCombo, &QComboBox::currentTextChanged, this, [this](const QString &file) {
        if (m_soundFileEdit) {
            m_soundFileEdit->setText(file);
        }
    });

    auto currentSoundFile = [this]() {
        const QString manual = m_soundFileEdit ? m_soundFileEdit->text().trimmed() : QString();
        if (!manual.isEmpty()) {
            return manual;
        }
        return m_soundFileCombo ? m_soundFileCombo->currentText().trimmed() : QString();
    };

    m_soundPlayVolumeSpin = new QSpinBox(playCard);
    m_soundPlayVolumeSpin->setRange(0, 100);
    m_soundPlayVolumeSpin->setValue(100);
    playGrid->addWidget(new QLabel(QStringLiteral("Select"), playCard), 0, 0);
    playGrid->addWidget(m_soundFileCombo, 0, 1, 1, 3);
    playGrid->addWidget(new QLabel(QStringLiteral("File"), playCard), 1, 0);
    playGrid->addWidget(m_soundFileEdit, 1, 1, 1, 3);
    playGrid->addWidget(new QLabel(QStringLiteral("Volume"), playCard), 2, 0);
    playGrid->addWidget(m_soundPlayVolumeSpin, 2, 1);
    auto *playWithVolume = makeButton(QStringLiteral("Play with volume"), playCard);
    connect(playWithVolume, &QPushButton::clicked, this, [this, currentSoundFile]() {
        const QString file = currentSoundFile();
        const int volume = m_soundPlayVolumeSpin->value();
        if (file.isEmpty()) {
            logMessage(QStringLiteral("SOUND: Empty file name."));
            return;
        }
        sendApiGet(QStringLiteral("api/sound/volume/set?value=%1").arg(volume),
                   QStringLiteral("command"),
                   QString());
        QTimer::singleShot(160, this, [this, file]() {
            sendApiGet(QStringLiteral("api/sound/play_current?file=%1").arg(encoded(file)),
                       QStringLiteral("command"),
                       QString());
        });
    });
    auto *playDirect = makeButton(QStringLiteral("Direct /play"), playCard);
    connect(playDirect, &QPushButton::clicked, this, [this, currentSoundFile]() {
        const QString file = currentSoundFile();
        if (file.isEmpty()) {
            logMessage(QStringLiteral("SOUND: Empty file name."));
            return;
        }
        sendApiGet(QStringLiteral("api/sound/play?file=%1&volume=%2")
                       .arg(encoded(file))
                       .arg(m_soundPlayVolumeSpin->value()),
                   QStringLiteral("command"),
                   QString());
    });
    auto *playCurrent = makeButton(QStringLiteral("Play current"), playCard);
    connect(playCurrent, &QPushButton::clicked, this, [this, currentSoundFile]() {
        const QString file = currentSoundFile();
        if (file.isEmpty()) {
            logMessage(QStringLiteral("SOUND: Empty file name."));
            return;
        }
        sendApiGet(QStringLiteral("api/sound/play_current?file=%1").arg(encoded(file)),
                   QStringLiteral("command"),
                   QString());
    });
    playGrid->addWidget(playWithVolume, 2, 2);
    playGrid->addWidget(playCurrent, 2, 3);
    playGrid->addWidget(playDirect, 3, 2, 1, 2);
    playLayout->addLayout(playGrid);
    contentLayout->addWidget(playCard);

    auto *quickPlayCard = createCard(QStringLiteral("Play sounds box"), content);
    auto *quickPlayLayout = cardLayout(quickPlayCard);
    m_quickPlayGroupsLayout = new QHBoxLayout();
    m_quickPlayGroupsLayout->setContentsMargins(0, 0, 0, 0);
    m_quickPlayGroupsLayout->setSpacing(10);
    quickPlayLayout->addLayout(m_quickPlayGroupsLayout);
    rebuildQuickSoundButtons();
    contentLayout->addWidget(quickPlayCard);

    auto *alarmsCard = createCard(QStringLiteral("Sound alarms"), content);
    auto *alarmsLayout = cardLayout(alarmsCard);
    auto *alarmsGrid = new QGridLayout();
    alarmsGrid->setHorizontalSpacing(6);
    alarmsGrid->setVerticalSpacing(6);
    alarmsGrid->addWidget(new QLabel(QStringLiteral("Slot"), alarmsCard), 0, 0);
    alarmsGrid->addWidget(new QLabel(QStringLiteral("Enabled"), alarmsCard), 0, 1);
    alarmsGrid->addWidget(new QLabel(QStringLiteral("Time"), alarmsCard), 0, 2);
    alarmsGrid->addWidget(new QLabel(QStringLiteral("File"), alarmsCard), 0, 3);
    alarmsGrid->addWidget(new QLabel(QStringLiteral("Vol"), alarmsCard), 0, 4);
    alarmsGrid->addWidget(new QLabel(QStringLiteral("Actions"), alarmsCard), 0, 5);
    alarmsGrid->addWidget(new QLabel(QStringLiteral("Status"), alarmsCard), 0, 6);

    m_soundAlarmUi.clear();
    for (int slot = 1; slot <= SoundAlarmCount; ++slot) {
        SoundAlarmUi alarmUi;
        alarmUi.enabled = new QCheckBox(alarmsCard);
        alarmUi.time = createTimeEdit(alarmsCard);
        alarmUi.file = new QLineEdit(QStringLiteral("sc1_sounds/terran_base_under_attack.wav"), alarmsCard);
        alarmUi.file->setMinimumWidth(210);
        alarmUi.volume = new QSpinBox(alarmsCard);
        alarmUi.volume->setRange(0, 100);
        alarmUi.volume->setValue(80);
        alarmUi.statusLabel = makeValueLabel(QStringLiteral("-"), alarmsCard);

        alarmsGrid->addWidget(new QLabel(QString::number(slot), alarmsCard), slot, 0);
        alarmsGrid->addWidget(alarmUi.enabled, slot, 1);
        alarmsGrid->addWidget(alarmUi.time, slot, 2);
        alarmsGrid->addWidget(alarmUi.file, slot, 3);
        alarmsGrid->addWidget(alarmUi.volume, slot, 4);

        auto *buttonCell = new QWidget(alarmsCard);
        auto *buttonLayout = new QHBoxLayout(buttonCell);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setSpacing(4);
        auto *setButton = makeButton(QStringLiteral("Set"), buttonCell);
        auto *enableButton = makeButton(QStringLiteral("Enable"), buttonCell);
        auto *disableButton = makeButton(QStringLiteral("Disable"), buttonCell);
        buttonLayout->addWidget(setButton);
        buttonLayout->addWidget(enableButton);
        buttonLayout->addWidget(disableButton);
        alarmsGrid->addWidget(buttonCell, slot, 5);
        alarmsGrid->addWidget(alarmUi.statusLabel, slot, 6);

        connect(setButton, &QPushButton::clicked, this, [this, slot, alarmUi]() {
            const QTime t = alarmUi.time->time();
            sendApiGet(QStringLiteral("api/sound/alarm/%1/set?enabled=%2&hour=%3&minute=%4&file=%5&volume=%6")
                           .arg(slot)
                           .arg(alarmUi.enabled->isChecked() ? 1 : 0)
                           .arg(t.hour())
                           .arg(t.minute())
                           .arg(encoded(alarmUi.file->text()))
                           .arg(alarmUi.volume->value()),
                       QStringLiteral("command"),
                       QStringLiteral("sound"));
        });
        connect(enableButton, &QPushButton::clicked, this, [this, slot]() {
            sendApiGet(QStringLiteral("api/sound/alarm/%1/enable").arg(slot),
                       QStringLiteral("command"),
                       QStringLiteral("sound"));
        });
        connect(disableButton, &QPushButton::clicked, this, [this, slot]() {
            sendApiGet(QStringLiteral("api/sound/alarm/%1/disable").arg(slot),
                       QStringLiteral("command"),
                       QStringLiteral("sound"));
        });

        m_soundAlarmUi.insert(slot, alarmUi);
    }

    alarmsLayout->addLayout(alarmsGrid);
    contentLayout->addWidget(alarmsCard);
    contentLayout->addStretch(1);
}

void MainWindow::setupDisplayTab()
{
    QWidget *page = makeScrollPage(nullptr, QStringLiteral("tabDisplay"), true);
    QWidget *content = page->findChild<QWidget*>(QStringLiteral("tabDisplay_content"));
    auto *contentLayout = qobject_cast<QVBoxLayout*>(content->layout());

    auto *statusCard = createCard(QStringLiteral("Display status"), content);
    auto *statusLayout = cardLayout(statusCard);
    m_displayStatusLabel = makeValueLabel(ageText(QDateTime()), statusCard);
    statusLayout->addWidget(m_displayStatusLabel);
    auto *statusRow = new QHBoxLayout();
    statusRow->addWidget(createApiButton(QStringLiteral("Refresh display"), QStringLiteral("api/display/status"), QString(), statusCard));
    statusRow->addWidget(createApiButton(QStringLiteral("Intro"), QStringLiteral("api/display/mode/intro"), QStringLiteral("display"), statusCard));
    statusRow->addWidget(createApiButton(QStringLiteral("Intro loop"), QStringLiteral("api/display/mode/intro_loop"), QStringLiteral("display"), statusCard));
    statusRow->addWidget(createApiButton(QStringLiteral("Clock"), QStringLiteral("api/display/mode/clock"), QStringLiteral("display"), statusCard));
    statusLayout->addLayout(statusRow);
    auto *backlightRow = new QHBoxLayout();
    backlightRow->addWidget(createApiButton(QStringLiteral("Backlight ON"), QStringLiteral("api/display/backlight/on"), QStringLiteral("display"), statusCard));
    backlightRow->addWidget(createApiButton(QStringLiteral("Backlight OFF"), QStringLiteral("api/display/backlight/off"), QStringLiteral("display"), statusCard));
    statusLayout->addLayout(backlightRow);
    contentLayout->addWidget(statusCard);

    auto *clockCard = createCard(QStringLiteral("Clock text"), content);
    auto *clockLayout = cardLayout(clockCard);
    auto *clockRow = new QHBoxLayout();
    m_displayClockTextEdit = new QLineEdit(QStringLiteral("GrowBox Desktop"), clockCard);
    clockRow->addWidget(new QLabel(QStringLiteral("Bottom text"), clockCard));
    clockRow->addWidget(m_displayClockTextEdit, 1);
    auto *setText = makeButton(QStringLiteral("Set text"), clockCard);
    connect(setText, &QPushButton::clicked, this, [this]() {
        sendApiGet(QStringLiteral("api/display/clock_text/set?text=%1").arg(encoded(m_displayClockTextEdit->text())),
                   QStringLiteral("command"),
                   QStringLiteral("display"));
    });
    clockRow->addWidget(setText);
    clockLayout->addLayout(clockRow);
    contentLayout->addWidget(clockCard);

    auto *rulesCard = createCard(QStringLiteral("Backlight rules"), content);
    auto *rulesLayout = cardLayout(rulesCard);
    auto *rulesGrid = new QGridLayout();
    rulesGrid->setHorizontalSpacing(8);
    rulesGrid->setVerticalSpacing(8);
    rulesGrid->addWidget(new QLabel(QStringLiteral("Rule"), rulesCard), 0, 0);
    rulesGrid->addWidget(new QLabel(QStringLiteral("Enabled"), rulesCard), 0, 1);
    rulesGrid->addWidget(new QLabel(QStringLiteral("Start"), rulesCard), 0, 2);
    rulesGrid->addWidget(new QLabel(QStringLiteral("End"), rulesCard), 0, 3);
    rulesGrid->addWidget(new QLabel(QStringLiteral("Action"), rulesCard), 0, 4);
    rulesGrid->addWidget(new QLabel(QStringLiteral("Status"), rulesCard), 0, 5);

    m_backlightRuleUi.clear();
    for (int id = 1; id <= BacklightRuleCount; ++id) {
        BacklightRuleUi ruleUi;
        ruleUi.enabled = new QCheckBox(rulesCard);
        ruleUi.start = createTimeEdit(rulesCard);
        ruleUi.end = createTimeEdit(rulesCard);
        ruleUi.end->setTime(QTime(22, 0));
        ruleUi.statusLabel = makeValueLabel(QStringLiteral("-"), rulesCard);

        auto *setRule = makeButton(QStringLiteral("Set"), rulesCard);
        connect(setRule, &QPushButton::clicked, this, [this, id, ruleUi]() {
            const QTime start = ruleUi.start->time();
            const QTime end = ruleUi.end->time();
            sendApiGet(QStringLiteral("api/display/backlight/rule/%1/set?enabled=%2&start_hour=%3&start_minute=%4&end_hour=%5&end_minute=%6")
                           .arg(id)
                           .arg(ruleUi.enabled->isChecked() ? 1 : 0)
                           .arg(start.hour())
                           .arg(start.minute())
                           .arg(end.hour())
                           .arg(end.minute()),
                       QStringLiteral("command"),
                       QStringLiteral("display"));
        });

        rulesGrid->addWidget(new QLabel(QString::number(id), rulesCard), id, 0);
        rulesGrid->addWidget(ruleUi.enabled, id, 1);
        rulesGrid->addWidget(ruleUi.start, id, 2);
        rulesGrid->addWidget(ruleUi.end, id, 3);
        rulesGrid->addWidget(setRule, id, 4);
        rulesGrid->addWidget(ruleUi.statusLabel, id, 5);
        m_backlightRuleUi.insert(id, ruleUi);
    }

    rulesLayout->addLayout(rulesGrid);
    contentLayout->addWidget(rulesCard);
    contentLayout->addStretch(1);

    const int logsIndex = ui->tabWidget->indexOf(ui->tab4_logs);
    ui->tabWidget->insertTab(logsIndex >= 0 ? logsIndex : ui->tabWidget->count(),
                             page,
                             QStringLiteral("Display"));
}

void MainWindow::on_pushButton_CONNECT_clicked()
{
    const QString ip = ui->lineEdit_IP->text().trimmed();

    if (ip.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("ERROR"), QStringLiteral("Wprowadz adres IP urzadzenia."));
        logMessage(QStringLiteral("ERROR: Wprowadz adres IP urzadzenia."));
        return;
    }

    logMessage(QStringLiteral("Kliknieto POLACZ (IP=%1)").arg(ip));
    requestAllStatus();
}

void MainWindow::logMessage(const QString &msg)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    ui->plainTextEditLog->appendPlainText(QStringLiteral("[%1] %2").arg(ts, msg));
}

void MainWindow::on_pushButton_CLEAR_LOG_clicked()
{
    ui->plainTextEditLog->clear();
    logMessage(QStringLiteral("Log wyczyszczony."));
}

void MainWindow::loadSettings()
{
    const QString cfgPath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("settings.cfg"));

    QSettings settings(cfgPath, QSettings::IniFormat);

    const QString ip = settings.value(QStringLiteral("network/ip"), QStringLiteral("192.168.0.185")).toString();
    const QString port = settings.value(QStringLiteral("network/port"), QStringLiteral("8080")).toString();
    const bool autoconnect = settings.value(QStringLiteral("network/autoconnect"), false).toBool();

    ui->checkBox_AUTOCONNECT->setChecked(autoconnect);
    ui->lineEdit_IP->setText(ip);
    ui->lineEdit_PORT->setText(port);

    logMessage(QStringLiteral("INFO: Zaladowanie ustawien."));
}

void MainWindow::saveSettings()
{
    const QString cfgPath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("settings.cfg"));

    QSettings settings(cfgPath, QSettings::IniFormat);

    settings.setValue(QStringLiteral("network/ip"), ui->lineEdit_IP->text().trimmed());
    settings.setValue(QStringLiteral("network/port"), ui->lineEdit_PORT->text().trimmed());
    settings.setValue(QStringLiteral("network/autoconnect"), ui->checkBox_AUTOCONNECT->isChecked());
    settings.sync();

    logMessage(QStringLiteral("INFO: Zapisanie ustawien."));
}

void MainWindow::onHttpFinished(QNetworkReply *reply)
{
    const QString path = reply->property("apiPath").toString();
    const QString kind = reply->property("kind").toString();
    const QString refreshAfter = reply->property("refreshAfter").toString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        const QString errorText = reply->errorString();
        qDebug() << "HTTP error:" << errorText;
        logMessage(QStringLiteral("NETWORK ERROR: %1 -> %2").arg(path, errorText));
        statusBar()->showMessage(QStringLiteral("HTTP error: %1").arg(errorText), 5000);
        reply->deleteLater();
        return;
    }

    logMessage(QStringLiteral("NETWORK OK [%1]: %2").arg(httpStatus).arg(path));

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        const QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("error"))) {
            logMessage(QStringLiteral("API ERROR: %1").arg(root.value(QStringLiteral("error")).toString()));
        }
        if (root.contains(QStringLiteral("result"))) {
            QStringList details;
            details << QStringLiteral("result=%1").arg(root.value(QStringLiteral("result")).toString());
            if (root.contains(QStringLiteral("file"))) {
                details << QStringLiteral("file=%1").arg(root.value(QStringLiteral("file")).toString());
            }
            if (root.contains(QStringLiteral("volume"))) {
                details << QStringLiteral("volume=%1").arg(root.value(QStringLiteral("volume")).toInt());
            }
            if (root.contains(QStringLiteral("value"))) {
                details << QStringLiteral("value=%1").arg(root.value(QStringLiteral("value")).toInt());
            }
            logMessage(QStringLiteral("API: %1").arg(details.join(QStringLiteral(", "))));
        }

        if (path == QStringLiteral("api/status") || kind == QStringLiteral("status")) {
            parseStatus(root, body);
        } else if (path == QStringLiteral("api/sound/status")
                   || path == QStringLiteral("api/sound/files")
                   || path == QStringLiteral("api/sound/volume/get")
                   || kind == QStringLiteral("sound")) {
            parseSoundStatus(root);
        } else if (path == QStringLiteral("api/display/status") || kind == QStringLiteral("display")) {
            parseDisplayStatus(root);
        }
    } else if (!body.isEmpty()) {
        logMessage(QStringLiteral("NETWORK BODY: %1").arg(QString::fromUtf8(body.left(300))));
    }

    if (refreshAfter == QStringLiteral("all")) {
        QTimer::singleShot(180, this, &MainWindow::requestAllStatus);
    } else if (refreshAfter == QStringLiteral("status")) {
        QTimer::singleShot(180, this, &MainWindow::requestStatus);
    } else if (refreshAfter == QStringLiteral("sound")) {
        QTimer::singleShot(180, this, &MainWindow::requestSoundStatus);
    } else if (refreshAfter == QStringLiteral("display")) {
        QTimer::singleShot(180, this, &MainWindow::requestDisplayStatus);
    }

    reply->deleteLater();
}

void MainWindow::sendGet(const QString &path)
{
    sendApiGet(path, QStringLiteral("command"), QStringLiteral("status"));
}

void MainWindow::sendApiGet(const QString &path, const QString &kind, const QString &refreshAfter)
{
    const QString ip = ui->lineEdit_IP->text().trimmed();
    const QString port = ui->lineEdit_PORT->text().trimmed();

    if (ip.isEmpty() || port.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Blad"), QStringLiteral("Podaj IP i port."));
        return;
    }

    bool portOk = false;
    const int portNumber = port.toInt(&portOk);
    if (!portOk || portNumber <= 0 || portNumber > 65535) {
        QMessageBox::warning(this, QStringLiteral("Blad"), QStringLiteral("Port musi byc liczba 1-65535."));
        return;
    }

    QString apiPath = path.trimmed();
    while (apiPath.startsWith(QLatin1Char('/'))) {
        apiPath.remove(0, 1);
    }

    QString pathPart = apiPath;
    QString queryPart;
    const int queryPos = apiPath.indexOf(QLatin1Char('?'));
    if (queryPos >= 0) {
        pathPart = apiPath.left(queryPos);
        queryPart = apiPath.mid(queryPos + 1);
    }

    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(ip);
    url.setPort(portNumber);
    url.setPath(QStringLiteral("/") + pathPart);
    if (!queryPart.isEmpty()) {
        url.setQuery(queryPart, QUrl::TolerantMode);
    }

    logMessage(QStringLiteral("NETWORK: GET %1").arg(url.toString(QUrl::FullyEncoded)));

    QNetworkRequest req{url};
    req.setRawHeader("Accept", "application/json, text/plain, */*");
    req.setRawHeader("Connection", "close");
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("GrowBoxDesktop/%1").arg(VERSION_SW));
    req.setTransferTimeout(20000);
    QNetworkReply *reply = m_netManager->get(req);
    reply->setProperty("apiPath", apiPath);
    reply->setProperty("kind", kind);
    reply->setProperty("refreshAfter", refreshAfter);
}

void MainWindow::requestStatus()
{
    sendApiGet(QStringLiteral("api/status"), QStringLiteral("status"), QString());
}

void MainWindow::requestSoundStatus()
{
    sendApiGet(QStringLiteral("api/sound/status"), QStringLiteral("sound"), QString());
}

void MainWindow::requestDisplayStatus()
{
    sendApiGet(QStringLiteral("api/display/status"), QStringLiteral("display"), QString());
}

void MainWindow::requestAllStatus()
{
    requestStatus();
    requestSoundStatus();
    requestDisplayStatus();
}

void MainWindow::setupLedButtons()
{
    auto connectLed = [this](QPushButton *btn, const QString &path)
    {
        if (!btn) {
            return;
        }

        connect(btn, &QPushButton::clicked, this, [this, path]() {
            sendGet(path);
        });
    };

    connectLed(ui->pushButton_LED1ON,  QStringLiteral("api/output/1/on"));
    connectLed(ui->pushButton_LED1OFF, QStringLiteral("api/output/1/off"));
    connectLed(ui->pushButton_LED2ON,  QStringLiteral("api/output/2/on"));
    connectLed(ui->pushButton_LED2OFF, QStringLiteral("api/output/2/off"));
    connectLed(ui->pushButton_LED3ON,  QStringLiteral("api/output/3/on"));
    connectLed(ui->pushButton_LED3OFF, QStringLiteral("api/output/3/off"));
    connectLed(ui->pushButton_LED4ON,  QStringLiteral("api/output/4/on"));
    connectLed(ui->pushButton_LED4OFF, QStringLiteral("api/output/4/off"));
    connectLed(ui->pushButton_LED5ON,  QStringLiteral("api/output/5/on"));
    connectLed(ui->pushButton_LED5OFF, QStringLiteral("api/output/5/off"));
    connectLed(ui->pushButton_LED6ON,  QStringLiteral("api/output/6/on"));
    connectLed(ui->pushButton_LED6OFF, QStringLiteral("api/output/6/off"));
    connectLed(ui->pushButton_LED7ON,  QStringLiteral("api/output/7/on"));
    connectLed(ui->pushButton_LED7OFF, QStringLiteral("api/output/7/off"));
    connectLed(ui->pushButton_LED8ON,  QStringLiteral("api/output/8/on"));
    connectLed(ui->pushButton_LED8OFF, QStringLiteral("api/output/8/off"));
    connectLed(ui->pushButton_LED_ALL_ON,  QStringLiteral("api/outputs/on_all"));
    connectLed(ui->pushButton_LED_ALL_OFF, QStringLiteral("api/outputs/off_all"));
}

void MainWindow::parseStatus(const QJsonObject &root, const QByteArray &body)
{
    m_lastStatusUpdate = QDateTime::currentDateTime();

    const QString localTime = root.value(QStringLiteral("deviceLocalTime")).toString(QStringLiteral("-"));
    if (m_deviceTimeLabel) {
        m_deviceTimeLabel->setText(QStringLiteral("Device time: %1").arg(localTime));
    }

    const bool globalAuto = root.value(QStringLiteral("globalAutoMode")).toBool(false);
    setStateChip(m_globalAutoLabel, globalAuto, QStringLiteral("System auto: ON"), QStringLiteral("System auto: OFF"));

    const bool soundAuto = root.value(QStringLiteral("globalSoundAutoMode")).toBool(false);
    setStateChip(m_soundAutoLabel, soundAuto, QStringLiteral("Sound auto: ON"), QStringLiteral("Sound auto: OFF"));

    if (root.contains(QStringLiteral("globalSoundVolume")) && m_soundVolumeLabel) {
        const int volume = root.value(QStringLiteral("globalSoundVolume")).toInt();
        m_soundVolumeLabel->setText(QStringLiteral("Sound volume: %1").arg(volume));
        if (m_soundVolumeSpin && m_soundVolumeSlider) {
            QSignalBlocker spinBlock(m_soundVolumeSpin);
            QSignalBlocker sliderBlock(m_soundVolumeSlider);
            m_soundVolumeSpin->setValue(volume);
            m_soundVolumeSlider->setValue(volume);
        }
    }

    int onCount = 0;
    const QJsonArray outputs = root.value(QStringLiteral("outputs")).toArray();
    for (const QJsonValue &value : outputs) {
        const QJsonObject output = value.toObject();
        const int id = output.value(QStringLiteral("id")).toInt();
        const bool state = output.value(QStringLiteral("state")).toBool(false);
        const int autoOnHour = output.value(QStringLiteral("autoOnHour")).toInt(-1);
        const int autoOnMinute = output.value(QStringLiteral("autoOnMinute")).toInt(0);
        const int autoOffHour = output.value(QStringLiteral("autoOffHour")).toInt(-1);
        const int autoOffMinute = output.value(QStringLiteral("autoOffMinute")).toInt(0);
        const bool manualMode = output.value(QStringLiteral("manualMode")).toBool(false);

        if (state) {
            ++onCount;
        }

        updateOutputState(id, state, autoOnHour, autoOnMinute, autoOffHour, autoOffMinute, manualMode);
    }

    if (m_outputSummaryLabel) {
        m_outputSummaryLabel->setText(QStringLiteral("Outputs ON: %1 / %2").arg(onCount).arg(OutputCount));
    }

    setGroupBoxState(ui->groupBoxLED_ALL, onCount > 0);
    updateSensorWidgets(root);

    if (m_statusJsonPreview) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        m_statusJsonPreview->setPlainText(parseError.error == QJsonParseError::NoError
                                              ? QString::fromUtf8(doc.toJson(QJsonDocument::Indented))
                                              : QString::fromUtf8(body));
    }

    updateAgeLabels();
}

void MainWindow::parseSoundStatus(const QJsonObject &root)
{
    m_lastSoundUpdate = QDateTime::currentDateTime();

    if (root.contains(QStringLiteral("globalSoundVolume"))) {
        const int volume = root.value(QStringLiteral("globalSoundVolume")).toInt();
        if (m_soundVolumeLabel) {
            m_soundVolumeLabel->setText(QStringLiteral("Sound volume: %1").arg(volume));
        }
        if (m_soundVolumeSpin && m_soundVolumeSlider) {
            QSignalBlocker spinBlock(m_soundVolumeSpin);
            QSignalBlocker sliderBlock(m_soundVolumeSlider);
            m_soundVolumeSpin->setValue(volume);
            m_soundVolumeSlider->setValue(volume);
        }
    }

    if (root.contains(QStringLiteral("globalSoundAutoMode"))) {
        const bool soundAuto = root.value(QStringLiteral("globalSoundAutoMode")).toBool(false);
        setStateChip(m_soundAutoLabel, soundAuto, QStringLiteral("Sound auto: ON"), QStringLiteral("Sound auto: OFF"));
    }

    if (root.contains(QStringLiteral("soundMultipleMode"))) {
        const bool multiple = root.value(QStringLiteral("soundMultipleMode")).toBool(false);
        if (m_soundMultipleCheckBox) {
            QSignalBlocker block(m_soundMultipleCheckBox);
            m_soundMultipleCheckBox->setChecked(multiple);
        }
        if (m_soundPlaybackModeLabel) {
            const int activeCount = root.value(QStringLiteral("activeSoundCount")).toInt(0);
            m_soundPlaybackModeLabel->setText(QStringLiteral("Playback mode: %1, active: %2")
                                                  .arg(multiple ? QStringLiteral("multiple") : QStringLiteral("single"))
                                                  .arg(activeCount));
            m_soundPlaybackModeLabel->setStyleSheet(multiple ? chipStyle(true) : mutedChipStyle());
        }
    }

    QJsonArray files = root.value(QStringLiteral("availableFiles")).toArray();
    if (files.isEmpty()) {
        files = root.value(QStringLiteral("files")).toArray();
    }
    if (!files.isEmpty()) {
        QStringList fileNames;
        for (const QJsonValue &value : files) {
            const QString file = value.toString().trimmed();
            if (!file.isEmpty()) {
                fileNames << file;
            }
        }
        setAvailableSoundFiles(fileNames);
    }

    const QJsonArray alarms = root.value(QStringLiteral("soundAlarms")).toArray();
    for (const QJsonValue &value : alarms) {
        const QJsonObject alarm = value.toObject();
        const int slot = alarm.value(QStringLiteral("slot")).toInt();
        if (!m_soundAlarmUi.contains(slot)) {
            continue;
        }

        SoundAlarmUi alarmUi = m_soundAlarmUi.value(slot);
        const bool enabled = alarm.value(QStringLiteral("enabled")).toBool(false);
        const int hour = alarm.value(QStringLiteral("hour")).toInt(0);
        const int minute = alarm.value(QStringLiteral("minute")).toInt(0);
        const int volume = alarm.value(QStringLiteral("volume")).toInt(0);
        const QString file = alarm.value(QStringLiteral("file")).toString();

        if (alarmUi.enabled) {
            QSignalBlocker block(alarmUi.enabled);
            alarmUi.enabled->setChecked(enabled);
        }
        if (alarmUi.time) {
            alarmUi.time->setTime(QTime(hour, minute));
        }
        if (alarmUi.file && !file.isEmpty()) {
            alarmUi.file->setText(file);
        }
        if (alarmUi.volume) {
            alarmUi.volume->setValue(volume);
        }
        if (alarmUi.statusLabel) {
            alarmUi.statusLabel->setText(QStringLiteral("%1 %2:%3")
                                             .arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                                             .arg(hour, 2, 10, QLatin1Char('0'))
                                             .arg(minute, 2, 10, QLatin1Char('0')));
            alarmUi.statusLabel->setStyleSheet(chipStyle(enabled));
        }
    }

    updateAgeLabels();
}

void MainWindow::parseDisplayStatus(const QJsonObject &root)
{
    m_lastDisplayUpdate = QDateTime::currentDateTime();

    const QString mode = root.value(QStringLiteral("mode")).toString(QStringLiteral("-"));
    const bool backlight = root.value(QStringLiteral("backlightOn")).toBool(false);
    const bool lcdInitialized = root.value(QStringLiteral("lcdInitialized")).toBool(false);
    const QString clockText = root.value(QStringLiteral("clockBottomText")).toString();

    if (m_displayModeLabel) {
        m_displayModeLabel->setText(QStringLiteral("LCD mode: %1").arg(mode));
    }
    setStateChip(m_backlightLabel, backlight, QStringLiteral("Backlight: ON"), QStringLiteral("Backlight: OFF"));
    setStateChip(m_lcdLabel, lcdInitialized, QStringLiteral("LCD init: ON"), QStringLiteral("LCD init: OFF"));
    if (m_displayClockTextEdit && !clockText.isEmpty()) {
        m_displayClockTextEdit->setText(clockText);
    }

    const QJsonArray rules = root.value(QStringLiteral("backlightRules")).toArray();
    for (const QJsonValue &value : rules) {
        const QJsonObject rule = value.toObject();
        const int id = rule.value(QStringLiteral("id")).toInt();
        if (!m_backlightRuleUi.contains(id)) {
            continue;
        }

        BacklightRuleUi ruleUi = m_backlightRuleUi.value(id);
        const bool enabled = rule.value(QStringLiteral("enabled")).toBool(false);
        const int startHour = rule.value(QStringLiteral("startHour")).toInt(0);
        const int startMinute = rule.value(QStringLiteral("startMinute")).toInt(0);
        const int endHour = rule.value(QStringLiteral("endHour")).toInt(0);
        const int endMinute = rule.value(QStringLiteral("endMinute")).toInt(0);

        if (ruleUi.enabled) {
            QSignalBlocker block(ruleUi.enabled);
            ruleUi.enabled->setChecked(enabled);
        }
        if (ruleUi.start) {
            ruleUi.start->setTime(QTime(startHour, startMinute));
        }
        if (ruleUi.end) {
            ruleUi.end->setTime(QTime(endHour, endMinute));
        }
        if (ruleUi.statusLabel) {
            ruleUi.statusLabel->setText(QStringLiteral("%1 %2-%3")
                                            .arg(enabled ? QStringLiteral("ON") : QStringLiteral("OFF"))
                                            .arg(timeText(startHour, startMinute))
                                            .arg(timeText(endHour, endMinute)));
            ruleUi.statusLabel->setStyleSheet(chipStyle(enabled));
        }
    }

    updateAgeLabels();
}

void MainWindow::updateOutputState(int id,
                                   bool state,
                                   int autoOnHour,
                                   int autoOnMinute,
                                   int autoOffHour,
                                   int autoOffMinute,
                                   bool manualMode)
{
    if (id < 1 || id > OutputCount) {
        return;
    }

    const QString stateText = state ? QStringLiteral("ON") : QStringLiteral("OFF");
    if (id - 1 < m_mainOutputLabels.size()) {
        QLabel *label = m_mainOutputLabels.at(id - 1);
        label->setText(QStringLiteral("OUT %1: %2").arg(id).arg(stateText));
        label->setStyleSheet(chipStyle(state));
    }

    if (m_outputUi.contains(id)) {
        OutputUi outputUi = m_outputUi.value(id);
        setStateChip(outputUi.stateLabel, state, QStringLiteral("State: ON"), QStringLiteral("State: OFF"));
        setStateChip(outputUi.manualLabel, manualMode, QStringLiteral("Manual: ON"), QStringLiteral("Manual: OFF"));
        if (outputUi.scheduleLabel) {
            outputUi.scheduleLabel->setText(QStringLiteral("Auto ON %1 / OFF %2")
                                                .arg(timeText(autoOnHour, autoOnMinute))
                                                .arg(timeText(autoOffHour, autoOffMinute)));
        }
        if (outputUi.autoOnTime && autoOnHour >= 0) {
            outputUi.autoOnTime->setTime(QTime(autoOnHour, autoOnMinute));
        }
        if (outputUi.autoOffTime && autoOffHour >= 0) {
            outputUi.autoOffTime->setTime(QTime(autoOffHour, autoOffMinute));
        }
        if (outputUi.card) {
            outputUi.card->setStyleSheet(cardStyle()
                                         + QStringLiteral("QFrame[card=\"true\"] { border-color: %1; }")
                                               .arg(state ? QStringLiteral("#44ff07") : QStringLiteral("#ff4242")));
        }
    }

    switch (id) {
    case 1: setGroupBoxState(ui->groupBoxLED1, state); break;
    case 2: setGroupBoxState(ui->groupBoxLED2, state); break;
    case 3: setGroupBoxState(ui->groupBoxLED3, state); break;
    case 4: setGroupBoxState(ui->groupBoxLED4, state); break;
    case 5: setGroupBoxState(ui->groupBoxLED5, state); break;
    case 6: setGroupBoxState(ui->groupBoxLED6, state); break;
    case 7: setGroupBoxState(ui->groupBoxLED7, state); break;
    case 8: setGroupBoxState(ui->groupBoxLED8, state); break;
    default: break;
    }
}

void MainWindow::updateSensorWidgets(const QJsonObject &root)
{
    const QJsonArray sensors = root.value(QStringLiteral("sensors")).toArray();
    bool anySensorData = false;

    for (int i = 0; i < m_sensorLabels.size(); ++i) {
        QString text = QStringLiteral("Sensor %1: n/a").arg(i + 1);
        if (i < sensors.size() && sensors.at(i).isObject()) {
            const QJsonObject sensor = sensors.at(i).toObject();
            const int id = sensor.value(QStringLiteral("id")).toInt(i + 1);
            const QJsonValue tempValue = sensor.contains(QStringLiteral("temperature"))
                                             ? sensor.value(QStringLiteral("temperature"))
                                             : sensor.value(QStringLiteral("temp"));
            const QJsonValue humidityValue = sensor.value(QStringLiteral("humidity"));
            const QString state = sensor.value(QStringLiteral("state")).toString();

            QStringList parts;
            if (!tempValue.isUndefined()) {
                parts << QStringLiteral("T %1 C").arg(tempValue.toDouble(), 0, 'f', 1);
            }
            if (!humidityValue.isUndefined()) {
                parts << QStringLiteral("H %1 %").arg(humidityValue.toDouble(), 0, 'f', 1);
            }
            if (!state.isEmpty()) {
                parts << state;
            }
            text = QStringLiteral("Sensor %1: %2").arg(id).arg(parts.isEmpty() ? QStringLiteral("n/a") : parts.join(QStringLiteral(", ")));
            anySensorData = true;
        }
        m_sensorLabels.at(i)->setText(text);
    }

    if (m_sensorSummaryLabel) {
        m_sensorSummaryLabel->setText(anySensorData
                                          ? QStringLiteral("Sensor values from /api/status")
                                          : QStringLiteral("No sensor data in last status response"));
    }
}

void MainWindow::updateAgeLabels()
{
    if (m_mainUpdatedLabel) {
        m_mainUpdatedLabel->setText(ageText(m_lastStatusUpdate));
    }
    if (m_statusUpdatedLabel) {
        m_statusUpdatedLabel->setText(ageText(m_lastStatusUpdate));
    }
    for (QLabel *label : m_oldSensorUpdateLabels) {
        if (label) {
            label->setText(ageText(m_lastStatusUpdate));
        }
    }
    if (m_soundStatusLabel) {
        m_soundStatusLabel->setText(ageText(m_lastSoundUpdate));
    }
    if (m_displayStatusLabel) {
        m_displayStatusLabel->setText(ageText(m_lastDisplayUpdate));
    }
}

void MainWindow::setStateChip(QLabel *label, bool on, const QString &onText, const QString &offText) const
{
    if (!label) {
        return;
    }

    label->setText(on ? onText : offText);
    label->setStyleSheet(chipStyle(on));
}

void MainWindow::setGroupBoxState(QGroupBox *groupBox, bool on) const
{
    if (!groupBox) {
        return;
    }

    groupBox->setStyleSheet(groupBoxStateStyle(on, groupBox->objectName()));
}

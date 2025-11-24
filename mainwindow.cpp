#include "mainwindow.h"
#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <unistd.h>
#include <sys/types.h>
#include <QInputDialog>
#include <QLabel>
#include <QTimer>
#include <QBrush>
#include <QDateTime>
#include <QScrollBar>
#include "PortBreaker.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_portBreaker = new PortBreaker(this);    
    connect(m_portBreaker, &PortBreaker::logMessage, this, &MainWindow::appendLog);
    connect(m_portBreaker, &PortBreaker::watchdogTriggered, this, &MainWindow::onWatchdogTriggered);
    if (!isRoot()) {
        QMessageBox::critical(this, "Root Error", "Application must be run as root.");
        exit(1);}
    QWidget* centralWidget = new QWidget;
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    QHBoxLayout* timerLayout = new QHBoxLayout();    
    m_timerSpinBox = new QSpinBox();
    m_timerSpinBox->setRange(1, 86400);
    m_timerSpinBox->setValue(5);
    timerLayout->addWidget(new QLabel("Czas (sek):"));
    timerLayout->addWidget(m_timerSpinBox);    
    m_showAllDevices = new QCheckBox("Show all fake_sys");
    m_showAllDevices->setChecked(false);    
    m_refreshButton = new QPushButton("Refresh");
    m_enableButton = new QPushButton("Enable");
    m_disableButton = new QPushButton("Disable");
    m_disableWithTimerButton = new QPushButton("Disable (timer)");
    m_resetSysfsButton = new QPushButton("Reset (sysfs)");
    m_resetIoctlButton = new QPushButton("Reset (ioctl)");
    m_resetAllButton = new QPushButton("Reset all ports");
    m_toggleWakeupButton = new QPushButton("on/off  wake-up (ACPI)");    
    m_watchdogCheckBox = new QCheckBox("Monitor (watchdog)");    
    centralLayout->addWidget(m_refreshButton);
    centralLayout->addWidget(m_showAllDevices);
    centralLayout->addWidget(m_enableButton);
    centralLayout->addWidget(m_disableButton);
    centralLayout->addLayout(timerLayout);
    centralLayout->addWidget(m_disableWithTimerButton);
    centralLayout->addWidget(m_resetSysfsButton);
    centralLayout->addWidget(m_resetIoctlButton);
    centralLayout->addWidget(m_toggleWakeupButton);
    centralLayout->addWidget(m_watchdogCheckBox); 
    centralLayout->addWidget(m_resetAllButton);    
    setCentralWidget(centralWidget);    
    m_deviceTable = new QTableWidget;
    m_deviceTable->setColumnCount(5);
    m_deviceTable->setHorizontalHeaderLabels({"VID:PID", "Nazwa Urzadzenia", "Status", "Sysfs", "Wake-up"});
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceTable->setSortingEnabled(true);    
    QSettings settings("PortBreakerFramework", "USBManager");
    QByteArray headerState = settings.value("tableHeaderState").toByteArray();
    if (!headerState.isEmpty()) {
        m_deviceTable->horizontalHeader()->restoreState(headerState);
    } else {
        m_deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
        m_deviceTable->setColumnWidth(0, 90);
        m_deviceTable->setColumnWidth(2, 90);
        m_deviceTable->setColumnWidth(4, 90);}
    QDockWidget* deviceDock = new QDockWidget("Lista Urzadzen", this);
    deviceDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    deviceDock->setWidget(m_deviceTable);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock);
    m_logDock = new QDockWidget("System Logs", this);
    m_logDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_logConsole = new QTextEdit();
    m_logConsole->setReadOnly(true);
    m_logConsole->setStyleSheet("background-color: #222; color: #0f0; font-family: Monospace;");
    m_logDock->setWidget(m_logConsole);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshButton);
    connect(m_enableButton, &QPushButton::clicked, this, &MainWindow::onEnableButton);
    connect(m_disableButton, &QPushButton::clicked, this, &MainWindow::onDisableButton);
    connect(m_disableWithTimerButton, &QPushButton::clicked, this, &MainWindow::onDisableWithTimerButton);
    connect(m_resetSysfsButton, &QPushButton::clicked, this, &MainWindow::onResetSysfsButton);
    connect(m_resetIoctlButton, &QPushButton::clicked, this, &MainWindow::onResetIoctlButton);
    connect(m_resetAllButton, &QPushButton::clicked, this, &MainWindow::onResetAllButton);
    connect(m_toggleWakeupButton, &QPushButton::clicked, this, &MainWindow::onToggleWakeupButton);
    connect(m_showAllDevices, &QCheckBox::toggled, this, &MainWindow::onShowAllDevicesToggled);
    connect(m_watchdogCheckBox, &QCheckBox::toggled, this, &MainWindow::onWatchdogToggled);
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    m_timerSpinBox->setValue(settings.value("timerValue", 5).toInt());
    m_showAllDevices->setChecked(settings.value("showAll", false).toBool());    
    updateDeviceTable();}

MainWindow::~MainWindow() {
    QSettings settings("PortBreakerFramework", "USBManager");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("timerValue", m_timerSpinBox->value());
    settings.setValue("showAll", m_showAllDevices->isChecked());
    if (m_deviceTable && m_deviceTable->horizontalHeader()) {
        settings.setValue("tableHeaderState", m_deviceTable->horizontalHeader()->saveState());}
    delete m_portBreaker;}

bool MainWindow::isRoot() {
    return geteuid() == 0;}

void MainWindow::appendLog(QString msg, int level) {
    QString color = "#00ff00";
    if (level == 1) color = "#ff5555";
    if (level == 2) color = "#55ffff";
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString html = QString("<span style='color: #888;'>[%1]</span> <span style='color: %2;'>%3</span>")
            .arg(timestamp, color, msg);    
    m_logConsole->append(html);
    QScrollBar *sb = m_logConsole->verticalScrollBar();
    sb->setValue(sb->maximum());}

void MainWindow::onWatchdogToggled(bool checked) {
    if (checked) {
        QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
        if (selectedItems.isEmpty()) {
            m_watchdogCheckBox->setChecked(false);
            appendLog("Select a device first to monitor!", 1);
            return;}
        int row = selectedItems.first()->row();
        QString vid_pid = m_deviceTable->item(row, 0)->text();
        if (vid_pid == "N/A") {
             m_watchdogCheckBox->setChecked(false);
             appendLog("Cannot monitor a device without VID:PID", 1);
             return;}
        m_portBreaker->startWatchdog(vid_pid.toStdString(), 1000);
    } else {
        m_portBreaker->stopWatchdog();}}

void MainWindow::onWatchdogTriggered(QString vid_pid) {
    m_watchdogCheckBox->setChecked(false);
    QMessageBox::warning(this, "Watchdog Alert", "Device " + vid_pid + " disconnected unexpectedly!");}

void MainWindow::onRefreshButton() {
    updateDeviceTable();
    appendLog("Device list refreshed", 0);}

void MainWindow::onEnableButton() {
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        appendLog("Select a device to enable", 1);
        return;}
    int row = selectedItems.first()->row();
    QString sysfs_path = m_deviceTable->item(row, 3)->text();    
    if (m_portBreaker->enableDeviceByPath(sysfs_path.toStdString())) {
        updateDeviceTable();}}

void MainWindow::onDisableButton() {
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        appendLog("Select a device to disable", 1);
        return;}
    int row = selectedItems.first()->row();
    QString sysfs_path = m_deviceTable->item(row, 3)->text();    
    if (m_portBreaker->disableDeviceByPath(sysfs_path.toStdString())) {
        updateDeviceTable();}}

void MainWindow::onResetSysfsButton() {
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        appendLog("Select a device to reset", 1);
        return;}
    int row = selectedItems.first()->row();
    QString sysfs_path = m_deviceTable->item(row, 3)->text();    
    if (m_portBreaker->resetDeviceSysfsByPath(sysfs_path.toStdString())) {
        updateDeviceTable();}}

void MainWindow::onResetIoctlButton() {
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        appendLog("Select a device to reset", 1);
        return;}
    int row = selectedItems.first()->row();
    QString vid_pid = m_deviceTable->item(row, 0)->text();    
    if (vid_pid == "N/A") {
        appendLog("Error: ioctl reset requires VID:PID", 1);
        return;}    
    if (m_portBreaker->resetDeviceIoctl(vid_pid.toStdString())) {
        updateDeviceTable();}}

void MainWindow::onResetAllButton() {
    if (m_portBreaker->resetAllDevicesSysfs()) {
        updateDeviceTable();}}

void MainWindow::onToggleWakeupButton() {
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        appendLog("Select a device to wake-up", 1);
        return;}
    int row = selectedItems.first()->row();
    QString sysfs_path = m_deviceTable->item(row, 3)->text();    
    if (m_portBreaker->toggleWakeupByPath(sysfs_path.toStdString())) {
        updateDeviceTable();}}

void MainWindow::onDisableWithTimerButton() {
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        appendLog("Select a device to disable", 1);
        return;}
    int row = selectedItems.first()->row();
    QString sysfs_path = m_deviceTable->item(row, 3)->text();
    QString vid_pid = m_deviceTable->item(row, 0)->text();    
    std::string path_str = sysfs_path.toStdString();
    int initial_seconds = m_timerSpinBox->value();    
    if (m_portBreaker->disableDeviceByPath(path_str)) {        
        updateDeviceTable();
        appendLog(QString("Timer: Disabled %1. Countdown started (%2s)").arg(vid_pid).arg(initial_seconds), 0);        
        QTimer* timer = new QTimer(this);        
        connect(timer, &QTimer::timeout, [this, path_str, vid_pid, timer, initial_seconds]() mutable {
            initial_seconds--;            
            int foundRow = -1;
            for(int i=0; i<m_deviceTable->rowCount(); ++i) {
                if (m_deviceTable->item(i, 3)->text() == QString::fromStdString(path_str)) {
                    foundRow = i;
                    break;}}
            if (initial_seconds > 0) {
                if (foundRow != -1) {
                    QTableWidgetItem* statusItem = m_deviceTable->item(foundRow, 2);
                    statusItem->setText(QString("Wait (%1s)").arg(initial_seconds));}
            } else {
                if (m_portBreaker->enableDeviceByPath(path_str)) {
                    appendLog(QString("Timer Finished: Re-enabled %1").arg(vid_pid), 2);
                } else {
                    appendLog(QString("Timer Error: Failed to re-enable %1").arg(vid_pid), 1);}
                updateDeviceTable();
                timer->stop();
                timer->deleteLater();}});        
        timer->start(1000);}}

void MainWindow::onShowAllDevicesToggled(bool checked) {
    Q_UNUSED(checked)
    updateDeviceTable();}

void MainWindow::updateDeviceTable() {
    std::vector<UsbDevice> all_devices = m_portBreaker->getDevices();
    if (!m_showAllDevices->isChecked()) {
        std::vector<UsbDevice> filtered_devices;
        for (const auto& dev : all_devices) {
            if (dev.vid_pid != "N/A" && dev.vid_pid.rfind("1d6b:", 0) != 0) {
                filtered_devices.push_back(dev);}}
        updateDeviceTable(filtered_devices);
    } else {
        updateDeviceTable(all_devices);}}

void MainWindow::updateDeviceTable(const std::vector<UsbDevice>& devices) {
    m_deviceTable->setRowCount(0);
    m_deviceTable->setRowCount(devices.size());
    for (size_t i = 0; i < devices.size(); ++i) {
        const UsbDevice& dev = devices.at(i);
        m_deviceTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(dev.vid_pid)));
        m_deviceTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(dev.name)));
        m_deviceTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(dev.path)));        
        QString status_path = QString::fromStdString(dev.authorized_path);
        QFile file(status_path);
        QString status = "N/A";
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            status = in.readAll().trimmed() == "1" ? "Aktywny" : "Wylaczony";
            file.close();}
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        if (status == "Aktywny") {
            statusItem->setBackground(QBrush(QColor(0, 100, 0)));
            statusItem->setForeground(QBrush(Qt::white));
        } else if (status == "Wylaczony") {
            statusItem->setBackground(QBrush(QColor(139, 0, 0)));
            statusItem->setForeground(QBrush(Qt::white));
        } else if (status == "N/A") {
            statusItem->setBackground(QBrush(QColor(50, 50, 50)));
            statusItem->setForeground(QBrush(Qt::white));}
        m_deviceTable->setItem(i, 2, statusItem);        
        QString wakeup_status = "N/A";
        if (dev.wakeup_path != "N/A") {
            QFile wakeup_file(QString::fromStdString(dev.wakeup_path));
            if (wakeup_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&wakeup_file);
                wakeup_status = in.readAll().trimmed();
                wakeup_file.close();}}
        QTableWidgetItem* wakeupItem = new QTableWidgetItem(wakeup_status);
        if (wakeup_status == "enabled") {
            wakeupItem->setBackground(QBrush(QColor(30, 80, 150)));
            wakeupItem->setForeground(QBrush(Qt::white));
        } else if (wakeup_status == "disabled") {
            wakeupItem->setBackground(QBrush(QColor(150, 80, 30)));
            wakeupItem->setForeground(QBrush(Qt::white));
        } else if (wakeup_status == "N/A") {
            wakeupItem->setBackground(QBrush(QColor(50, 50, 50)));
            wakeupItem->setForeground(QBrush(Qt::white));}
        m_deviceTable->setItem(i, 4, wakeupItem);}}

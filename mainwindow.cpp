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
#include "PortBreaker.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_portBreaker = new PortBreaker(this);

    if (!isRoot()) {
        QMessageBox::critical(this, "headless", "run framework as root.");
        exit(1);
    }

    QWidget* centralWidget = new QWidget;
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout* timerLayout = new QHBoxLayout();
    m_timerSpinBox = new QSpinBox();
    m_timerSpinBox->setRange(1, 10000000);
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
    m_toggleWakeupButton = new QPushButton("on/off Wake-up (ACPI)");
    m_statusLabel = new QLabel("ready (^_-)");

    centralLayout->addWidget(m_refreshButton);
    centralLayout->addWidget(m_showAllDevices);
    centralLayout->addWidget(m_enableButton);
    centralLayout->addWidget(m_disableButton);
    centralLayout->addLayout(timerLayout);
    centralLayout->addWidget(m_disableWithTimerButton);
    centralLayout->addWidget(m_resetSysfsButton);
    centralLayout->addWidget(m_resetIoctlButton);
    centralLayout->addWidget(m_toggleWakeupButton);
    centralLayout->addWidget(m_resetAllButton);
    centralLayout->addWidget(m_statusLabel);

    setCentralWidget(centralWidget);

    // 🔹 Tabela urządzeń
    m_deviceTable = new QTableWidget;
    m_deviceTable->setColumnCount(5);
    m_deviceTable->setHorizontalHeaderLabels({"VID:PID", "Nazwa Urzadzenia", "Status", "Sysfs", "Wake-up"});
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceTable->setSortingEnabled(true);

    // 🔹 Przywracanie poprzedniego stanu kolumn
    QSettings settings("PortBreakerFramework", "USBManager");
    QByteArray headerState = settings.value("tableHeaderState").toByteArray();
    if (!headerState.isEmpty()) {
        m_deviceTable->horizontalHeader()->restoreState(headerState);
    } else {
        // Domyślne szerokości jeśli brak zapisu
        m_deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        m_deviceTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
        m_deviceTable->setColumnWidth(0, 90);
        m_deviceTable->setColumnWidth(2, 90);
        m_deviceTable->setColumnWidth(4, 90);
    }

    // 🔹 Dock z tabelą
    QDockWidget* deviceDock = new QDockWidget("Lista Urzadzen", this);
    deviceDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    deviceDock->setWidget(m_deviceTable);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock);

    // 🔹 Połączenia sygnałów
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshButton);
    connect(m_enableButton, &QPushButton::clicked, this, &MainWindow::onEnableButton);
    connect(m_disableButton, &QPushButton::clicked, this, &MainWindow::onDisableButton);
    connect(m_disableWithTimerButton, &QPushButton::clicked, this, &MainWindow::onDisableWithTimerButton);
    connect(m_resetSysfsButton, &QPushButton::clicked, this, &MainWindow::onResetSysfsButton);
    connect(m_resetIoctlButton, &QPushButton::clicked, this, &MainWindow::onResetIoctlButton);
    connect(m_resetAllButton, &QPushButton::clicked, this, &MainWindow::onResetAllButton);
    connect(m_toggleWakeupButton, &QPushButton::clicked, this, &MainWindow::onToggleWakeupButton);
    connect(m_showAllDevices, &QCheckBox::toggled, this, &MainWindow::onShowAllDevicesToggled);

    // 🔹 Przywracanie ustawień okna
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    m_timerSpinBox->setValue(settings.value("timerValue", 5).toInt());
    m_showAllDevices->setChecked(settings.value("showAll", false).toBool());

    updateDeviceTable();
}
MainWindow::~MainWindow()
{
    QSettings settings("PortBreakerFramework", "USBManager");

    // 🔹 Zapisz bieżący stan okna i widżetów
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("timerValue", m_timerSpinBox->value());
    settings.setValue("showAll", m_showAllDevices->isChecked());

    // 🔹 Zapisz układ kolumn tabeli
    if (m_deviceTable && m_deviceTable->horizontalHeader()) {
        settings.setValue("tableHeaderState", m_deviceTable->horizontalHeader()->saveState());
    }

    delete m_portBreaker;
}

bool MainWindow::isRoot() {
    return geteuid() == 0;
}

void MainWindow::onRefreshButton()
{
    updateDeviceTable();
    m_statusLabel->setText("Device list refreshed.");
}

void MainWindow::onEnableButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Select a device to enable.");
        return;
    }
    QString sysfs_path = m_deviceTable->item(selectedItems.first()->row(), 3)->text(); 
    
    if (m_portBreaker->enableDeviceByPath(sysfs_path.toStdString())) {
        m_statusLabel->setText(QString("Enabled (path): %1").arg(sysfs_path));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Error: Cannot enable %1.").arg(sysfs_path));
    }
}

void MainWindow::onDisableButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Select a device to disable.");
        return;
    }
    QString sysfs_path = m_deviceTable->item(selectedItems.first()->row(), 3)->text(); 
    
    if (m_portBreaker->disableDeviceByPath(sysfs_path.toStdString())) {
        m_statusLabel->setText(QString("Disabled (path): %1").arg(sysfs_path));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Error: Cannot disable %1.").arg(sysfs_path));
    }
}

void MainWindow::onResetSysfsButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Select a device to reset.");
        return;
    }
    QString sysfs_path = m_deviceTable->item(selectedItems.first()->row(), 3)->text(); 

    if (m_portBreaker->resetDeviceSysfsByPath(sysfs_path.toStdString())) {
        m_statusLabel->setText(QString("Device reset via sysfs: %1").arg(sysfs_path));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Error: Cannot reset device via sysfs: %1.").arg(sysfs_path));
    }
}

void MainWindow::onResetIoctlButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Select a device to reset.");
        return;
    }
    QString vid_pid = m_deviceTable->item(selectedItems.first()->row(), 0)->text();
    if (vid_pid == "N/A") {
        m_statusLabel->setText("Error: ioctl reset requires VID:PID. Select another device.");
        return;
    }
    if (m_portBreaker->resetDeviceIoctl(vid_pid.toStdString())) {
        m_statusLabel->setText(QString("Reset (ioctl): %1").arg(vid_pid));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Error: Cannot reset (ioctl): %1.").arg(vid_pid));
    }
}

void MainWindow::onResetAllButton()
{
    if (m_portBreaker->resetAllDevicesSysfs()) {
        m_statusLabel->setText("All USB host controllers have been reset.");
        updateDeviceTable();
    } else {
        m_statusLabel->setText("Error: Cannot reset all controllers.");
    }
}

void MainWindow::onToggleWakeupButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Select a device to toggle wake-up.");
        return;
    }
    QString sysfs_path = m_deviceTable->item(selectedItems.first()->row(), 3)->text(); 

    if (m_portBreaker->toggleWakeupByPath(sysfs_path.toStdString())) {
        m_statusLabel->setText(QString("Wake-up toggled: %1").arg(sysfs_path));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Error: Cannot toggle wake-up for %1.").arg(sysfs_path));
    }
}

void MainWindow::onDisableWithTimerButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Select a device to disable.");
        return;
    }
    
    QString sysfs_path = m_deviceTable->item(selectedItems.first()->row(), 3)->text(); 
    std::string path_str = sysfs_path.toStdString();
    
    const int delay_sec = m_timerSpinBox->value();
    const int delay_ms = delay_sec * 1000;

    if (m_portBreaker->disableDeviceByPath(path_str)) {
        m_statusLabel->setText(QString("Disabled: %1. Auto re-enable in %2 seconds.").arg(sysfs_path).arg(delay_sec));
        updateDeviceTable();

        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, [this, path_str, sysfs_path, timer]() {
            if (m_portBreaker->enableDeviceByPath(path_str)) {
                m_statusLabel->setText(QString("Device re-enabled after timer: %1").arg(sysfs_path));
            } else {
                m_statusLabel->setText(QString("Error: Cannot re-enable after timer: %1.").arg(sysfs_path));
            }
            updateDeviceTable();
            timer->deleteLater();
        });
        timer->start(delay_ms);
        
    } else {
        m_statusLabel->setText(QString("Error: Cannot disable %1.").arg(sysfs_path));
    }
}

void MainWindow::onShowAllDevicesToggled(bool checked)
{
    Q_UNUSED(checked)
    updateDeviceTable();
}

void MainWindow::updateDeviceTable()
{
    std::vector<UsbDevice> all_devices = m_portBreaker->getDevices();
    
    if (!m_showAllDevices->isChecked()) {
        std::vector<UsbDevice> filtered_devices;
        for (const auto& dev : all_devices) {
            // Filtr: Pokaż tylko urządzenia z VID:PID, które nie są Root Hubami (1d6b:*)
            if (dev.vid_pid != "N/A" && dev.vid_pid.rfind("1d6b:", 0) != 0) {
                filtered_devices.push_back(dev);
            }
        }
        updateDeviceTable(filtered_devices);
    } else {
        updateDeviceTable(all_devices);
    }
}

void MainWindow::updateDeviceTable(const std::vector<UsbDevice>& devices)
{
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
            file.close();
        }
        
        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        if (status == "Aktywny") {
            statusItem->setBackground(QBrush(QColor(0, 100, 0)));
            statusItem->setForeground(QBrush(Qt::white));
        } else if (status == "Wylaczony") {
            statusItem->setBackground(QBrush(QColor(139, 0, 0)));
            statusItem->setForeground(QBrush(Qt::white));
        } else if (status == "N/A") {
            statusItem->setBackground(QBrush(QColor(50, 50, 50)));
            statusItem->setForeground(QBrush(Qt::white));
        }
        m_deviceTable->setItem(i, 2, statusItem);

        QString wakeup_status = "N/A";
        if (dev.wakeup_path != "N/A") {
            QFile wakeup_file(QString::fromStdString(dev.wakeup_path));
            if (wakeup_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&wakeup_file);
                wakeup_status = in.readAll().trimmed();
                wakeup_file.close();
            }
        }
        
        QTableWidgetItem* wakeupItem = new QTableWidgetItem(wakeup_status);
        if (wakeup_status == "enabled") {
            wakeupItem->setBackground(QBrush(QColor(30, 80, 150))); 
            wakeupItem->setForeground(QBrush(Qt::white));
        } else if (wakeup_status == "disabled") {
            wakeupItem->setBackground(QBrush(QColor(150, 80, 30)));
            wakeupItem->setForeground(QBrush(Qt::white));
        } else if (wakeup_status == "N/A") {
             wakeupItem->setBackground(QBrush(QColor(50, 50, 50)));
             wakeupItem->setForeground(QBrush(Qt::white));
        }
        m_deviceTable->setItem(i, 4, wakeupItem);
    }
}

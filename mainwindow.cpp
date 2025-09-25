#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <unistd.h>
#include <sys/types.h>
#include "PortBreaker.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_portBreaker = new PortBreaker(this);

    if (!isRoot()) {
        QMessageBox::critical(this, "hazeDev", "run framework as root.");
        exit(1);
    }
    
    QWidget* centralWidget = new QWidget;
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);

    m_refreshButton = new QPushButton("Refresh");
    m_enableButton = new QPushButton("Wlacz");
    m_disableButton = new QPushButton("Wylacz");
    m_resetSysfsButton = new QPushButton("Reset (sysfs)");
    m_resetIoctlButton = new QPushButton("Reset (ioctl)");
    m_resetAllButton = new QPushButton("Reset all ports");
    m_statusLabel = new QLabel("ready (^_-)");
    
    centralLayout->addWidget(m_refreshButton);
    centralLayout->addWidget(m_enableButton);
    centralLayout->addWidget(m_disableButton);
    centralLayout->addWidget(m_resetSysfsButton);
    centralLayout->addWidget(m_resetIoctlButton);
    centralLayout->addWidget(m_resetAllButton);
    centralLayout->addWidget(m_statusLabel);
    
    setCentralWidget(centralWidget); 

    m_deviceTable = new QTableWidget;
    m_deviceTable->setColumnCount(4);
    m_deviceTable->setHorizontalHeaderLabels({"VID:PID", "Nazwa Urzadzenia", "Status", "Sciezka"});

    m_deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);

    // Ustawienie szerokości poszczególnych kolumn
    m_deviceTable->setColumnWidth(0, 90);
    m_deviceTable->setColumnWidth(2, 90);
    m_deviceTable->setColumnWidth(3, 200);

    // Ustawienie zachowania wyboru w tabeli (wybieranie calych wierszy)
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Zapobieganie edytowaniu komorek
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QDockWidget* deviceDock = new QDockWidget("Lista Urzadzen", this);
    deviceDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    deviceDock->setWidget(m_deviceTable);
    addDockWidget(Qt::LeftDockWidgetArea, deviceDock);
    
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshButton);
    connect(m_enableButton, &QPushButton::clicked, this, &MainWindow::onEnableButton);
    connect(m_disableButton, &QPushButton::clicked, this, &MainWindow::onDisableButton);
    connect(m_resetSysfsButton, &QPushButton::clicked, this, &MainWindow::onResetSysfsButton);
    connect(m_resetIoctlButton, &QPushButton::clicked, this, &MainWindow::onResetIoctlButton);
    connect(m_resetAllButton, &QPushButton::clicked, this, &MainWindow::onResetAllButton);
    
    updateDeviceTable();
}

MainWindow::~MainWindow()
{
    delete m_portBreaker;
}

bool MainWindow::isRoot() {
    return geteuid() == 0;
}

void MainWindow::onRefreshButton()
{
    updateDeviceTable();
    m_statusLabel->setText("Lista urzadzen odswiezona.");
}

// Slot wywolywany po kliknieciu przycisku "Wlacz"
void MainWindow::onEnableButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Wybierz urzadzenie do wlaczenia.");
        return;
    }
    QString vid_pid = selectedItems.first()->text();
    if (m_portBreaker->enableDevice(vid_pid.toStdString())) {
        m_statusLabel->setText(QString("Wlaczono urzadzenie: %1").arg(vid_pid));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Blad: Nie mozna wlaczyc urzadzenia: %1. Wymagane uprawnienia administratora.").arg(vid_pid));
    }
}

void MainWindow::onDisableButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Wybierz urzadzenie do wylaczenia.");
        return;
    }
    QString vid_pid = selectedItems.first()->text();
    if (m_portBreaker->disableDevice(vid_pid.toStdString())) {
        m_statusLabel->setText(QString("Wylaczono urzadzenie: %1").arg(vid_pid));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Blad: Nie mozna wylaczyc urzadzenia: %1. Wymagane uprawnienia administratora.").arg(vid_pid));
    }
}

// Slot wywolywany po kliknieciu "Odłącz"
void MainWindow::onResetSysfsButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Wybierz urzadzenie do zresetowania.");
        return;
    }
    QString vid_pid = selectedItems.first()->text();
    if (m_portBreaker->resetDeviceSysfs(vid_pid.toStdString())) { // Wywolanie resetu za pomoca sysfs
        m_statusLabel->setText(QString("Zresetowano urzadzenie: %1 (sysfs)").arg(vid_pid));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Blad: Nie mozna zresetowac urzadzenia: %1 (sysfs). Wymagane uprawnienia administratora.").arg(vid_pid));
    }
}

// Slot wywolywany po kliknieciu "Podłącz"
void MainWindow::onResetIoctlButton()
{
    QList<QTableWidgetItem*> selectedItems = m_deviceTable->selectedItems();
    if (selectedItems.isEmpty()) {
        m_statusLabel->setText("Wybierz urzadzenie do zresetowania.");
        return;
    }
    QString vid_pid = selectedItems.first()->text();
    if (m_portBreaker->resetDeviceIoctl(vid_pid.toStdString())) { // Wywolanie resetu za pomoca ioctl
        m_statusLabel->setText(QString("Zresetowano urzadzenie: %1 (ioctl)").arg(vid_pid));
        updateDeviceTable();
    } else {
        m_statusLabel->setText(QString("Blad: Nie mozna zresetowac urzadzenia: %1 (ioctl). Wymagane uprawnienia administratora.").arg(vid_pid));
    }
}

// Slot wywolywany po kliknieciu przycisku "Zresetuj Wszystkie Porty"
void MainWindow::onResetAllButton()
{
    if (m_portBreaker->resetAllDevicesSysfs()) { // Wywolanie resetu wszystkich urzadzen
        m_statusLabel->setText("Zresetowano wszystkie urzadzenia.");
        updateDeviceTable();
    } else {
        m_statusLabel->setText("Blad: Nie mozna zresetowac wszystkich urzadzen. Wymagane uprawnienia administratora.");
    }
}

// Funkcja odpowiedzialna za odswiezanie zawartosci tabeli urzadzen
void MainWindow::updateDeviceTable()
{
    m_deviceTable->setRowCount(0); // Wyczyszczenie tabeli
    std::vector<UsbDevice> devices = m_portBreaker->getDevices(); // Pobranie listy urzadzen z klasy PortBreaker
    m_deviceTable->setRowCount(devices.size()); // Ustawienie liczby wierszy
    for (size_t i = 0; i < devices.size(); ++i) { // Iteracja przez liste urzadzen
        const UsbDevice& dev = devices.at(i); // Pobranie pojedynczego urzadzenia
        m_deviceTable->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(dev.vid_pid))); // Ustawienie kolumny VID:PID
        m_deviceTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(dev.name))); // Ustawienie nazwy urzadzenia
        
        QString status_path = QString::fromStdString(dev.authorized_path);
        QFile file(status_path);
        QString status = "Nieznany";
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            status = in.readAll().trimmed() == "1" ? "Aktywny" : "Wylaczony"; // Odczyt statusu z pliku sysfs
            file.close();
        }
        m_deviceTable->setItem(i, 2, new QTableWidgetItem(status)); // Ustawienie statusu
        m_deviceTable->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(dev.path))); // Ustawienie sciezki
    }
}

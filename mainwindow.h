#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QDockWidget>
#include <QTimer>
#include <QSpinBox>
#include <QCheckBox>
#include <QTextEdit>
#include "PortBreaker.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRefreshButton();
    void onEnableButton();
    void onDisableButton();
    void onDisableWithTimerButton();
    void onResetSysfsButton();
    void onResetIoctlButton();
    void onResetAllButton();
    void onShowAllDevicesToggled(bool checked);
    void onToggleWakeupButton();
    void onWatchdogToggled(bool checked);
    void appendLog(QString msg, int level);
    void onWatchdogTriggered(QString vid_pid);

private:
    PortBreaker* m_portBreaker;
    QTableWidget* m_deviceTable;    
    QPushButton* m_refreshButton;
    QPushButton* m_enableButton;
    QPushButton* m_disableButton;
    QPushButton* m_disableWithTimerButton;
    QPushButton* m_resetSysfsButton;
    QPushButton* m_resetIoctlButton;
    QPushButton* m_resetAllButton;
    QPushButton* m_toggleWakeupButton;    
    QSpinBox* m_timerSpinBox;
    QCheckBox* m_showAllDevices;    
    QCheckBox* m_watchdogCheckBox;
    QDockWidget* m_logDock;
    QTextEdit* m_logConsole;
    void updateDeviceTable();
    void updateDeviceTable(const std::vector<UsbDevice>& devices);
    bool isRoot();
};

#endif

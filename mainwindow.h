#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QDockWidget>
#include "PortBreaker.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRefreshButton();
    void onEnableButton();
    void onDisableButton();
    void onResetSysfsButton();
    void onResetIoctlButton();
    void onResetAllButton();

private:
    PortBreaker* m_portBreaker;
    QTableWidget* m_deviceTable;
    QPushButton* m_refreshButton;
    QPushButton* m_enableButton;
    QPushButton* m_disableButton;
    QPushButton* m_resetSysfsButton;
    QPushButton* m_resetIoctlButton;
    QPushButton* m_resetAllButton;
    QLabel* m_statusLabel;
    
    void updateDeviceTable();
    bool isRoot();
};

#endif // MAINWINDOW_H

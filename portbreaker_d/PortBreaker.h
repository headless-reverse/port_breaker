#ifndef PORTBREAKER_H
#define PORTBREAKER_H

#include <string>
#include <vector>
#include <map>
#include <QObject>
#include <QTimer>
#include <QDebug>
#include <QDateTime>

const std::string SYSFS_USB_DEVICES = "/sys/bus/usb/devices/";
const std::string DEV_USB_BUS = "/dev/bus/usb/";
const std::vector<std::string> USB_IDS_PATHS = {
    "/usr/share/hwdata/usb.ids",
    "/usr/share/misc/usb.ids"
};

struct UsbDevice {
    std::string path;
    std::string authorized_path;
    std::string vid_pid;
    std::string name;
    std::string dev_path;
    std::string wakeup_path;
    long long reset_timestamp_ms = 0;
    int reset_duration_s = 0;
};

class PortBreaker : public QObject {
    Q_OBJECT

public:
    explicit PortBreaker(QObject *parent = nullptr);    
    std::vector<UsbDevice> getDevices();
    bool enableDeviceByPath(const std::string& sysfs_path);
    bool disableDeviceByPath(const std::string& sysfs_path);
    bool disableDeviceWithTimer(const std::string& sysfs_path, int duration_s);
    bool resetDeviceSysfsByPath(const std::string& sysfs_path);
    bool resetDeviceIoctl(const std::string& vid_pid);
    bool resetAllDevicesSysfs();
    bool toggleWakeupByPath(const std::string& sysfs_path);    
    // Watchdog API
    void startWatchdog(const std::string& vid_pid, int checkIntervalMs);
    void stopWatchdog();
    void setFilterMode(bool enabled);
    bool isFilterModeEnabled() const { return m_filterAll; }

signals:
    void logMessage(QString msg);
    void watchdogTriggered(QString vid_pid);
    
private slots:
    void onWatchdogTimeout();
    void onDynamicTimerTimeout();

private:
    std::map<std::string, std::string> m_usbNames;
    QTimer* m_watchdogTimer;
    QTimer* m_dynamicTimer;
    std::map<std::string, UsbDevice> m_deviceCache; 
    std::string m_watchedVidPid;
    bool m_filterAll = false;
    std::string readSysfsFile(const std::string& file_path);
    bool writeSysfsFile(const std::string& file_path, const std::string& value);
    std::map<std::string, std::string> loadUsbNames();
    std::string findDevPathByVidPid(const std::string& vid_pid);
    bool deviceExists(const std::string& vid_pid);
    std::vector<UsbDevice> getAllDevices(); 
};

#endif

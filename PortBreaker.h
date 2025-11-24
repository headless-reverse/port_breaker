#ifndef PORTBREAKER_H
#define PORTBREAKER_H

#include <string>
#include <vector>
#include <map>
#include <QObject>
#include <QDebug>
#include <QThread>
#include <QTimer>

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
};

class PortBreaker : public QObject {
    Q_OBJECT

public:
    explicit PortBreaker(QObject *parent = nullptr);    
    std::vector<UsbDevice> getDevices();    
    bool enableDeviceByPath(const std::string& sysfs_path);
    bool disableDeviceByPath(const std::string& sysfs_path);
    bool resetDeviceSysfsByPath(const std::string& sysfs_path);
    bool enableDevice(const std::string& vid_pid);
    bool disableDevice(const std::string& vid_pid);
    bool resetDeviceSysfs(const std::string& vid_pid);
    bool resetDeviceIoctl(const std::string& vid_pid);
    bool resetAllDevicesSysfs();
    bool toggleWakeupByPath(const std::string& sysfs_path);
    void startWatchdog(const std::string& vid_pid, int checkIntervalMs);
    void stopWatchdog();

signals:
    void logMessage(QString msg, int level);
    void watchdogTriggered(QString vid_pid);

private slots:
    void onWatchdogTimeout();

private:
    std::map<std::string, std::string> m_usbNames;    
    QTimer* m_watchdogTimer;
    std::string m_watchedVidPid;
    std::string readSysfsFile(const std::string& file_path);
    bool writeSysfsFile(const std::string& file_path, const std::string& value);
    std::map<std::string, std::string> loadUsbNames();
    bool ioctlResetDevice(const std::string& dev_path);
    std::string findDevPathByVidPid(const std::string& vid_pid);
    bool deviceExists(const std::string& vid_pid);
};

#endif

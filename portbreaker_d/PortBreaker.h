#ifndef PORTBREAKER_H
#define PORTBREAKER_H

#include <string>
#include <vector>
#include <map>
#include <QObject>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QHash>

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

signals:
    void deviceStatusChanged(); 

public:
    explicit PortBreaker(QObject *parent = nullptr);
    std::vector<UsbDevice> getDevices();
    void setFilterMode(bool showAll); 
    bool enableDeviceByPath(const std::string& sysfs_path);
    bool disableDeviceByPath(const std::string& sysfs_path);
    bool resetDeviceSysfsByPath(const std::string& sysfs_path);
    bool resetDeviceIoctl(const std::string& vid_pid);
    bool resetAllDevicesSysfs();
    bool toggleWakeupByPath(const std::string& sysfs_path);
    bool disableDeviceWithTimer(const std::string& sysfs_path, int seconds); 

private slots:
    void reEnableDevice(const QString& sysfsPath);

private:
    std::map<std::string, std::string> m_usbNames;
    bool m_showAllDevices = false; 
    QHash<QString, QTimer*> m_timers; 
    std::string readSysfsFile(const std::string& file_path);
    bool writeSysfsFile(const std::string& file_path, const std::string& value);
    std::map<std::string, std::string> loadUsbNames();
    bool ioctlResetDevice(const std::string& dev_path);
    std::string findDevPathByVidPid(const std::string& vid_pid);
    bool isFakeSysDevice(const std::string& device_path);
};

#endif // PORTBREAKER_H

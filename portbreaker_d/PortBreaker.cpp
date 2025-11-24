#include "PortBreaker.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <QDir>
#include <QThread> 
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <QTimer>
#include <QDateTime>

PortBreaker::PortBreaker(QObject *parent) : QObject(parent) {
    m_usbNames = loadUsbNames();
    m_watchdogTimer = new QTimer(this);
    connect(m_watchdogTimer, &QTimer::timeout, this, &PortBreaker::onWatchdogTimeout);
    m_dynamicTimer = new QTimer(this);
    connect(m_dynamicTimer, &QTimer::timeout, this, &PortBreaker::onDynamicTimerTimeout);
    m_dynamicTimer->start(1000);
    emit logMessage("PortBreaker initialized.");}

void PortBreaker::setFilterMode(bool enabled) {
    m_filterAll = enabled;
    emit logMessage(QString("Filter mode set to: %1").arg(enabled ? "true" : "false"));}

void PortBreaker::startWatchdog(const std::string& vid_pid, int checkIntervalMs) {
    m_watchedVidPid = vid_pid;
    if (m_watchdogTimer->isActive()) m_watchdogTimer->stop();
    m_watchdogTimer->start(checkIntervalMs);
    emit logMessage(QString("Watchdog: STARTED monitoring %1").arg(QString::fromStdString(vid_pid)));}

void PortBreaker::stopWatchdog() {
    if (m_watchdogTimer->isActive()) {
        m_watchdogTimer->stop();
        emit logMessage("Watchdog: STOPPED");}
    m_watchedVidPid = "";}

void PortBreaker::onWatchdogTimeout() {
    if (m_watchedVidPid.empty()) return;
    if (!deviceExists(m_watchedVidPid)) {
        emit logMessage(QString("Watchdog ALERT: Device %1 lost!").arg(QString::fromStdString(m_watchedVidPid)));
        emit watchdogTriggered(QString::fromStdString(m_watchedVidPid));}}

bool PortBreaker::deviceExists(const std::string& vid_pid) {
    auto devices = getAllDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid) return true;}
    return false;}

bool PortBreaker::disableDeviceWithTimer(const std::string& sysfs_path, int duration_s) {
    if (duration_s <= 0) return false;
    auto it = m_deviceCache.find(sysfs_path);
    if (disableDeviceByPath(sysfs_path)) {
        if (it != m_deviceCache.end()) {
            it->second.reset_timestamp_ms = QDateTime::currentMSecsSinceEpoch();
            it->second.reset_duration_s = duration_s;
            emit logMessage(QString("Device %1 disabled. Automatic re-enable in %2 seconds.").arg(QString::fromStdString(it->second.vid_pid)).arg(duration_s));
        } else {
             emit logMessage(QString("Device %1 disabled by Sysfs. Timer not set (not found in cache).").arg(QString::fromStdString(sysfs_path)));}
        return true;}
    return false;}

void PortBreaker::onDynamicTimerTimeout() {
    long long currentTime = QDateTime::currentMSecsSinceEpoch();
    std::vector<std::string> devicesToEnable;
    for (auto& pair : m_deviceCache) {
        UsbDevice& dev = pair.second;
        if (dev.reset_duration_s > 0) {
            long long expiryTime = dev.reset_timestamp_ms + (long long)dev.reset_duration_s * 1000;
            if (currentTime >= expiryTime) {
                devicesToEnable.push_back(dev.path);}}}
    for (const auto& path : devicesToEnable) {
        if (enableDeviceByPath(path)) {
            auto it = m_deviceCache.find(path);
            if (it != m_deviceCache.end()) {
                emit logMessage(QString("Dynamic Timer: Device %1 re-enabled automatically.").arg(QString::fromStdString(it->second.vid_pid)));}}}}

std::string PortBreaker::readSysfsFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) return "";    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    content.erase(std::remove_if(content.begin(), content.end(), [](unsigned char c){
        return std::isspace(c) || c == '\n' || c == '\r';
    }), content.end());
    return content;}

bool PortBreaker::writeSysfsFile(const std::string& file_path, const std::string& value) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        emit logMessage(QString("Error: Permission denied writing to %1, errno: %2").arg(QString::fromStdString(file_path)).arg(strerror(errno)));
        return false;}
    file << value;
    file.close();
    return true;}

std::map<std::string, std::string> PortBreaker::loadUsbNames() {
    std::map<std::string, std::string> usb_names;
    std::string ids_path;
    for (const auto& path : USB_IDS_PATHS) {
        if (access(path.c_str(), R_OK) != -1) {
            ids_path = path;
            break;}}
    if (ids_path.empty()) {
        qDebug() << "[!] Warning: usb.ids file not found. Names will not be displayed.";
        return usb_names;}
    std::ifstream ids_file(ids_path);
    if (!ids_file.is_open()) {
        qDebug() << "[!] Error: Cannot open file " << QString::fromStdString(ids_path) << ".";
        return usb_names;}
    std::string line;
    std::string current_vendor_id;
    std::string current_vendor_name;
    while (std::getline(ids_file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line[0] != '\t' && line.length() >= 4) {
            current_vendor_id = line.substr(0, 4);
            current_vendor_name = line.substr(5);
        } else if (line[0] == '\t' && line.length() >= 6 && !current_vendor_id.empty()) {
            std::string product_id = line.substr(1, 4);
            std::string product_name = line.substr(6);
            std::string full_id = current_vendor_id + ":" + product_id;
            usb_names[full_id] = current_vendor_name + " " + product_name;}}
    return usb_names;}

std::vector<UsbDevice> PortBreaker::getAllDevices() {
    QDir devices_dir(QString::fromStdString(SYSFS_USB_DEVICES));
    std::map<std::string, UsbDevice> current_scan_results;
    for (const QString& entry_q : devices_dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        std::string entry = entry_q.toStdString();
        std::string device_path = SYSFS_USB_DEVICES + entry;
        std::string authorized_path = device_path + "/authorized";
        if (access(authorized_path.c_str(), F_OK) == 0) {
            UsbDevice dev;
            dev.path = device_path;
            dev.authorized_path = authorized_path;
            std::string vid = readSysfsFile(device_path + "/idVendor");
            std::string pid = readSysfsFile(device_path + "/idProduct");
            if (!vid.empty() && !pid.empty()) {
                dev.vid_pid = vid + ":" + pid;
                auto it = m_usbNames.find(dev.vid_pid);
                dev.name = (it != m_usbNames.end()) ? it->second : readSysfsFile(device_path + "/manufacturer") + " " + readSysfsFile(device_path + "/product");
                dev.name.erase(std::remove(dev.name.begin(), dev.name.end(), '\t'), dev.name.end());
            } else if (entry.rfind("usb", 0) == 0) {
                dev.vid_pid = "N/A";
                dev.name = "Linux Foundation " + entry.substr(3) + ".0 root hub";
            } else {
                dev.vid_pid = "N/A";
                dev.name = "USB Hub " + entry;}
            std::string bus_num_str = readSysfsFile(device_path + "/busnum");
            std::string dev_num_str = readSysfsFile(device_path + "/devnum");
            if (!bus_num_str.empty() && !dev_num_str.empty()) {
                std::string bus_padded = bus_num_str.length() < 3 ? std::string(3 - bus_num_str.length(), '0') + bus_num_str : bus_num_str;
                std::string dev_padded = dev_num_str.length() < 3 ? std::string(3 - dev_num_str.length(), '0') + dev_num_str : dev_num_str;
                dev.dev_path = DEV_USB_BUS + bus_padded + "/" + dev_padded;
            } else {
                dev.dev_path = "";}
            std::string wakeup_path = device_path + "/power/wakeup";
            dev.wakeup_path = (access(wakeup_path.c_str(), F_OK) != -1) ? wakeup_path : "N/A";
            auto cache_it = m_deviceCache.find(device_path);
            if (cache_it != m_deviceCache.end()) {
                dev.reset_timestamp_ms = cache_it->second.reset_timestamp_ms;
                dev.reset_duration_s = cache_it->second.reset_duration_s;}
            current_scan_results[dev.path] = dev;}}
    m_deviceCache = std::move(current_scan_results);
    std::vector<UsbDevice> devices;
    for (const auto& pair : m_deviceCache) {
        devices.push_back(pair.second);}
    return devices;}

std::vector<UsbDevice> PortBreaker::getDevices() {
    std::vector<UsbDevice> all_devices = getAllDevices();
    if (m_filterAll) {
        return all_devices;}
    std::vector<UsbDevice> filtered_devices;
    for (const auto& dev : all_devices) {
        // Filter out 'N/A' devices and root hubs (vid_pid starting with "1d6b:")
        if (dev.vid_pid != "N/A" && dev.vid_pid.rfind("1d6b:", 0) != 0) {
            filtered_devices.push_back(dev);}}
    return filtered_devices;}

bool PortBreaker::enableDeviceByPath(const std::string& sysfs_path) {
    std::string authorized_path = sysfs_path + "/authorized";
    bool res = writeSysfsFile(authorized_path, "1");
    if (res) {
        auto it = m_deviceCache.find(sysfs_path);
        if (it != m_deviceCache.end()) {
            it->second.reset_timestamp_ms = 0;
            it->second.reset_duration_s = 0;}}
    emit logMessage(QString(res ? "Enabled device: %1" : "Failed to enable device: %1").arg(QString::fromStdString(sysfs_path)));
    return res;}

bool PortBreaker::disableDeviceByPath(const std::string& sysfs_path) {
    std::string authorized_path = sysfs_path + "/authorized";
    bool res = writeSysfsFile(authorized_path, "0");
    emit logMessage(QString(res ? "Disabled device: %1" : "Failed to disable device: %1").arg(QString::fromStdString(sysfs_path)));
    return res;}

bool PortBreaker::resetDeviceSysfsByPath(const std::string& sysfs_path) {
    std::string authorized_path = sysfs_path + "/authorized";
    if (writeSysfsFile(authorized_path, "0")) {
        QThread::msleep(100); 
        bool res = writeSysfsFile(authorized_path, "1");
        auto it = m_deviceCache.find(sysfs_path);
        if (it != m_deviceCache.end()) {
            it->second.reset_timestamp_ms = 0;
            it->second.reset_duration_s = 0;}
        emit logMessage(QString(res ? "Sysfs Reset OK: %1" : "Sysfs Reset Failed: %1").arg(QString::fromStdString(sysfs_path)));
        return res;}
    return false;}

bool PortBreaker::toggleWakeupByPath(const std::string& sysfs_path) {
    std::string wakeup_path = sysfs_path + "/power/wakeup";
    if (access(wakeup_path.c_str(), F_OK) == -1) {
        emit logMessage(QString("Error: File power/wakeup does not exist for %1").arg(QString::fromStdString(sysfs_path)));
        return false;}
    std::string current_state = readSysfsFile(wakeup_path);
    std::string new_state;
    if (current_state.find("enabled") != std::string::npos) {
        new_state = "disabled";
    } else if (current_state.find("disabled") != std::string::npos) {
        new_state = "enabled";
    } else {
        emit logMessage(QString("Error: Unknown wakeup state for %1: %2").arg(QString::fromStdString(sysfs_path)).arg(QString::fromStdString(current_state)));
        return false;}
    bool res = writeSysfsFile(wakeup_path, new_state);
    emit logMessage(QString("Wakeup toggled to: %1 for %2").arg(QString::fromStdString(new_state)).arg(QString::fromStdString(sysfs_path)));
    return res;}

std::string PortBreaker::findDevPathByVidPid(const std::string& vid_pid) {
    std::vector<UsbDevice> devices = getAllDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid && !dev.dev_path.empty()) {
            return dev.dev_path;}}
    return "";}

bool PortBreaker::resetDeviceIoctl(const std::string& vid_pid) {
    std::string dev_path = findDevPathByVidPid(vid_pid);
    if (dev_path.empty()) {
        emit logMessage(QString("Error: Could not find device path for ioctl reset of %1").arg(QString::fromStdString(vid_pid)));
        return false;}
    int fd = open(dev_path.c_str(), O_WRONLY);
    if (fd < 0) {
        emit logMessage(QString("Error: Cannot open device file %1 for ioctl reset, errno: %2").arg(QString::fromStdString(dev_path)).arg(strerror(errno)));
        return false;}
    bool success = true;
    if (ioctl(fd, USBDEVFS_RESET, 0) < 0) {
        emit logMessage(QString("Error: ioctl error: Unable to reset device %1, errno: %2").arg(QString::fromStdString(vid_pid)).arg(strerror(errno)));
        success = false;}
    close(fd);    
    if (success) {
        emit logMessage(QString("ioctl reset successful for %1").arg(QString::fromStdString(vid_pid)));
        QThread::msleep(200);}
    return success;}

bool PortBreaker::resetAllDevicesSysfs() {
    emit logMessage("Resetting ALL controllers...");
    std::vector<std::string> hostControllers;
    QDir devices_dir(QString::fromStdString(SYSFS_USB_DEVICES));    
    for (const QString& entry_q : devices_dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        std::string name = entry_q.toStdString();
        if (name.rfind("usb", 0) == 0 && name.length() > 3 && std::all_of(name.begin() + 3, name.end(), ::isdigit)) {
            hostControllers.push_back(name);}}
    if (hostControllers.empty()) {
        emit logMessage("Error: No USB host controllers found");
        return false;}
    bool all_disabled = true;
    for (const auto& controller : hostControllers) {
        std::string authorized_path = SYSFS_USB_DEVICES + controller + "/authorized";
        if (access(authorized_path.c_str(), F_OK) == 0) {
            if (!writeSysfsFile(authorized_path, "0")) {
                all_disabled = false;}}}
    QThread::msleep(100);
    bool all_enabled = true;
    for (const auto& controller : hostControllers) {
        std::string authorized_path = SYSFS_USB_DEVICES + controller + "/authorized";
        if (access(authorized_path.c_str(), F_OK) == 0) {
            if (!writeSysfsFile(authorized_path, "1")) {
                all_enabled = false;}}}
    emit logMessage("All controllers reset cycle done");
    return all_disabled && all_enabled;}

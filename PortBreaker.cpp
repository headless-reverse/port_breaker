#include "PortBreaker.h"
#include <iostream>
#include <fstream>
#include <set>
#include <sstream>
#include <cstdio>
#include <memory>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <stdexcept>
#include <QThread>
#include <QDebug>
#include <QDir>

PortBreaker::PortBreaker(QObject *parent) : QObject(parent) {
    m_usbNames = loadUsbNames();    
    m_watchdogTimer = new QTimer(this);
    connect(m_watchdogTimer, &QTimer::timeout, this, &PortBreaker::onWatchdogTimeout);}

void PortBreaker::startWatchdog(const std::string& vid_pid, int checkIntervalMs) {
    m_watchedVidPid = vid_pid;
    if (m_watchdogTimer->isActive()) m_watchdogTimer->stop();
    m_watchdogTimer->start(checkIntervalMs);
    emit logMessage(QString("Watchdog: STARTED monitoring %1").arg(QString::fromStdString(vid_pid)), 0);}

void PortBreaker::stopWatchdog() {
    if (m_watchdogTimer->isActive()) {
        m_watchdogTimer->stop();
        emit logMessage("Watchdog: STOPPED", 0);}}

void PortBreaker::onWatchdogTimeout() {
    if (m_watchedVidPid.empty()) return;
    if (!deviceExists(m_watchedVidPid)) {
        emit logMessage(QString("Watchdog ALERT: Device %1 lost!").arg(QString::fromStdString(m_watchedVidPid)), 1);
        emit watchdogTriggered(QString::fromStdString(m_watchedVidPid));}}

bool PortBreaker::deviceExists(const std::string& vid_pid) {
    auto devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid) return true;}
    return false;}

std::string PortBreaker::readSysfsFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "";}
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    content.erase(std::remove_if(content.begin(), content.end(), [](unsigned char c){
        return std::isspace(c) || c == '\n' || c == '\r';
    }), content.end());
    return content;}

bool PortBreaker::writeSysfsFile(const std::string& file_path, const std::string& value) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        emit logMessage(QString("Error: Permission denied writing to %1").arg(QString::fromStdString(file_path)), 1);
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
        qDebug() << "[!] Ostrzezenie: błąd pliku usb.ids. Nazwy nie beda wyswietlane.";
        return usb_names;}
    std::ifstream ids_file(ids_path);
    if (!ids_file.is_open()) {
        qDebug() << "[!] Blad: Nie mozna otworzyc pliku " << QString::fromStdString(ids_path) << ".";
        return usb_names;}
    std::string line;
    std::string current_vendor_id;
    std::string current_vendor_name;
    while (std::getline(ids_file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;}
        if (line[0] != '\t' && line.length() >= 4) {
            current_vendor_id = line.substr(0, 4);
            current_vendor_name = line.substr(5);
        } else if (line[0] == '\t' && line.length() >= 6 && !current_vendor_id.empty()) {
            std::string product_id = line.substr(1, 4);
            std::string product_name = line.substr(6);
            std::string full_id = current_vendor_id + ":" + product_id;
            usb_names[full_id] = current_vendor_name + " " + product_name;}}
    return usb_names;}

std::vector<UsbDevice> PortBreaker::getDevices() {
    std::vector<UsbDevice> devices;
    QDir devices_dir(QString::fromStdString(SYSFS_USB_DEVICES));
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
                if (it != m_usbNames.end()) {
                    dev.name = it->second;
                } else {
                    std::string manufacturer_name = readSysfsFile(device_path + "/manufacturer");
                    std::string product_name = readSysfsFile(device_path + "/product");
                    dev.name = manufacturer_name + " " + product_name;
                    dev.name.erase(std::remove(dev.name.begin(), dev.name.end(), ' '), dev.name.end());}
            } else if (entry.rfind("usb", 0) == 0) {
                dev.vid_pid = "N/A";
                dev.name = "Linux Foundation " + entry.substr(3) + ".0 root hub";
            } else {
                dev.vid_pid = "N/A";
                dev.name = "USB Hub " + entry;}
            std::string bus_num_str = readSysfsFile(device_path + "/busnum");
            std::string dev_num_str = readSysfsFile(device_path + "/devnum");
            if (!bus_num_str.empty() && !dev_num_str.empty()) {
                std::string bus_padded = std::string(3 - bus_num_str.length(), '0') + bus_num_str;
                std::string dev_padded = std::string(3 - dev_num_str.length(), '0') + dev_num_str;
                dev.dev_path = DEV_USB_BUS + bus_padded + "/" + dev_padded;
            } else {
                dev.dev_path = "";}
            std::string wakeup_path = device_path + "/power/wakeup";
            if (access(wakeup_path.c_str(), F_OK) != -1) {
                dev.wakeup_path = wakeup_path;
            } else {
                dev.wakeup_path = "N/A";}
            devices.push_back(dev);}}
    return devices;}

bool PortBreaker::enableDeviceByPath(const std::string& sysfs_path) {
    std::string authorized_path = sysfs_path + "/authorized";
    bool res = writeSysfsFile(authorized_path, "1");
    emit logMessage(res ? "Enabled device" : "Failed to enable", res ? 2 : 1);
    return res;}

bool PortBreaker::disableDeviceByPath(const std::string& sysfs_path) {
    std::string authorized_path = sysfs_path + "/authorized";
    bool res = writeSysfsFile(authorized_path, "0");
    emit logMessage(res ? "Disabled device" : "Failed to disable", res ? 2 : 1);
    return res;}

bool PortBreaker::resetDeviceSysfsByPath(const std::string& sysfs_path) {
    std::string authorized_path = sysfs_path + "/authorized";
    if (writeSysfsFile(authorized_path, "0")) {
        QThread::sleep(1);
        bool res = writeSysfsFile(authorized_path, "1");
        emit logMessage(res ? "Sysfs Reset OK" : "Sysfs Reset Failed", res ? 2 : 1);
        return res;}
    return false;}

bool PortBreaker::toggleWakeupByPath(const std::string& sysfs_path) {
    std::string wakeup_path = sysfs_path + "/power/wakeup";
    if (access(wakeup_path.c_str(), F_OK) == -1) {
        emit logMessage("File power/wakeup does not exist", 1);
        return false;}
    std::string current_state = readSysfsFile(wakeup_path);
    std::string new_state;
    if (current_state.find("enabled") != std::string::npos) {
        new_state = "disabled";
    } else if (current_state.find("disabled") != std::string::npos) {
        new_state = "enabled";
    } else {
        emit logMessage("Unknown wakeup state", 1);
        return false;}
    bool res = writeSysfsFile(wakeup_path, new_state);
    emit logMessage(QString("Wakeup toggled to: %1").arg(QString::fromStdString(new_state)), res ? 2 : 1);
    return res;}

std::string PortBreaker::findDevPathByVidPid(const std::string& vid_pid) {
    std::vector<UsbDevice> devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid && !dev.dev_path.empty()) {
            return dev.dev_path;}}
    return "";}

bool PortBreaker::ioctlResetDevice(const std::string& dev_path) {
    int fd = open(dev_path.c_str(), O_WRONLY);
    if (fd < 0) {
        emit logMessage(QString("Cannot open file %1").arg(QString::fromStdString(dev_path)), 1);
        return false;}
    bool success = true;
    if (ioctl(fd, USBDEVFS_RESET, 0) < 0) {
        emit logMessage("ioctl error: Unable to reset device", 1);
        success = false;}
    close(fd);
    if (success) {
        emit logMessage("ioctl reset successful", 2);}
    return success;}

bool PortBreaker::resetDeviceIoctl(const std::string& vid_pid) {
    std::string dev_path = findDevPathByVidPid(vid_pid);
    if (!dev_path.empty()) {
        bool success = ioctlResetDevice(dev_path);
        if (success) {
            QThread::sleep(2);}
        return success;}
    emit logMessage("Error: Could not find device path for ioctl reset", 1);
    return false;}

bool PortBreaker::resetAllDevicesSysfs() {
    emit logMessage("Resetting ALL controllers...", 0);
    std::vector<std::string> hostControllers;
    QDir devices_dir(QString::fromStdString(SYSFS_USB_DEVICES));
    for (const QString& entry_q : devices_dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        std::string name = entry_q.toStdString();
        if (name.rfind("usb", 0) == 0 && name.length() > 3 && std::all_of(name.begin() + 3, name.end(), ::isdigit)) {
            hostControllers.push_back(name);}}
    if (hostControllers.empty()) {
        emit logMessage("No USB host controllers found", 1);
        return false;}
    bool all_disabled = true;
    for (const auto& controller : hostControllers) {
        std::string authorized_path = SYSFS_USB_DEVICES + controller + "/authorized";
        if (access(authorized_path.c_str(), F_OK) == 0) {
            if (!writeSysfsFile(authorized_path, "0")) {
                all_disabled = false;}}}
    QThread::sleep(1);
    bool all_enabled = true;
    for (const auto& controller : hostControllers) {
        std::string authorized_path = SYSFS_USB_DEVICES + controller + "/authorized";
        if (access(authorized_path.c_str(), F_OK) == 0) {
            if (!writeSysfsFile(authorized_path, "1")) {
                all_enabled = false;}}}
    emit logMessage("All controllers reset cycle done", 2);
    return all_disabled && all_enabled;}

bool PortBreaker::enableDevice(const std::string& vid_pid) { return false; }
bool PortBreaker::disableDevice(const std::string& vid_pid) { return false; }
bool PortBreaker::resetDeviceSysfs(const std::string& vid_pid) { return false; }

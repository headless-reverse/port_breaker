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

PortBreaker::PortBreaker(QObject *parent) : QObject(parent)
{
    m_usbNames = loadUsbNames();
}

std::string PortBreaker::readSysfsFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    content.erase(std::remove_if(content.begin(), content.end(), [](unsigned char c){ return std::isspace(c); }), content.end());
    return content;
}

bool PortBreaker::writeSysfsFile(const std::string& file_path, const std::string& value) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        qDebug() << "[!] Blad: Nie mozna otworzyc pliku " << QString::fromStdString(file_path) << " do zapisu.";
        return false;
    }
    file << value;
    file.close();
    return true;
}

std::map<std::string, std::string> PortBreaker::loadUsbNames() {
    std::map<std::string, std::string> usb_names;
    std::string ids_path;
    for (const auto& path : USB_IDS_PATHS) {
        if (access(path.c_str(), R_OK) != -1) {
            ids_path = path;
            break;
        }
    }
    if (ids_path.empty()) {
        qDebug() << "[!] Ostrzezenie: Nie znaleziono pliku usb.ids. Nazwy urzadzen nie beda wyswietlane.";
        return usb_names;
    }
    std::ifstream ids_file(ids_path);
    if (!ids_file.is_open()) {
        qDebug() << "[!] Blad: Nie mozna otworzyc pliku " << QString::fromStdString(ids_path) << ".";
        return usb_names;
    }
    std::string line;
    std::string current_vendor_id;
    std::string current_vendor_name;
    while (std::getline(ids_file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line[0] != '\t' && line.length() >= 4) {
            current_vendor_id = line.substr(0, 4);
            current_vendor_name = line.substr(5);
        } else if (line[0] == '\t' && line.length() >= 6 && !current_vendor_id.empty()) {
            std::string product_id = line.substr(1, 4);
            std::string product_name = line.substr(6);
            std::string full_id = current_vendor_id + ":" + product_id;
            usb_names[full_id] = current_vendor_name + " " + product_name;
        }
    }
    return usb_names;
}

std::vector<UsbDevice> PortBreaker::getDevices() {
    std::vector<UsbDevice> devices;
    DIR* dir = opendir(SYSFS_USB_DEVICES.c_str());
    if (!dir) {
        qDebug() << "[!] Blad: Nie mozna otworzyc katalogu " << QString::fromStdString(SYSFS_USB_DEVICES);
        return devices;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        std::string device_path = SYSFS_USB_DEVICES + name;
        std::string authorized_path = device_path + "/authorized";
        std::string vid_path = device_path + "/idVendor";
        std::string pid_path = device_path + "/idProduct";
        std::string manufacturer_path = device_path + "/manufacturer";
        std::string product_path = device_path + "/product";
        struct stat st;
        if (stat(device_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) &&
            access(authorized_path.c_str(), F_OK) != -1) {
            std::string vid = readSysfsFile(vid_path);
            std::string pid = readSysfsFile(pid_path);
            if (!vid.empty() && !pid.empty()) {
                std::string vid_pid = vid + ":" + pid;
                std::string device_name;
                auto it = m_usbNames.find(vid_pid);
                if (it != m_usbNames.end()) {
                    device_name = it->second;
                } else {
                    std::string manufacturer_name = readSysfsFile(manufacturer_path);
                    std::string product_name = readSysfsFile(product_path);
                    if (!manufacturer_name.empty() || !product_name.empty()) {
                        device_name = manufacturer_name + " " + product_name;
                        device_name.erase(std::remove_if(device_name.begin(), device_name.end(), [](unsigned char c){ return std::isspace(c); }), device_name.end());
                    } else {
                        device_name = "Nieznane urzadzenie";
                    }
                }
                std::string bus_num_str = readSysfsFile(device_path + "/busnum");
                std::string dev_num_str = readSysfsFile(device_path + "/devnum");
                std::string dev_path;
                if (!bus_num_str.empty() && !dev_num_str.empty()) {
                    std::string bus_padded = std::string(3 - bus_num_str.length(), '0') + bus_num_str;
                    std::string dev_padded = std::string(3 - dev_num_str.length(), '0') + dev_num_str;
                    dev_path = DEV_USB_BUS + bus_padded + "/" + dev_padded;
                } else {
                    dev_path = "";
                }
                devices.push_back({device_path, authorized_path, vid_pid, device_name, dev_path});
            }
        }
    }
    closedir(dir);
    return devices;
}

bool PortBreaker::ioctlResetDevice(const std::string& dev_path) {
    int fd = open(dev_path.c_str(), O_WRONLY);
    if (fd < 0) {
        qDebug() << "[!] Blad: Nie mozna otworzyc pliku urzadzenia " << QString::fromStdString(dev_path) << ".";
        return false;
    }
    if (ioctl(fd, USBDEVFS_RESET, 0) < 0) {
        qDebug() << "[!] Blad ioctl: Nie mozna zresetowac urzadzenia.";
        close(fd);
        return false;
    }
    qDebug() << "[✓] Zresetowano urzadzenie za pomoca ioctl.";
    close(fd);
    return true;
}

std::string PortBreaker::findDevPathByVidPid(const std::string& vid_pid) {
    std::vector<UsbDevice> devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid) {
            return dev.dev_path;
        }
    }
    return "";
}

bool PortBreaker::enableDevice(const std::string& vid_pid) {
    std::vector<UsbDevice> devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid) {
            return writeSysfsFile(dev.authorized_path, "1");
        }
    }
    return false;
}

bool PortBreaker::disableDevice(const std::string& vid_pid) {
    std::vector<UsbDevice> devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid) {
            return writeSysfsFile(dev.authorized_path, "0");
        }
    }
    return false;
}

bool PortBreaker::resetDeviceSysfs(const std::string& vid_pid) {
    std::vector<UsbDevice> devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.vid_pid == vid_pid) {
            if (writeSysfsFile(dev.authorized_path, "0")) {
                QThread::sleep(1); 
                return writeSysfsFile(dev.authorized_path, "1");
            }
            return false;
        }
    }
    return false;
}

bool PortBreaker::resetDeviceIoctl(const std::string& vid_pid) {
    std::string dev_path = findDevPathByVidPid(vid_pid);
    if (!dev_path.empty()) {
        bool success = ioctlResetDevice(dev_path);
        if (success) {
            QThread::sleep(2);
        }
        return success;
    }
    qDebug() << "[!] Blad: Nie mozna zlokalizowac sciezki urzadzenia do uzycia z ioctl.";
    return false;
}

bool PortBreaker::resetAllDevicesSysfs() {
    std::vector<std::string> hostControllers;
    DIR* dir = opendir(SYSFS_USB_DEVICES.c_str());
    if (!dir) {
        qDebug() << "[!] Blad: Nie mozna otworzyc katalogu " << QString::fromStdString(SYSFS_USB_DEVICES);
        return false;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        // Znajdź tylko główne kontrolery, np. "usb1", "usb2"
        if (name.rfind("usb", 0) == 0 && name.length() > 3 && std::all_of(name.begin() + 3, name.end(), ::isdigit)) {
            hostControllers.push_back(name);
        }
    }
    closedir(dir);

    if (hostControllers.empty()) {
        qDebug() << "[!] Nie znaleziono zadnych kontrolerow hosta USB do zresetowania.";
        return false;
    }

    // Wyłącz wszystkie kontrolery
    for (const auto& controller : hostControllers) {
        std::string authorized_path = SYSFS_USB_DEVICES + controller + "/authorized";
        if (access(authorized_path.c_str(), F_OK) == 0) {
            if (!writeSysfsFile(authorized_path, "0")) {
                qDebug() << "Blad podczas wylaczania kontrolera: " << QString::fromStdString(controller);
            }
        }
    }

    // Opóźnienie, aby system mógł zareagować
    QThread::sleep(1);

    // Włącz wszystkie kontrolery
    for (const auto& controller : hostControllers) {
        std::string authorized_path = SYSFS_USB_DEVICES + controller + "/authorized";
        if (access(authorized_path.c_str(), F_OK) == 0) {
            if (!writeSysfsFile(authorized_path, "1")) {
                qDebug() << "Blad podczas wlaczania kontrolera: " << QString::fromStdString(controller);
            }
        }
    }

    return true;
}

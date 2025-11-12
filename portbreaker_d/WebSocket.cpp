#include "WebSocket.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <unistd.h>

WebSocket::WebSocket(PortBreaker* breaker, quint16 port, QObject *parent)
    : QObject(parent), m_portBreaker(breaker) {
    if (geteuid() != 0) {
        qCritical() << "[!] ERROR: portbreaker_d must be run as root!";
        exit(1);
    }   
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("portbreaker_d WebSocket Server"),
                                             QWebSocketServer::NonSecureMode, this);
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        qDebug() << "[✓] portbreaker_d server listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &WebSocket::onNewConnection);
    } else {
        qCritical() << "[!] ERROR: Could not start portbreaker_d:" << m_pWebSocketServer->errorString();
    }   
    connect(m_portBreaker, &PortBreaker::deviceStatusChanged, 
            this, [this]() {
                qDebug() << "[portbreaker_d] Device status change signal received. Broadcasting update.";
                broadcast(getDevicesJson());
            });
}

WebSocket::~WebSocket() {
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void WebSocket::onNewConnection() {
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
    qDebug() << "[portbreaker_d] New client connection from:" << pSocket->peerAddress().toString();
    connect(pSocket, &QWebSocket::textMessageReceived, this, &WebSocket::processTextMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &WebSocket::socketDisconnected);
    m_clients << pSocket;   
    pSocket->sendTextMessage(QJsonDocument(getDevicesJson()).toJson(QJsonDocument::Compact));
    sendResponse(pSocket, "STATUS", "Connected to portbreaker_d. Client ID " + QString::number((quintptr)pSocket));
}

void WebSocket::socketDisconnected() {
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    qDebug() << "[portbreaker_d] Client disconnected:" << pClient->peerAddress().toString();
    if (pClient) {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}

void WebSocket::sendResponse(QWebSocket* client, const QString& type, const QString& message, bool success) {
    QJsonObject obj;
    obj["type"] = type;
    obj["message"] = message;
    obj["success"] = success;
    client->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QJsonDocument WebSocket::getDevicesJson() {
    QJsonArray devicesArray;
    std::vector<UsbDevice> devices = m_portBreaker->getDevices();   
    for (const auto& dev : devices) {
        QJsonObject devObj;
        devObj["vid_pid"] = QString::fromStdString(dev.vid_pid);
        devObj["name"] = QString::fromStdString(dev.name);
        devObj["sysfs_path"] = QString::fromStdString(dev.path);        
        QString status = "N/A";
        QFile file(QString::fromStdString(dev.authorized_path));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            status = in.readAll().trimmed() == "1" ? "Enabled" : "Disabled";
            file.close();
        }
        devObj["status"] = status;
        QString wakeup = "N/A";
        if (dev.wakeup_path != "N/A") {
            QFile wakeupFile(QString::fromStdString(dev.wakeup_path));
            if (wakeupFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&wakeupFile);
                wakeup = in.readAll().trimmed();
                wakeupFile.close();
            }
        }
        devObj["wakeup"] = wakeup;
        devicesArray.append(devObj);
    }
    QJsonObject mainObj;
    mainObj["type"] = "DEVICES_LIST";
    mainObj["data"] = devicesArray;
    return QJsonDocument(mainObj);
}

void WebSocket::broadcast(const QJsonDocument& doc) {
    for (QWebSocket* client : m_clients) {
        client->sendTextMessage(doc.toJson(QJsonDocument::Compact));
    }
}

void WebSocket::processTextMessage(QString message) {
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        sendResponse(pClient, "ERROR", "Invalid JSON format.");
        return;
    }
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    QString vid_pid = obj["vid_pid"].toString();
    QString sysfs_path = obj["sysfs_path"].toString();
    bool result = false;
    qDebug() << "[portbreaker_d] Received type:" << type << "for client:" << (quintptr)pClient;
    if (type == "GET_DEVICES") {
        pClient->sendTextMessage(QJsonDocument(getDevicesJson()).toJson(QJsonDocument::Compact));
    } 
    else if (type == "SET_FILTER_ALL") {
        m_portBreaker->setFilterMode(true);
        sendResponse(pClient, "ACK", "Mode enabled: Show all devices (including fake_sys).", true);
        broadcast(QJsonDocument(getDevicesJson())); 
    }
    else if (type == "SET_FILTER_DEFAULT") {
        m_portBreaker->setFilterMode(false);
        sendResponse(pClient, "ACK", "Mode enabled: Default filter (hide fake_sys).", true);
        broadcast(QJsonDocument(getDevicesJson())); 
    }
    else if (type == "DISABLE_TIMER" && !sysfs_path.isEmpty() && obj.contains("seconds")) {
        int seconds = obj["seconds"].toInt();
        result = m_portBreaker->disableDeviceWithTimer(sysfs_path.toStdString(), seconds);
        sendResponse(pClient, "ACK", QString("Device disabled with timer for %1s.").arg(seconds), result);
        broadcast(QJsonDocument(getDevicesJson()));
    }
    else if (type == "ENABLE" && !sysfs_path.isEmpty()) {
        result = m_portBreaker->enableDeviceByPath(sysfs_path.toStdString());
        sendResponse(pClient, "ACK", "Device enabled via Sysfs.", result);
        broadcast(QJsonDocument(getDevicesJson()));
    } else if (type == "DISABLE" && !sysfs_path.isEmpty()) {
        result = m_portBreaker->disableDeviceByPath(sysfs_path.toStdString());
        sendResponse(pClient, "ACK", "Device disabled via Sysfs.", result);
        broadcast(QJsonDocument(getDevicesJson()));
    } else if (type == "RESET_SYSFS" && !sysfs_path.isEmpty()) {
        result = m_portBreaker->resetDeviceSysfsByPath(sysfs_path.toStdString());
        sendResponse(pClient, "ACK", "Sysfs reset (disable/enable).", result);
        broadcast(QJsonDocument(getDevicesJson()));
    } else if (type == "RESET_IOCTL" && !vid_pid.isEmpty()) {
        result = m_portBreaker->resetDeviceIoctl(vid_pid.toStdString());
        sendResponse(pClient, "ACK", "IOCTL reset.", result);
        broadcast(QJsonDocument(getDevicesJson()));
    } else if (type == "RESET_ALL") {
        result = m_portBreaker->resetAllDevicesSysfs();
        sendResponse(pClient, "ACK", "Host controllers reset.", result);
        broadcast(QJsonDocument(getDevicesJson()));
    } else if (type == "TOGGLE_WAKEUP" && !sysfs_path.isEmpty()) {
        result = m_portBreaker->toggleWakeupByPath(sysfs_path.toStdString());
        sendResponse(pClient, "ACK", "Wake-up (ACPI) toggled.", result);
        broadcast(QJsonDocument(getDevicesJson()));
    } else {
        sendResponse(pClient, "ERROR", "Unknown command or missing required parameter (sysfs_path/vid_pid/seconds).");
    }
}

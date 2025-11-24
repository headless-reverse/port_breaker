#include "WebSocket.h"
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QHostAddress>

WebSocket::WebSocket(PortBreaker* portBreaker, quint16 port, QObject *parent) 
    : QObject(parent), 
      m_portBreaker(portBreaker) {
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("PortBreaker Daemon"), QWebSocketServer::NonSecureMode, this);
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        qDebug() << "[*] WebSocket Server listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection, this, &WebSocket::onNewConnection);
    } else {
        qCritical() << "[!] Failed to start WebSocket Server:" << m_pWebSocketServer->errorString();}
    connect(m_portBreaker, &PortBreaker::logMessage, this, &WebSocket::portBreakerLog);
    connect(m_portBreaker, &PortBreaker::watchdogTriggered, this, &WebSocket::portBreakerWatchdogTriggered);
    m_periodicUpdateTimer = new QTimer(this);
    connect(m_periodicUpdateTimer, &QTimer::timeout, this, &WebSocket::onPeriodicUpdateTimeout);
    m_periodicUpdateTimer->start(2000);}

WebSocket::~WebSocket() {
    m_pWebSocketServer->close();
    QMutexLocker lock(&m_clientsMutex); 
    qDeleteAll(m_clients);}

void WebSocket::onNewConnection() {
    QWebSocket* pSocket = m_pWebSocketServer->nextPendingConnection();
    qDebug() << "[*] New connection from:" << pSocket->peerAddress().toString();
    QMutexLocker lock(&m_clientsMutex);
    connect(pSocket, &QWebSocket::textMessageReceived, this, &WebSocket::processTextMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &WebSocket::socketDisconnected);
    m_clients.append(pSocket);
    QJsonObject welcome;
    welcome["type"] = "log";
    welcome["message"] = QString("Welcome to portbreaker_d. Server time: %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss"));
    pSocket->sendTextMessage(QJsonDocument(welcome).toJson(QJsonDocument::Compact));
    sendDeviceList(pSocket);}

void WebSocket::socketDisconnected() {
    QWebSocket* pSocket = qobject_cast<QWebSocket*>(sender());
    if (pSocket) {
        QMutexLocker lock(&m_clientsMutex);
        m_clients.removeOne(pSocket);
        qDebug() << "[*] Socket disconnected:" << pSocket->peerAddress().toString();
        pSocket->deleteLater();}}

void WebSocket::processTextMessage(QString message) {
    QWebSocket* pSocket = qobject_cast<QWebSocket*>(sender());
    if (!pSocket) return;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        QJsonObject err;
        err["type"] = "error";
        err["message"] = "Invalid JSON format.";
        pSocket->sendTextMessage(QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;}
    QJsonObject obj = doc.object();
    QString command = obj["command"].toString().trimmed().toLower();
    QString data = obj["data"].toString().trimmed();
    if (command.isEmpty()) {
        QJsonObject err;
        err["type"] = "error";
        err["message"] = "Missing 'command' field.";
        pSocket->sendTextMessage(QJsonDocument(err).toJson(QJsonDocument::Compact));
        return;}
    processCommand(pSocket, command, data);}

void WebSocket::processCommand(QWebSocket* pSocket, const QString& command, const QString& data) {
    bool success = false;
    QString logMsg = "";
    if (command == "get_devices") {
        sendDeviceList(pSocket);
        return;
    } else if (command == "enable") {
        success = m_portBreaker->enableDeviceByPath(data.toStdString());
    } else if (command == "disable") {
        success = m_portBreaker->disableDeviceByPath(data.toStdString());
    } else if (command == "disable_with_timer") {
        QStringList parts = data.split(',');
        if (parts.size() == 2) {
            std::string sysfs_path = parts[0].toStdString();
            int duration_s = parts[1].toInt();
            if (duration_s > 0) {
                success = m_portBreaker->disableDeviceWithTimer(sysfs_path, duration_s);
            } else {
                logMsg = "Timer duration must be greater than 0.";}
        } else {
            logMsg = "Invalid format for disable_with_timer. Expected: 'sysfs_path,duration_s'";}
    } else if (command == "reset_sysfs") {
        success = m_portBreaker->resetDeviceSysfsByPath(data.toStdString());
    } else if (command == "reset_ioctl") {
        success = m_portBreaker->resetDeviceIoctl(data.toStdString()); 
    } else if (command == "reset_all") {
        success = m_portBreaker->resetAllDevicesSysfs();
    } else if (command == "toggle_wakeup") {
        success = m_portBreaker->toggleWakeupByPath(data.toStdString());
    } else if (command == "start_watchdog") {
        QStringList parts = data.split(',');
        if (parts.size() == 2) {
            m_portBreaker->startWatchdog(parts[0].toStdString(), parts[1].toInt());
            success = true;
        } else {
            logMsg = "Invalid format for start_watchdog. Expected: 'vid:pid,interval_ms'";}
    } else if (command == "stop_watchdog") {
        m_portBreaker->stopWatchdog();
        success = true;
    } else if (command == "toggle_filter") {
        bool newState = !m_portBreaker->isFilterModeEnabled();
        m_portBreaker->setFilterMode(newState);
        success = true;
    } else {
        logMsg = QString("Unknown command: %1").arg(command);}
    QJsonObject response;
    response["type"] = success ? "command_ok" : "command_error";
    response["command"] = command;
    if (!logMsg.isEmpty()) response["message"] = logMsg;
    pSocket->sendTextMessage(QJsonDocument(response).toJson(QJsonDocument::Compact));
    if (success && command != "get_devices") {
        sendDeviceList(pSocket); }}

void WebSocket::sendDeviceList(QWebSocket* pSocket) {
    std::vector<UsbDevice> devices = m_portBreaker->getDevices();
    QJsonArray devicesArray;
    for (const auto& dev : devices) {
        devicesArray.append(singleDeviceToJson(dev));}
    QJsonObject obj;
    obj["type"] = "device_list";
    obj["filter_mode"] = m_portBreaker->isFilterModeEnabled();
    obj["devices"] = devicesArray;
    pSocket->sendTextMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));}

QJsonObject WebSocket::singleDeviceToJson(const UsbDevice& device) {
    QJsonObject obj;
    obj["vid_pid"] = QString::fromStdString(device.vid_pid);
    obj["name"] = QString::fromStdString(device.name);
    obj["path"] = QString::fromStdString(device.path);
    obj["status"] = getDeviceStatus(device);
    obj["wakeup"] = getWakeupStatus(device);
    obj["dev_path"] = QString::fromStdString(device.dev_path);
    obj["reset_ts"] = device.reset_timestamp_ms;
    obj["reset_dur"] = device.reset_duration_s;
    return obj;}

QString WebSocket::getDeviceStatus(const UsbDevice& device) {
    QFile file(QString::fromStdString(device.authorized_path));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString status = in.readAll().trimmed();
        file.close();
        return status == "1" ? "Aktywny" : "Wylaczony";}
    return "N/A";}

QString WebSocket::getWakeupStatus(const UsbDevice& device) {
    if (device.wakeup_path == "N/A") return "N/A";
    QFile file(QString::fromStdString(device.wakeup_path));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString status = in.readAll().trimmed();
        file.close();
        return status;}
    return "N/A";}

void WebSocket::portBreakerLog(QString msg) {
    QJsonObject log;
    log["type"] = "log";
    log["message"] = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(msg);
    QMutexLocker lock(&m_clientsMutex);
    for (QWebSocket* pSocket : m_clients) {
        pSocket->sendTextMessage(QJsonDocument(log).toJson(QJsonDocument::Compact));}}

void WebSocket::portBreakerWatchdogTriggered(QString vid_pid) {
    QJsonObject alert;
    alert["type"] = "alert";
    alert["source"] = "watchdog";
    alert["vid_pid"] = vid_pid;
    alert["message"] = QString("Device %1 disconnected unexpectedly!").arg(vid_pid);    
    QMutexLocker lock(&m_clientsMutex);
    for (QWebSocket* pSocket : m_clients) {
        pSocket->sendTextMessage(QJsonDocument(alert).toJson(QJsonDocument::Compact));}}

void WebSocket::onPeriodicUpdateTimeout() {
    QMutexLocker lock(&m_clientsMutex);
    for (QWebSocket* pSocket : m_clients) {
        sendDeviceList(pSocket);}}

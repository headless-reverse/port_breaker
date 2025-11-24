#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <QObject>
#include <QWebSocketServer>
#include <QList>
#include <QTimer>
#include <QMutex>
#include "PortBreaker.h"

class QWebSocket;
class WebSocket : public QObject {
    Q_OBJECT

public:
    explicit WebSocket(PortBreaker* portBreaker, quint16 port, QObject *parent = nullptr);
    ~WebSocket();

private slots:
    void onNewConnection();
    void processTextMessage(QString message);
    void socketDisconnected();
    void portBreakerLog(QString msg);
    void portBreakerWatchdogTriggered(QString vid_pid);
    void onPeriodicUpdateTimeout();

private:
    QWebSocketServer* m_pWebSocketServer;
    QList<QWebSocket*> m_clients;
    PortBreaker* m_portBreaker;
    QTimer* m_periodicUpdateTimer;
    QMutex m_clientsMutex;

    void processCommand(QWebSocket* pSocket, const QString& command, const QString& data);
    void sendDeviceList(QWebSocket* pSocket);
    QJsonObject singleDeviceToJson(const UsbDevice& device); 
    QString getDeviceStatus(const UsbDevice& device);
    QString getWakeupStatus(const UsbDevice& device);
};

#endif

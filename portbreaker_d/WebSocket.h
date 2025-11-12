#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "PortBreaker.h"

class WebSocket : public QObject {
    Q_OBJECT

public:
    explicit WebSocket(PortBreaker* breaker, quint16 port, QObject *parent = nullptr);
    ~WebSocket();

private slots:
    void onNewConnection();
    void processTextMessage(QString message);
    void socketDisconnected();

private:
    QWebSocketServer* m_pWebSocketServer;
    QList<QWebSocket*> m_clients;
    PortBreaker* m_portBreaker;
    QJsonDocument getDevicesJson();
    void sendResponse(QWebSocket* client, const QString& type, const QString& message, bool success = true);
    void broadcast(const QJsonDocument& doc);
};

#endif // WEBSOCKET_H

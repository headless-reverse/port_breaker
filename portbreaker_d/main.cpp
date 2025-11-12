#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include "WebSocket.h"
#include "PortBreaker.h"

#define CONFIG_PATH "/usr/local/etc/portbreaker/portbreaker.conf"
#define DEFAULT_PORT 7678
#define DEFAULT_FLAG_FILTER_ALL false

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    qDebug() << "[*] portbreaker_d - WebSocket";
    QSettings settings(CONFIG_PATH, QSettings::IniFormat);
    quint16 port = settings.value("Server/port", DEFAULT_PORT).toUInt();
    bool startFilterAll = settings.value("PortBreaker/startFilterAll", DEFAULT_FLAG_FILTER_ALL).toBool();
    qDebug() << "[*] Wczytana konfiguracja - Port:" << port << ", Start Filter All:" << startFilterAll;
    PortBreaker breaker;
    breaker.setFilterMode(startFilterAll);     
    WebSocket server(&breaker, port); 
    return a.exec();
}

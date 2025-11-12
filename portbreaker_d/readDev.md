# 🛰️ portbreaker_d — wersja daemon (WebSocket)

**usługa systemowa (systemd service)  zdalne sterowanie portami USB poprzez WebSocket API**

    cmake -B build
    cmake --build build -j$(nproc)

## Wymagane Pliki:

### PLIK /etc/systemd/system/portbreaker.service
```ini
[Unit]
Description=port_breaker --daemon (WebSocket)
After=network.target
[Service]
ExecStart=/usr/local/bin/portbreaker_d
User=root
Group=root
Type=simple
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
[Install]
WantedBy=multi-user.target
```
### PLIK /usr/local/etc/portbreaker/portbreaker.conf
```ini
[Server]
port=7678

[PortBreaker]
startFilterAll=false
```
## systemctl:

**Przeładowanie Konfiguracji / LOGI**

    systemctl daemon-reload
    journalctl -u portbreaker.service -f
**włącz/wyłącz uruchamianie przy starcie**

    systemctl enable portbreaker.service
    systemctl disable portbreaker.service
**start/stop/status daemon**

    systemctl start portbreaker.service
    systemctl stop portbreaker.service
    systemctl status portbreaker.service

## 🌐 Katalog html
zmien index.html <br>
const WS_URL = "ws://127.0.0.1:7678";<br>
w zależności od conf

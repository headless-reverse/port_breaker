# Instrukcje Uruchomienia i Zarządzania Demonem portbreaker_d (WebSocket)
## Wymagane Pliki:

### PLIK /etc/systemd/system/portbreaker.service

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

### PLIK /usr/local/etc/portbreaker/portbreaker.conf

[Server]
port=7678

[PortBreaker]
startFilterAll=false

## Zarządzanie Usługą:

### nowy plik konfiguracyjny:
systemctl daemon-reload
### włącz automatyczne uruchamianie przy starcie
systemctl enable portbreaker.service
### uruchom daemon natychmiast
systemctl start portbreaker.service
### sprawdzenie statusu
systemctl status portbreaker
### logi w czasie rzeczywistym
journalctl -u portbreaker.service -f
### zatrzymanie daemon
systemctl stop portbreaker.service
### wyłączenie automatycznego uruchamiania przy starcie systemu
systemctl disable portbreaker.service

## Katalog html
zmien
const WS_URL = "ws://127.0.0.1:7678";
w zależności od conf

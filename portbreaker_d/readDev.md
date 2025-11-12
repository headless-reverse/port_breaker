# Instrukcje Uruchomienia i Zarządzania Demonem portbreaker_d (WebSocket)
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
## Zarządzanie Usługą:

**Przeładowanie Konfiguracji**
```bash
sudo systemctl daemon-reload
```
**włącz uruchamianie przy starcie**
```bash
systemctl enable portbreaker.service
```
**uruchom daemon**
```bash
systemctl start portbreaker.service
```
**status**
```bash
systemctl status portbreaker.service
```
**logi w czasie rzeczywistym**
```bash
journalctl -u portbreaker.service -f
```
**zatrzymanie daemon**
```bash
systemctl stop portbreaker.service
```
**wyłączenie uruchamiania przy starcie systemu**
```bash
systemctl disable portbreaker.service
```

## 🌐 Katalog html
zmien<br>
const WS_URL = "ws://127.0.0.1:7678";<br>
w zależności od conf

<img width="1131" height="653" alt="obraz" src="https://github.com/user-attachments/assets/eb3f7ae5-29db-4816-8ab3-3b6b08902ea3" />

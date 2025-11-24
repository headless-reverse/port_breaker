**  W BUDOWIE **
# ⚡ port_breaker
zarządzanie portami usb / sysfs, ioctl - app deskopowa w C/C ++Qt6  (Linux)
    
    cmake -B build
    cmake --build build -j$(nproc)

  <img width="1081" height="399" alt="portbreaker" src="https://github.com/user-attachments/assets/202b44da-e1e6-499d-8149-569f1a43d560" />

## 🧠 Opis Techniczny

**🔌 (Enable/Disable)**  
Zapis sekwencji echo 1 lub echo 0 do pliku /sys/bus/usb/devices/*/authorized.  
Logiczne odłączanie (0) lub podłączanie (1) urządzenia.  
  
**⏱️ Disable (timer)**  
Zapis echo 0 do authorized, następnie automatyczny echo 1 po czasie ...sek.  
Używa QTimer (Qt) do ponownego włączenia po opóźnieniu (ms).  
  
**♻️ Reset (sysfs)**  
Sekwencja zapisu echo 0 → echo 1 do pliku authorized.  
Symuluje fizyczne odłączenie/podłączenie urządzenia (miękki reset).  
  
**💥 Reset (ioctl)**  
Wywołanie ioctl(USBDEVFS_RESET) na /dev/bus/usb/....  
Twardy reset na poziomie jądra — wymaga ścieżki urządzenia.  
  
**🌐 Globalny Reset Szyny**  
Iteracja po wszystkich usbX/authorized, zapis 0 → 1.  
Resetuje porty na kontrolerach hosta.  
  
**🌙 Zarządzanie Wake-up**  
Zapis enabled lub disabled do /sys/.../power/wakeup.  
Kontroluje, czy urządzenie może wybudzić system (ACPI).  

## 🧩 Logika Aplikacji

**🔍 Wykrywanie Urządzeń**  
Skanowanie i parsowanie drzewa katalogów w /sys/bus/usb/devices/.  
Identyfikacja urządzeń i pobieranie ich ścieżek (authorized_path, wakeup_path).  
  
**🗂️ Mapowanie Nazw**  
Wyszukiwanie par VID:PID w plikach konfiguracyjnych usb.ids.  
Tłumaczenie ID na czytelne nazwy producenta i produktu.  
  
**🚫 Filtrowanie Root Hubów**  
Domyślnie ukrywa urządzenia o VID 1d6b:*. <-- USTAW POD SIEBIE.  
PLIK mainwindow.cpp linia 257 if (dev.vid_pid != "N/A" && dev.vid_pid.rfind("1d6b:", 0) != 0) {  
// Filtr: Pokaż tylko urządzenia z VID:PID, które nie są Root Hubami (1d6b:*)  
Ogranicza widoczność do faktycznych urządzeń, z możliwością wyłączenia filtra.  
  
**🧾 Wymagania Systemowe**  
Sprawdzenie geteuid() == 0.  
Aplikacja wymaga root do operacji zapisu sysfs / ioctl.  

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

## 🌐 html.tar.xz
zmien index.html  
const WS_URL = "ws://127.0.0.1:7678";  
w zależności od conf

<img width="1131" height="653" alt="obraz" src="https://github.com/user-attachments/assets/eb3f7ae5-29db-4816-8ab3-3b6b08902ea3" />

# 🖥 portbreaker — wersja command_line_interface
```bash
gcc -o portbreaker portbreaker.c -lrt
```
```ini
portbreaker  
Użycie: portbreaker [opcja] [argumenty]
  -e, --enable <SYSFS_PATH>             Włącz urządzenie. (Użyj ścieżki z kolumny [PATH]).
  -d, --disable <SYSFS_PATH>            Wyłącz urządzenie.
  -r, --reset-sysfs <SYSFS_PATH>        Resetuj (Wyłącz->Włącz) urządzenie Sysfs.
  -t, --timed-disable <PATH> <SEC>      Wyłącz na SEC sekund, a następnie włącz.
  -i, --reset-ioctl <DEV_PATH>          Resetuj przez ioctl (dla /dev/bus/usb/XXX/YYY).
  -R, --reset-all                       Resetuj wszystkie kontrolery hosta (Sysfs).
Lista:
  -l, --list 'all'                      Lista urządzeń (pomija hosty, 'all' pokazuje wszystkie).
```

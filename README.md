# ⚡ port_breaker
zarządzanie portami usb / sysfs, ioctl - app deskopowa w C/C ++Qt6  (Linux)
    
    cmake -B build
    cmake --build build -j$(nproc)

  <img width="1081" height="399" alt="portbreaker" src="https://github.com/user-attachments/assets/202b44da-e1e6-499d-8149-569f1a43d560" />

## 🧠 Opis Techniczny

**🔌 (Enable/Disable)**<br>
Zapis sekwencji echo 1 lub echo 0 do pliku /sys/bus/usb/devices/*/authorized.<br>
Logiczne odłączanie (0) lub podłączanie (1) urządzenia.<br>
**⏱️ Disable (timer)**<br>
Zapis echo 0 do authorized, następnie automatyczny echo 1 po czasie ...sek.<br>
Używa QTimer (Qt) do ponownego włączenia po opóźnieniu (ms).<br>
**♻️ Reset (sysfs)**<br>
Szybka sekwencja zapisu echo 0 → echo 1 do pliku authorized.<br>
Symuluje fizyczne odłączenie/podłączenie urządzenia (miękki reset).<br>
**💥 Reset (ioctl)**<br>
Wywołanie ioctl(USBDEVFS_RESET) na /dev/bus/usb/....<br>
Twardy reset na poziomie jądra — wymaga ścieżki urządzenia.<br>
**🌐 Globalny Reset Szyny**<br>
Iteracja po wszystkich usbX/authorized, zapis 0 → 1.<br>
Resetuje wszystkie porty i urządzenia na kontrolerach hosta.<br>
**🌙 Zarządzanie Wake-up**<br>
Zapis enabled lub disabled do /sys/.../power/wakeup.<br>
Kontroluje, czy urządzenie może wybudzić system (ACPI).<br>

## 🧩 Logika Aplikacji

**🔍 Wykrywanie Urządzeń**<br>
Skanowanie i parsowanie drzewa katalogów w /sys/bus/usb/devices/.<br>
Identyfikacja urządzeń i pobieranie ich ścieżek (authorized_path, wakeup_path).<br>
**🗂️ Mapowanie Nazw**<br>
Wyszukiwanie par VID:PID w plikach konfiguracyjnych usb.ids.<br>
Tłumaczenie ID na czytelne nazwy producenta i produktu.<br>
**🚫 Filtrowanie Root Hubów**<br>
Domyślnie ukrywa urządzenia o VID 1d6b:*. <-- USTAW POD SIEBIE.<br>
PLIK mainwindow.cpp linia 257 if (dev.vid_pid != "N/A" && dev.vid_pid.rfind("1d6b:", 0) != 0) {<br>
// Filtr: Pokaż tylko urządzenia z VID:PID, które nie są Root Hubami (1d6b:*)<br>
Ogranicza widoczność do faktycznych urządzeń peryferyjnych, z możliwością wyłączenia filtra.<br>
**🧾 Wymagania Systemowe**<br>
Sprawdzenie geteuid() == 0.<br>
Aplikacja wymaga roota do operacji zapisu sysfs / ioctl.<br>

# 🛰️ portbreaker_d — wersja daemon (WebSocket)

**usługa systemowa (systemd service)  zdalne sterowanie portami USB poprzez WebSocket API**

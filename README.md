**⚡ port_breaker**
zarządzanie portami usb / sysfs, ioctl - app deskopowa w C/C ++Qt6  (Linux)
    
    cmake -B build
    cmake --build build -j$(nproc)

  <img width="1081" height="399" alt="portbreaker" src="https://github.com/user-attachments/assets/202b44da-e1e6-499d-8149-569f1a43d560" />

#🧠 Opis Techniczny

**🔌 (Enable/Disable)**<br>
Zapis sekwencji echo 1 lub echo 0 do pliku /sys/bus/usb/devices/*/authorized.<br>
Logiczne odłączanie (0) lub podłączanie (1) urządzenia.
**⏱️ Disable (timer)**
Zapis echo 0 do authorized, następnie automatyczny echo 1 po czasie ...sek.
Używa QTimer (Qt) do ponownego włączenia po opóźnieniu (ms).
**♻️ Reset (sysfs)**
Szybka sekwencja zapisu echo 0 → echo 1 do pliku authorized.,
Symuluje fizyczne odłączenie/podłączenie urządzenia (miękki reset).
**💥 Reset (ioctl)**
Wywołanie ioctl(USBDEVFS_RESET) na /dev/bus/usb/....
Twardy reset na poziomie jądra — wymaga ścieżki urządzenia.
**🌐 Globalny Reset Szyny**
Iteracja po wszystkich usbX/authorized, zapis 0 → 1.
Resetuje wszystkie porty i urządzenia na kontrolerach hosta.
**🌙 Zarządzanie Wake-up**
Zapis enabled lub disabled do /sys/.../power/wakeup.
Kontroluje, czy urządzenie może wybudzić system (ACPI).

#🧩 Logika Aplikacji

**🔍 Wykrywanie Urządzeń**
Skanowanie i parsowanie drzewa katalogów w /sys/bus/usb/devices/.,
Identyfikacja urządzeń i pobieranie ich ścieżek (authorized_path, wakeup_path).
**🗂️ Mapowanie Nazw**
Wyszukiwanie par VID:PID w plikach konfiguracyjnych usb.ids.
Tłumaczenie ID na czytelne nazwy producenta i produktu.
**🚫 Filtrowanie Root Hubów**
Domyślnie ukrywa urządzenia o VID 1d6b:*. <-- USTAW POD SIEBIE.
PLIK mainwindow.cpp linia 272  if (dev.vid_pid != "N/A" && dev.vid_pid.rfind("1d6b:", 0) != 0) {
// Filtr: Pokaż tylko urządzenia z VID:PID, które nie są Root Hubami (1d6b:*)
Ogranicza widoczność do faktycznych urządzeń peryferyjnych, z możliwością wyłączenia filtra.
**🧾 Wymagania Systemowe**
Sprawdzenie geteuid() == 0.
Aplikacja wymaga roota do operacji zapisu sysfs / ioctl.

# 🛰️ portbreaker_d — wersja daemon (WebSocket Server)

usługa systemowa (systemd service), zapewniająca zdalne sterowanie portami USB poprzez WebSocket API.

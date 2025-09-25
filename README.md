# port_breaker
zarządzanie portami usb / sysfs , ioctl 
Zaawansowane Narzędzie do Zarządzania i Resetowania Urządzeń USB (Linux)

PortBreaker jest aplikacją desktopową stworzoną w oparciu o framework Qt6, przeznaczoną do niskopoziomowej interakcji z urządzeniami USB w systemach operacyjnych Linux. Narzędzie to wykorzystuje krytyczne mechanizmy jądra — system plików sysfs oraz wywołania ioctl — w celu precyzyjnej kontroli nad autoryzacją i stanem portów USB.

Wymóg operacyjny: Wszelkie operacje kontrolne i modyfikujące stan urządzeń USB wymagają uprawnień administracyjnych (root). Aplikacja egzekwuje ten wymóg, sprawdzając efektywny identyfikator użytkownika (geteuid()) przy starcie.

Kluczowe Funkcjonalności

Funkcjonalność	Opis techniczny
Zarządzanie Autoryzacją	Umożliwia włączanie (echo 1) i wyłączanie (echo 0) urządzenia poprzez modyfikację wartości w pliku /sys/bus/usb/devices/*/authorized. Jest to metoda programowego odłączania/podłączania logicznego.
Resetowanie (Sysfs)	Przeprowadza miękki reset urządzenia. Procedura polega na szybkim zapisie sekwencji 0 → 1 do pliku authorized, co symuluje ponowne podłączenie urządzenia do szyny.
Resetowanie (ioctl)	Wykonuje twardy reset urządzenia na poziomie jądra. Wykorzystuje wywołanie ioctl z komendą USBDEVFS_RESET na pliku urządzenia (np. /dev/bus/usb/001/00X). Jest to bardziej inwazywna i często skuteczniejsza metoda przy problemach z urządzeniem.
Globalny Reset Szyny	Umożliwia resetowanie wszystkich kontrolerów hosta USB (usb1, usb2...) poprzez pętlę wyłączania i włączania ich autoryzacji w sysfs. Skutkuje to restartem wszystkich podłączonych urządzeń USB.
Wykrywanie Urządzeń	Skanuje drzewo /sys/bus/usb/devices/ w celu identyfikacji podłączonych urządzeń, pobierając ich VID:PID oraz odczytując ich aktualny stan autoryzacji.
Mapowanie Nazw	Wykorzystuje pliki standardu usb.ids (np. /usr/share/hwdata/usb.ids) do tłumaczenia par VID:PID na czytelne nazwy producenta i produktu.

KOMPILACJA
  cmake -B build
  cmake --build build -j$(nproc)

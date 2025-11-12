// gcc -o portbreaker portbreaker.c -lrt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/usbdevice_fs.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include <libgen.h>
#define MAX_PATH 256
#define MAX_NAME 64
#define MAX_DEVICES 128
#define SYSFS_USB_DEVICES "/sys/bus/usb/devices/"
struct UsbDevice {
    char path[MAX_PATH];
    char authorized_path[MAX_PATH];
    char vid_pid[9];
    char name[MAX_NAME];
    char dev_path[MAX_PATH];
    char wakeup_path[MAX_PATH];
};
static char* read_sysfs_file(const char *file_path, char *buffer, size_t buffer_size) {
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        strncpy(buffer, "", buffer_size);
        return NULL;
    }
    if (fgets(buffer, buffer_size, fp) != NULL) {
        size_t len = strlen(buffer);
        while (len > 0 && isspace((unsigned char)buffer[len - 1])) {
            buffer[--len] = '\0';
        }
    } else {
        strncpy(buffer, "", buffer_size);
    }
    fclose(fp);
    return buffer;
}
static int write_sysfs_file(const char *file_path, const char *value) {
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        fprintf(stderr, "Błąd: Nie można otworzyć %s do zapisu. Wymagane uprawnienia root.\n", file_path);
        return 0;
    }
    if (fputs(value, fp) < 0) {
        fprintf(stderr, "Błąd zapisu do %s.\n", file_path);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}
static void get_device_name(const char *sysfs_path, char *name_buffer) {
    char product_path[MAX_PATH];
    char product_buffer[MAX_NAME];
    snprintf(product_path, MAX_PATH, "%s/product", sysfs_path);
    if (read_sysfs_file(product_path, product_buffer, MAX_NAME)) {
        strncpy(name_buffer, product_buffer, MAX_NAME);
        name_buffer[MAX_NAME - 1] = '\0';
        return;
    }
    char path_copy[MAX_PATH];
    strncpy(path_copy, sysfs_path, MAX_PATH);
    strncpy(name_buffer, basename(path_copy), MAX_NAME);
    name_buffer[MAX_NAME - 1] = '\0';
}
static int get_devices_list(struct UsbDevice devices[], int max_devices) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    if ((dir = opendir(SYSFS_USB_DEVICES)) == NULL) {
        perror("Błąd otwarcia katalogu Sysfs");
        return 0;
    }
    while ((entry = readdir(dir)) != NULL && count < max_devices) {
        if (entry->d_name[0] == '.') continue;
        char path[MAX_PATH];
        snprintf(path, MAX_PATH, "%s%s", SYSFS_USB_DEVICES, entry->d_name);
        struct stat statbuf;
        if (stat(path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode)) continue;
        char id_path[MAX_PATH];
        char vid_pid_buffer[9];
        snprintf(id_path, MAX_PATH, "%s/idVendor", path);
        if (access(id_path, F_OK) != 0) continue;
        char vid[5], pid[5];
        if (!read_sysfs_file(id_path, vid, 5)) continue;
        snprintf(id_path, MAX_PATH, "%s/idProduct", path);
        if (!read_sysfs_file(id_path, pid, 5)) continue;
        snprintf(vid_pid_buffer, 9, "%s:%s", vid, pid);
        struct UsbDevice *dev = &devices[count++];
        strncpy(dev->path, path, MAX_PATH);
        snprintf(dev->authorized_path, MAX_PATH, "%s/authorized", path);
        strncpy(dev->vid_pid, vid_pid_buffer, 9);
        get_device_name(path, dev->name);
        snprintf(dev->wakeup_path, MAX_PATH, "%s/power/wakeup", path);
        strncpy(dev->dev_path, "", MAX_PATH);
    }
    closedir(dir);
    return count;
}
static int ioctl_reset_device(const char *dev_path) {
    int fd = open(dev_path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Błąd: Nie można otworzyć %s: %s\n", dev_path, strerror(errno));
        return 0;
    }
    fprintf(stdout, "Resetowanie przez IOCTL %s...\n", dev_path);
    if (ioctl(fd, USBDEVFS_RESET, 0) < 0) {
        fprintf(stderr, "Błąd IOCTL (USBDEVFS_RESET) dla %s: %s\n", dev_path, strerror(errno));
        close(fd);
        return 0;
    }
    fprintf(stdout, "Sukces: IOCTL reset zakończony.\n");
    close(fd);
    return 1;
}
static int reset_device_sysfs(const char *sysfs_path) {
    char authorized_path[MAX_PATH];
    snprintf(authorized_path, MAX_PATH, "%s/authorized", sysfs_path);
    if (write_sysfs_file(authorized_path, "0")) {
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
        return write_sysfs_file(authorized_path, "1");
    }
    return 0;
}
static int toggle_device_sysfs(const char *sysfs_path, int enable) {
    char authorized_path[MAX_PATH];
    snprintf(authorized_path, MAX_PATH, "%s/authorized", sysfs_path);
    return write_sysfs_file(authorized_path, enable ? "1" : "0");
}
static int reset_all_devices_sysfs() {
    DIR *dir;
    struct dirent *entry;
    int all_disabled = 1;
    int all_enabled = 1;
    if ((dir = opendir(SYSFS_USB_DEVICES)) == NULL) {
        perror("Błąd otwarcia katalogu Sysfs");
        return 0;
    }
    char hostControllers[MAX_DEVICES][MAX_NAME];
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < MAX_DEVICES) {
        if (strncmp(entry->d_name, "usb", 3) == 0) {
            int is_controller = 1;
            for (size_t i = 3; i < strlen(entry->d_name); i++) {
                if (!isdigit((unsigned char)entry->d_name[i])) {
                    is_controller = 0;
                    break;
                }
            }
            if (is_controller && strlen(entry->d_name) > 3) {
                strncpy(hostControllers[count++], entry->d_name, MAX_NAME);
            }
        }
    }
    closedir(dir);
    if (count == 0) {
        fprintf(stderr, "Brak kontrolerów hosta USB do zresetowania.\n");
        return 0;
    }
    for (int i = 0; i < count; i++) {
        char authorized_path[MAX_PATH];
        snprintf(authorized_path, MAX_PATH, "%s%s/authorized", SYSFS_USB_DEVICES, hostControllers[i]);
        if (access(authorized_path, F_OK) == 0) {
            if (!write_sysfs_file(authorized_path, "0")) {
                fprintf(stderr, "Błąd wyłączania kontrolera: %s\n", hostControllers[i]);
                all_disabled = 0;
            }
        }
    }
    struct timespec ts = {1, 0};
    nanosleep(&ts, NULL);
    for (int i = 0; i < count; i++) {
        char authorized_path[MAX_PATH];
        snprintf(authorized_path, MAX_PATH, "%s%s/authorized", SYSFS_USB_DEVICES, hostControllers[i]);
        if (access(authorized_path, F_OK) == 0) {
            if (!write_sysfs_file(authorized_path, "1")) {
                fprintf(stderr, "Błąd włączania kontrolera: %s\n", hostControllers[i]);
                all_enabled = 0;
            }
        }
    }
    return all_disabled && all_enabled;
}
void print_usage(const char *appName) {
    fprintf(stdout, "Użycie: %s [opcja] [argumenty]\n", appName);
    fprintf(stdout, "  -e, --enable <SYSFS_PATH>      	Włącz urządzenie. (Użyj ścieżki z kolumny [PATH]).\n");
    fprintf(stdout, "  -d, --disable <SYSFS_PATH>     	Wyłącz urządzenie.\n");
    fprintf(stdout, "  -r, --reset-sysfs <SYSFS_PATH> 	Resetuj (Wyłącz->Włącz) urządzenie Sysfs.\n");
    fprintf(stdout, "  -t, --timed-disable <PATH> <SEC>	Wyłącz na SEC sekund, a następnie włącz.\n");
    fprintf(stdout, "  -i, --reset-ioctl <DEV_PATH>   	Resetuj przez ioctl (dla /dev/bus/usb/XXX/YYY).\n");
    fprintf(stdout, "  -R, --reset-all                	Resetuj wszystkie kontrolery hosta (Sysfs).\n");
    fprintf(stdout, "Lista:\n");
    fprintf(stdout, "  -l, --list 'all'              	Lista urządzeń (pomija hosty, 'all' pokazuje wszystkie).\n");
}
int main(int argc, char *argv[]) {
    if (argc >= 2) {
        const char *arg = argv[1];
        int is_modifying_command = (strcmp(arg, "-l") != 0 && strcmp(arg, "--list") != 0 && strcmp(arg, "-h") != 0 && strcmp(arg, "--help") != 0);
        if (is_modifying_command && geteuid() != 0) {
            fprintf(stderr, "Błąd: Wymagane uprawnienia root (sudo) do modyfikacji urządzeń.\n");
            return 1;
        }
    }
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0) {
        struct UsbDevice devices[MAX_DEVICES];
        int count = get_devices_list(devices, MAX_DEVICES);
        int show_all = (argc > 2 && (strcmp(argv[2], "all") == 0));
        if (count == 0) {
            fprintf(stdout, "Brak urządzeń USB.\n");
            return 0;
        }
        fprintf(stdout, "%-35s | %-7s | %-8s | %-7s | %s\n", 
                "Path", "VID:PID", "Status", "Wake-up", "Nazwa Urządzenia");
        fprintf(stdout, "------------------------------------------------------------------------------------------------\n");
        for (int i = 0; i < count; i++) {
            if (!show_all && (strcmp(devices[i].vid_pid, "N/A") == 0 || strncmp(devices[i].vid_pid, "1d6b", 4) == 0)) {
                continue;
            }
            char status[10] = "N/A";
            char authorized_buffer[10];
            read_sysfs_file(devices[i].authorized_path, authorized_buffer, 10);
            if (strcmp(authorized_buffer, "1") == 0) strncpy(status, "Aktywny", 10);
            else if (strcmp(authorized_buffer, "0") == 0) strncpy(status, "Wylaczony", 10);
            char wakeup_buffer[10] = "N/A";
            if (devices[i].wakeup_path[0] != '\0' && read_sysfs_file(devices[i].wakeup_path, wakeup_buffer, 10)) {
                size_t len = strlen(wakeup_buffer);
                if (len > 0 && isspace((unsigned char)wakeup_buffer[len - 1])) wakeup_buffer[len - 1] = '\0';
            }            
            fprintf(stdout, "%-35s | %-7s | %-8s | %-7s | %s\n",
                    devices[i].path,
                    devices[i].vid_pid,
                    status,
                    wakeup_buffer,
                    devices[i].name);
        }
        return 0;
    }
    if (strcmp(argv[1], "-R") == 0 || strcmp(argv[1], "--reset-all") == 0) {
        if (reset_all_devices_sysfs()) {
            fprintf(stdout, "Sukces: Zresetowano wszystkie kontrolery hosta.\n");
            return 0;
        } else {
            fprintf(stderr, "Błąd: Nie powiodło się zresetowanie wszystkich kontrolerów hosta.\n");
            return 1;
        }
    }
    if (argc < 3) {
        fprintf(stderr, "Błąd: Brakuje argumentu.\n");
        print_usage(argv[0]);
        return 1;
    }
    const char *path_or_devpath = argv[2];
    if (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--enable") == 0) {
        if (toggle_device_sysfs(path_or_devpath, 1)) fprintf(stdout, "Sukces: Włączono: %s\n", path_or_devpath);
        else goto error_exit;
    } else if (strcmp(argv[1], "-d") == 0 || strcmp(argv[1], "--disable") == 0) {
        if (toggle_device_sysfs(path_or_devpath, 0)) fprintf(stdout, "Sukces: Wyłączono: %s\n", path_or_devpath);
        else goto error_exit;
    } else if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--reset-sysfs") == 0) {
        if (reset_device_sysfs(path_or_devpath)) fprintf(stdout, "Sukces: Zresetowano (Sysfs): %s\n", path_or_devpath);
        else goto error_exit;
    } else if (strcmp(argv[1], "-i") == 0 || strcmp(argv[1], "--reset-ioctl") == 0) {
        if (ioctl_reset_device(path_or_devpath)) fprintf(stdout, "Sukces: Zresetowano (ioctl): %s\n", path_or_devpath);
        else goto error_exit;
    } else if (strcmp(argv[1], "-t") == 0 || strcmp(argv[1], "--timed-disable") == 0) {
        if (argc < 4) {
             fprintf(stderr, "Błąd: Brakuje czasu (SEC). Użycie: -t <PATH> <SEC>\n");
             return 1;
        }
        int delay_sec = atoi(argv[3]);
        if (!toggle_device_sysfs(path_or_devpath, 0)) {
            fprintf(stderr, "Błąd: Nie powiodło się wyłączenie urządzenia: %s\n", path_or_devpath);
            return 1;
        }
        fprintf(stdout, "Wyłączono. Czekam %d sekund...\n", delay_sec);
        sleep(delay_sec);
        if (toggle_device_sysfs(path_or_devpath, 1)) fprintf(stdout, "Sukces: Urządzenie włączone ponownie: %s\n", path_or_devpath);
        else goto error_exit;
    } else {
        fprintf(stderr, "Nieznana opcja: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
    return 0;
error_exit:
    fprintf(stderr, "Błąd: Operacja nie powiodła się.\n");
    return 1;
}

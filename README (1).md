# QR Scan Logger — ESP8266 + PHP + HTML

Sistem pencatatan scan QR/barcode: kamera HP → PHP API → MySQL, plus ESP8266 sebagai klien IoT.

---

## Arsitektur

```
[HP Browser]  →  scanner.html  →  api.php  →  MySQL
[ESP8266]     →  WiFi HTTP     →  api.php  →  MySQL
```

---

## File

| File | Fungsi |
|------|--------|
| `qrscan_esp8266.ino` | Firmware Arduino ESP8266 |
| `api.php` | REST API PHP (CRUD) |
| `scanner.html` | Dashboard scan QR via browser/HP |

---

## 1. Setup Database MySQL

```sql
CREATE DATABASE qrscan_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

Buat tabel otomatis: buka browser ke:
```
http://localhost/qrscan-api/api.php?action=install
```

Atau buat manual:
```sql
CREATE TABLE scan_logs (
    id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    qr_data    VARCHAR(500) NOT NULL,
    device_id  VARCHAR(100) NOT NULL DEFAULT 'unknown',
    rssi       SMALLINT     NOT NULL DEFAULT 0,
    ip_address VARCHAR(45)  NOT NULL DEFAULT '',
    created_at DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_device  (device_id),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

---

## 2. Setup PHP Server

1. Copy `api.php` ke folder web server (XAMPP: `htdocs/qrscan-api/`)
2. Edit konfigurasi di `api.php`:

```php
define('DB_HOST', 'localhost');
define('DB_NAME', 'qrscan_db');
define('DB_USER', 'root');
define('DB_PASS', '');
```

3. Pastikan server bisa diakses dari jaringan lokal (nonaktifkan firewall untuk port 80)

---

## 3. Setup ESP8266

### Library yang dibutuhkan (Arduino Library Manager)
- `ArduinoJson` by Benoit Blanchon (v6.x)
- `ESP8266WiFi` (sudah termasuk board package)
- `ESP8266HTTPClient` (sudah termasuk board package)

### Board Manager
Tambahkan URL ke Arduino IDE → Preferences → Additional Board Manager URLs:
```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```
Install: **ESP8266 by ESP8266 Community**

### Konfigurasi di `qrscan_esp8266.ino`
```cpp
const char* WIFI_SSID     = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
const char* SERVER_BASE   = "http://192.168.1.100/qrscan-api";
```

### Upload Settings
- Board: `NodeMCU 1.0 (ESP-12E Module)`
- Upload Speed: `115200`
- Port: sesuai COM port perangkat

### Perintah Serial Monitor (115200 baud)
```
LOG:DATA_QR    → Kirim log scan
LIST           → Tampilkan semua log
DELETE:5       → Hapus log ID 5
STATS          → Statistik
HELP           → Bantuan
```

---

## 4. Setup scanner.html (HP/Browser)

1. Buka `scanner.html` di browser HP (Chrome Android/iOS Safari)
2. Isi URL API di kolom konfigurasi: `http://192.168.1.100/qrscan-api/api.php`
3. Klik **Simpan** → klik **▶ Mulai Scan**
4. Izinkan akses kamera saat diminta
5. Arahkan ke QR code → otomatis tercatat ke server

> **Catatan**: HP dan server harus dalam satu jaringan WiFi yang sama.  
> Untuk akses dari luar jaringan, gunakan domain publik atau port forwarding.

---

## 5. Endpoint API

| Method | URL | Fungsi |
|--------|-----|--------|
| POST | `api.php?action=log` | Simpan log scan baru |
| GET | `api.php?action=list` | Ambil daftar log |
| DELETE | `api.php?action=delete&id=N` | Hapus log by ID |
| GET | `api.php?action=stats` | Statistik ringkas |
| GET | `api.php?action=install` | Inisialisasi tabel |

### Query params untuk `list`
```
?action=list&limit=50&offset=0&q=keyword&device_id=ESP&start_date=2025-01-01&end_date=2025-12-31
```

### Contoh payload POST log
```json
{
  "qr_data": "BARANG-001-ABC",
  "device_id": "ESP8266-AA:BB:CC:DD:EE:FF",
  "rssi": -65
}
```

---

## 6. Wiring LED (opsional)

```
ESP8266 D4  → Resistor 220Ω → LED Hijau → GND
ESP8266 D5  → Resistor 220Ω → LED Merah → GND
```

---

## Lisensi

MIT — Bebas digunakan dan dimodifikasi.  
PT Sismadi Langit Solusi

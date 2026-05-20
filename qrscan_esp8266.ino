/*
 * QR Scan Logger - ESP8266
 * Sistem: ESP8266 <-> PHP API Server <-> MySQL
 * Fungsi: Kirim & ambil log scan QR via WiFi (HTTP)
 * Author: Wawan / PT Sismadi Langit Solusi
 * Framework: Arduino + ESP8266HTTPClient
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// ============================================================
// KONFIGURASI - Sesuaikan sebelum upload
// ============================================================
const char* WIFI_SSID     = "NAMA_WIFI_ANDA";
const char* WIFI_PASSWORD = "PASSWORD_WIFI_ANDA";
const char* SERVER_BASE   = "http://192.168.1.100/qrscan-api";  // IP server PHP

// Pin indikator LED (opsional)
#define LED_SUKSES D4   // LED hijau
#define LED_GAGAL  D5   // LED merah

// ============================================================
// ENDPOINT API
// ============================================================
String urlLog()    { return String(SERVER_BASE) + "/api.php?action=log";    }
String urlList()   { return String(SERVER_BASE) + "/api.php?action=list";   }
String urlDelete() { return String(SERVER_BASE) + "/api.php?action=delete"; }
String urlStats()  { return String(SERVER_BASE) + "/api.php?action=stats";  }

WiFiClient wifiClient;

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_SUKSES, OUTPUT);
  pinMode(LED_GAGAL,  OUTPUT);
  digitalWrite(LED_SUKSES, LOW);
  digitalWrite(LED_GAGAL,  LOW);

  Serial.println("\n=== QR Scan Logger ESP8266 ===");
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[OK] WiFi terhubung");
    Serial.print("[IP] ");
    Serial.println(WiFi.localIP());
    kedipLed(LED_SUKSES, 3);
  } else {
    Serial.println("\n[GAGAL] Tidak bisa terhubung WiFi");
    kedipLed(LED_GAGAL, 5);
  }

  tampilkanPetunjuk();
}

// ============================================================
// LOOP - Baca perintah dari Serial Monitor
// ============================================================
void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() == 0) return;

    // Format perintah:
    // LOG:QRCODE_DATA        => simpan log scan
    // LIST                   => tampilkan semua log
    // DELETE:id              => hapus log by ID
    // STATS                  => tampilkan statistik
    // HELP                   => bantuan

    if (input.startsWith("LOG:")) {
      String qrData = input.substring(4);
      qrData.trim();
      if (qrData.length() > 0) {
        kirimLogScan(qrData);
      } else {
        Serial.println("[!] QR data kosong");
      }

    } else if (input == "LIST") {
      ambilDaftarLog();

    } else if (input.startsWith("DELETE:")) {
      String idStr = input.substring(7);
      idStr.trim();
      hapusLog(idStr.toInt());

    } else if (input == "STATS") {
      ambilStatistik();

    } else if (input == "HELP") {
      tampilkanPetunjuk();

    } else {
      // Asumsikan input langsung adalah data QR (dari scanner USB/serial)
      kirimLogScan(input);
    }
  }
}

// ============================================================
// FUNGSI: Kirim log scan ke server (CREATE)
// ============================================================
void kirimLogScan(String qrData) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[!] WiFi terputus, mencoba reconnect...");
    WiFi.reconnect();
    delay(3000);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[GAGAL] Tidak bisa reconnect");
      kedipLed(LED_GAGAL, 2);
      return;
    }
  }

  HTTPClient http;
  http.begin(wifiClient, urlLog());
  http.addHeader("Content-Type", "application/json");

  // Buat payload JSON
  StaticJsonDocument<256> doc;
  doc["qr_data"]    = qrData;
  doc["device_id"]  = "ESP8266-" + String(WiFi.macAddress());
  doc["rssi"]       = WiFi.RSSI();

  String payload;
  serializeJson(doc, payload);

  Serial.print("[>] Kirim: ");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    String respBody = http.getString();
    Serial.print("[<] HTTP ");
    Serial.print(httpCode);
    Serial.print(" | ");
    Serial.println(respBody);

    StaticJsonDocument<256> resp;
    DeserializationError err = deserializeJson(resp, respBody);

    if (!err && resp["status"] == "success") {
      Serial.println("[OK] Log berhasil disimpan");
      Serial.print("     ID: ");
      Serial.println(resp["id"].as<int>());
      kedipLed(LED_SUKSES, 1);
    } else {
      Serial.println("[!] Gagal menyimpan log");
      kedipLed(LED_GAGAL, 1);
    }
  } else {
    Serial.print("[GAGAL] HTTP error: ");
    Serial.println(http.errorToString(httpCode));
    kedipLed(LED_GAGAL, 2);
  }

  http.end();
}

// ============================================================
// FUNGSI: Ambil daftar log (READ)
// ============================================================
void ambilDaftarLog() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[!] WiFi tidak terhubung");
    return;
  }

  HTTPClient http;
  http.begin(wifiClient, urlList());

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String body = http.getString();

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, body);

    if (!err) {
      JsonArray logs = doc["data"].as<JsonArray>();
      int total = doc["total"].as<int>();

      Serial.println("┌─────────────────────────────────────────────────┐");
      Serial.print  ("│ DAFTAR LOG SCAN (total: ");
      Serial.print(total);
      Serial.println(")                        │");
      Serial.println("├──────┬──────────────────────────┬───────────────┤");
      Serial.println("│  ID  │ QR Data                  │ Waktu         │");
      Serial.println("├──────┼──────────────────────────┼───────────────┤");

      for (JsonObject log : logs) {
        String id   = String(log["id"].as<int>());
        String data = log["qr_data"].as<String>();
        String ts   = log["created_at"].as<String>();

        // Potong jika terlalu panjang
        if (data.length() > 24) data = data.substring(0, 21) + "...";
        if (ts.length() > 13)   ts   = ts.substring(5, 16);

        Serial.printf("│ %4s │ %-24s │ %-13s │\n",
          id.c_str(), data.c_str(), ts.c_str());
      }

      Serial.println("└──────┴──────────────────────────┴───────────────┘");
    } else {
      Serial.println("[!] Gagal parse JSON response");
    }
  } else {
    Serial.print("[GAGAL] HTTP: ");
    Serial.println(httpCode);
  }

  http.end();
}

// ============================================================
// FUNGSI: Hapus log by ID (DELETE)
// ============================================================
void hapusLog(int id) {
  if (id <= 0) {
    Serial.println("[!] ID tidak valid");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[!] WiFi tidak terhubung");
    return;
  }

  HTTPClient http;
  String url = urlDelete() + "&id=" + String(id);
  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");

  // Kirim DELETE via POST dengan method override
  StaticJsonDocument<64> doc;
  doc["id"] = id;
  String payload;
  serializeJson(doc, payload);

  int httpCode = http.sendRequest("DELETE", payload);

  if (httpCode > 0) {
    String body = http.getString();
    Serial.print("[<] ");
    Serial.println(body);

    StaticJsonDocument<128> resp;
    if (!deserializeJson(resp, body) && resp["status"] == "success") {
      Serial.print("[OK] Log ID ");
      Serial.print(id);
      Serial.println(" berhasil dihapus");
    } else {
      Serial.println("[!] Gagal menghapus log");
    }
  } else {
    Serial.print("[GAGAL] HTTP: ");
    Serial.println(httpCode);
  }

  http.end();
}

// ============================================================
// FUNGSI: Ambil statistik (READ)
// ============================================================
void ambilStatistik() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[!] WiFi tidak terhubung");
    return;
  }

  HTTPClient http;
  http.begin(wifiClient, urlStats());

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String body = http.getString();
    StaticJsonDocument<512> doc;

    if (!deserializeJson(doc, body)) {
      Serial.println("=== STATISTIK SCAN ===");
      Serial.print("Total Log     : "); Serial.println(doc["total_log"].as<int>());
      Serial.print("Scan Hari Ini : "); Serial.println(doc["hari_ini"].as<int>());
      Serial.print("Device Aktif  : "); Serial.println(doc["device_count"].as<int>());
      Serial.print("Scan Terakhir : "); Serial.println(doc["last_scan"].as<String>());
    }
  } else {
    Serial.print("[GAGAL] HTTP: ");
    Serial.println(httpCode);
  }

  http.end();
}

// ============================================================
// HELPER: Kedipkan LED
// ============================================================
void kedipLed(int pin, int kali) {
  for (int i = 0; i < kali; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

// ============================================================
// HELPER: Tampilkan petunjuk Serial Monitor
// ============================================================
void tampilkanPetunjuk() {
  Serial.println("\n========== PERINTAH SERIAL MONITOR ==========");
  Serial.println("LOG:<data>   => Kirim log scan QR");
  Serial.println("LIST         => Tampilkan semua log");
  Serial.println("DELETE:<id>  => Hapus log berdasarkan ID");
  Serial.println("STATS        => Statistik scan");
  Serial.println("HELP         => Tampilkan petunjuk ini");
  Serial.println("<text>       => Langsung kirim sebagai QR data");
  Serial.println("==============================================\n");
}

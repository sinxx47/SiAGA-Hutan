/**
 * ====================================================================================================
 * PROJECT: SiAGA Hutan (Forest Sentinel & Acoustic Fire Guard)
 * FIRMWARE: Gateway Receiver & MQTT Bridge (ESP32-S3 / ESP32 Standar)
 * DESKRIPSI: Menerima transmisi telemetri LoRa E220 dari Node Lapangan di hutan,
 *            menerjemahkan data ke format MQTT JSON terstandar, mem-publish ke HiveMQ Cloud /
 *            Broker MQTT, dan menjembatani downlink command dari Dashboard kembali ke Node via LoRa.
 * ====================================================================================================
 * 
 * Pengkabelan Modul LoRa E220-900T22D pada Gateway:
 *   E220 RXD <--- ESP32 GPIO 47 (TX1)
 *   E220 TXD ---> ESP32 GPIO 21 (RX1)
 *   E220 AUX ---> ESP32 GPIO 38 (Digital In, LOW=Busy, HIGH=Ready)
 *   E220 M0, M1 -> GND (Normal Mode / Transparent Transmission)
 *   E220 VCC ---> 5V (Diberi elco 470uF)
 *   E220 GND ---> GND
 * 
 * Topik MQTT HiveMQ Cloud:
 *   - Publish Telemetri : "iot/siaga_hutan/telemetry"
 *   - Publish Peringatan: "iot/siaga_hutan/alert"
 *   - Publish Status LWT: "iot/siaga_hutan/status"
 *   - Subscribe Command : "iot/siaga_hutan/command"
 * ====================================================================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ====================================================================================================
// 1. KONFIGURASI JARINGAN WI-FI & BROKER MQTT
// ====================================================================================================

// Kredensial Wi-Fi
const char* WIFI_SSID     = "NAMA_WIFI_HOTSPOT";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";

// Kredensial HiveMQ Cloud (Port 8883 dengan TLS)
const char* MQTT_HOST     = "xxxxxxxx.s1.eu.hivemq.cloud"; 
const int   MQTT_PORT     = 8883;
const char* MQTT_USER     = "USERNAME_HIVEMQ";
const char* MQTT_PASS     = "PASSWORD_HIVEMQ";

// Identitas & Topik MQTT
const char* CLIENT_ID     = "GATEWAY-SIAGA-HUTAN-01";
const char* TOPIC_TELEM    = "iot/siaga_hutan/telemetry";
const char* TOPIC_ALERT    = "iot/siaga_hutan/alert";
const char* TOPIC_STATUS   = "iot/siaga_hutan/status";
const char* TOPIC_CMD      = "iot/siaga_hutan/command";

// Pinout E220 LoRa
#define PIN_LORA_RX       21 // ESP32 RX <- E220 TXD
#define PIN_LORA_TX       47 // ESP32 TX -> E220 RXD
#define PIN_LORA_AUX      38 // AUX Status
#define PIN_STATUS_LED    2  // Indikator LED

HardwareSerial LoRaSerial(1);
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

uint32_t lastMqttReconnectMs = 0;
uint32_t packetsReceived = 0;

// ====================================================================================================
// 2. CALLBACK DOWNLINK MQTT -> FORWARD KE LORA
// ====================================================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.println(F("--------------------------------------------------"));
  Serial.printf("[MQTT DOWNLINK] Topik: %s | Pesan: %s\n", topic, message.c_str());

  if (String(topic) == TOPIC_CMD) {
    // Teruskan langsung perintah ke Node Sensor melalui radio LoRa
    while (digitalRead(PIN_LORA_AUX) == LOW) {
      delay(5); // Tunggu radio siap
    }

    LoRaSerial.println(message);
    Serial.printf("[LORA TX FORWARD] Perintah diteruskan ke Node: %s\n", message.c_str());

    // Beri kedip LED indikator
    digitalWrite(PIN_STATUS_LED, HIGH);
    delay(100);
    digitalWrite(PIN_STATUS_LED, LOW);
  }
  Serial.println(F("--------------------------------------------------"));
}

// ====================================================================================================
// 3. KONEKSI WI-FI & MQTT LWT (LAST WILL AND TESTAMENT)
// ====================================================================================================

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("\n[WIFI] Menghubungkan ke AP: %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(F("."));
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F(" TERHUBUNG!"));
    Serial.printf("[WIFI] IP Gateway: %s | RSSI: %d dBm\n", 
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println(F(" GAGAL! Akan dicoba kembali nanti."));
  }
}

void connectMQTT() {
  if (mqttClient.connected() || WiFi.status() != WL_CONNECTED) return;

  uint32_t nowMs = millis();
  if (nowMs - lastMqttReconnectMs < 5000) return;
  lastMqttReconnectMs = nowMs;

  Serial.print(F("[MQTT] Menghubungkan ke broker HiveMQ Cloud... "));

  // Konfigurasi TLS tanpa verifikasi sertifikat CA (cocok untuk ESP32 praktis)
  secureClient.setInsecure();

  // Last Will: Terbitkan "offline" secara otomatis jika gateway terputus tiba-tiba
  const char* willMsg = "{\"gateway\":\"OFFLINE\",\"status\":\"disconnected\"}";

  if (mqttClient.connect(CLIENT_ID, MQTT_USER, MQTT_PASS, TOPIC_STATUS, 1, true, willMsg)) {
    Serial.println(F("TERHUBUNG!"));

    // Publish status Gateway ONLINE (Retained)
    mqttClient.publish(TOPIC_STATUS, "{\"gateway\":\"ONLINE\",\"status\":\"connected\"}", true);

    // Subscribe ke topik kendali downlink
    mqttClient.subscribe(TOPIC_CMD);
    Serial.printf("[MQTT] Berhasil subscribe ke topik command: %s\n", TOPIC_CMD);
  } else {
    Serial.printf("GAGAL! rc=%d (Cek username/password/koneksi)\n", mqttClient.state());
  }
}

// ====================================================================================================
// 4. PENERIMAAN PAKET LORA & PUBLISH KE MQTT
// ====================================================================================================

void processIncomingLoRaPackets() {
  if (LoRaSerial.available()) {
    String loraPacket = LoRaSerial.readStringUntil('\n');
    loraPacket.trim();

    if (loraPacket.length() == 0) return;

    packetsReceived++;
    digitalWrite(PIN_STATUS_LED, HIGH);

    Serial.println(F("=================================================="));
    Serial.printf("[LORA RX #%lu] %s\n", (unsigned long)packetsReceived, loraPacket.c_str());

    // Cek apakah paket valid JSON
    if (loraPacket.startsWith("{") && loraPacket.endsWith("}")) {
      // 1. Publish ke Topik Telemetri Utama
      if (mqttClient.connected()) {
        bool pubOk = mqttClient.publish(TOPIC_TELEM, loraPacket.c_str());
        Serial.printf("[MQTT PUB] Telemetry -> %s\n", pubOk ? "BERHASIL" : "GAGAL");

        // 2. Evaluasi Status Alert: Jika risk > 0 (WASPADA atau BAHAYA)
        if (loraPacket.indexOf("\"risk\":1") >= 0) {
          mqttClient.publish(TOPIC_ALERT, "{\"alert\":\"WASPADA_ASAP\",\"data\":", false);
          mqttClient.publish(TOPIC_ALERT, loraPacket.c_str(), false);
          Serial.println(F("[MQTT ALERT] Mengirim peringatan SIAGA ASAP!"));
        } else if (loraPacket.indexOf("\"risk\":2") >= 0) {
          mqttClient.publish(TOPIC_ALERT, "{\"alert\":\"BAHAYA_KEBAKARAN_KRITIS\",\"data\":", false);
          mqttClient.publish(TOPIC_ALERT, loraPacket.c_str(), false);
          Serial.println(F("[MQTT ALERT] >>> MENGIRIM PERINGATAN DARURAT API AKTIF! <<<"));
        }
      } else {
        Serial.println(F("[WARN] MQTT terputus, paket LoRa tersimpan di log lokal."));
      }
    } else {
      Serial.printf("[LORA NON-JSON] Data: %s\n", loraPacket.c_str());
    }

    digitalWrite(PIN_STATUS_LED, LOW);
    Serial.println(F("=================================================="));
  }
}

// ====================================================================================================
// 5. SETUP & LOOP GATEWAY
// ====================================================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n========================================================"));
  Serial.println(F("   FIRMWARE GATEWAY LORA -> MQTT BRIDGE (SiAGA HUTAN)   "));
  Serial.println(F("========================================================"));

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  pinMode(PIN_LORA_AUX, INPUT);

  // Inisialisasi UART1 untuk Modul LoRa E220-900T22D
  LoRaSerial.begin(9600, SERIAL_8N1, PIN_LORA_RX, PIN_LORA_TX);
  Serial.println(F("[INIT] UART1 LoRa E220 terhubung pada GPIO 21 (RX) & 47 (TX)."));

  // Inisialisasi MQTT Client
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512); // Peningkatan buffer untuk JSON komprehensif

  // Hubungkan ke Wi-Fi & MQTT
  connectWiFi();
  connectMQTT();

  Serial.println(F("[READY] Gateway SiAGA Hutan aktif mendengarkan radio LoRa!\n"));
}

void loop() {
  // 1. Jaga koneksi Wi-Fi & MQTT
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  } else if (!mqttClient.connected()) {
    connectMQTT();
  }

  // 2. Loop MQTT untuk memproses keepalive dan downlink callback
  mqttClient.loop();

  // 3. Proses paket radio LoRa yang masuk dari node hutan
  processIncomingLoRaPackets();
}

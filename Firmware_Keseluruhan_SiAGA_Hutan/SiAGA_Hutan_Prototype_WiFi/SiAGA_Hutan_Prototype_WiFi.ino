
#include <Adafruit_SHT31.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <driver/i2s.h>

const char *WIFI_SSID = "Mysaa_JSK";
const char *WIFI_PASSWORD = "12345678";

const char *MQTT_HOST = "0ea0f595ab254d60a011cbb996de055f.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char *MQTT_USER = "SiAGA_Hutan";
const char *MQTT_PASS = "2Z3ZLSE!fSVbqFi";

const char *DEVICE_ID = "SIAGA-HUTAN-PROTOTYPE-01";
const char *TOPIC_TELEM =
    "iot/siaga_hutan/telemetry"; // Uplink data telemetri reguler
const char *TOPIC_ALERT =
    "iot/siaga_hutan/alert"; // Uplink peringatan bahaya kebakaran
const char *TOPIC_STATUS =
    "iot/siaga_hutan/status"; // Status Online/Offline (LWT)
const char *TOPIC_CMD =
    "iot/siaga_hutan/command"; // Downlink kendali manual dari dashboard

// 2. DEFINISI PINOUT PERANGKAT KERAS (ESP32-S3 N16R8)

// --- Sensor Analog (Wajib di ADC1 agar TIDAK bentrok dengan Wi-Fi) ---
#define PIN_MQ2_ADC 1 // GPIO 1  - ADC1_CH0 (Pembagi tegangan 10k/20k + C 100nF)
#define PIN_SHARP_ADC                                                          \
  2 // GPIO 2  - ADC1_CH1 (Pembagi tegangan 4.7k/10k, TANPA KAPASITOR)

// --- Bus TWI / I2C (SHT31) ---
#define PIN_I2C_SDA 8       // GPIO 8  - I2C Data (Pull-up 4.7k ke 3.3V)
#define PIN_I2C_SCL 9       // GPIO 9  - I2C Clock (Pull-up 4.7k ke 3.3V)
#define SHT31_I2C_ADDR 0x44 // Alamat I2C default SHT31

// --- Aktuator & Pemicu Sensor ---
#define PIN_SHARP_LED 14 // GPIO 14 - Pulsa IRED GP2Y1010 (Aktif LOW)
#define PIN_SERVO_PAN 42 // GPIO 42 - PWM Sinyal Kendali Sudut Servo SG90

// --- USART2 (Komunikasi ke ESP32-CAM) ---
#define PIN_CAM_TX 16 // GPIO 16 - ESP32-S3 TX2 -> ESP32-CAM U0RXD
#define PIN_CAM_RX 17 // GPIO 17 - ESP32-S3 RX2 <- ESP32-CAM U0TXD
#define CAM_BAUDRATE 115200

// --- I2S DMA Audio (Mikrofon MEMS Digital INMP441) ---
#define PIN_I2S_SCK 39 // GPIO 39 - I2S Bit Clock (BCLK)
#define PIN_I2S_WS 40  // GPIO 40 - I2S Word Select (LRCLK)
#define PIN_I2S_SD 41  // GPIO 41 - I2S Serial Data (DOUT)
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE 16000
#define I2S_BUFFER_LEN 256

// 3. PARAMETER KALIBRASI, TIMING & RASIO PEMBAGI TEGANGAN

// Rasio Pembagi Tegangan: Vadc / Vsensor
const float SHARP_DIV_RATIO = 0.6803f; // 10k / (4.7k + 10k)
const float MQ2_DIV_RATIO = 0.6667f;   // 20k / (10k + 20k)

// Timing Sensor Debu Sharp GP2Y1010AU0F
const uint32_t SHARP_PERIOD_US = 10000; // Siklus 10 ms (100 Hz)
const uint32_t SHARP_SAMPLE_US =
    280; // Jendela sampling ADC tepat 280 µs setelah LED ON
const uint32_t SHARP_PULSE_US = 320; // Lebar pulsa LED 320 µs
const uint8_t SHARP_AVG_COUNT = 30;  // Rata-rata 30 sampel (~300 ms)
float sharpV0 = 0.60f;               // Tegangan dasar udara bersih (Volt)

// Sensor Gas MQ-2
const float MQ2_VCC = 5.0f;
const float MQ2_RL_OHM = 5000.0f;
const uint32_t MQ2_WARMUP_MS = 20000; // Pemanasan awal 20 detik
const uint32_t MQ2_PERIOD_MS = 50;

// Ambang Batas Logika Risiko Kebakaran (Fire Risk Rule-Engine)
float THRESH_TEMP_WARN = 30.0f; // °C
float THRESH_TEMP_DANGER = 50.0f;
float THRESH_HUM_DRY = 30.0f;   // %RH
float THRESH_DUST_WARN = 0.15f; // mg/m³
float THRESH_DUST_DANGER = 0.35f;
float THRESH_GAS_WARN = 1.50f; // Volt MQ-2
float THRESH_GAS_DANGER = 2.60f;
float THRESH_ACOUSTIC_RMS = 2500.0f; // RMS desis api / gergaji mesin

// Interval Pengiriman Data
const uint32_t INTERVAL_PUB_NORMAL =
    5000; // Kirim tiap 5 detik jika kondisi normal
const uint32_t INTERVAL_PUB_ALERT =
    1500; // Kirim tiap 1.5 detik jika waspada / bahaya

// 4. STRUKTUR DATA, OBJEK & ENUM STATUS SISTEM

// Definisi Level Risiko Kebakaran
enum FireRiskLevel { RISK_NORMAL = 0, RISK_WASPADA = 1, RISK_BAHAYA = 2 };

// Mesin Status Sensor Debu
enum SharpState { SHARP_IDLE, SHARP_LED_ON, SHARP_WAIT_OFF };

// Wadah Data Telemetri Terpadu
struct TelemetryData {
  float temperature;  // Suhu SHT31 (°C)
  float humidity;     // Kelembapan SHT31 (%RH)
  float dustMgM3;     // GP2Y1010 (mg/m³)
  float dustVoSensor; // Tegangan output sensor debu (V)
  float gasVolt;      // Tegangan output sensor MQ-2 (V)
  float gasRsKohm;    // Resistansi sensor MQ-2 (kΩ)
  float acousticRms;  // Energi RMS mikrofon MEMS INMP441
  uint8_t servoAngle; // Sudut Servo SG90 (0 - 180 derajat)
  FireRiskLevel risk; // Tingkat ancaman kebakaran
  bool camAck;        // Status konfirmasi kamera
  uint32_t packetSeq; // Nomor urut paket
};

// Objek-Objek Global
Adafruit_SHT31 sht31;
Servo servoPan;
HardwareSerial SerialCam(2); // UART2 Hardware ESP32-S3
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// Variabel Pewaktu & Penampung Data
TelemetryData telemetry;
SharpState sharpState = SHARP_IDLE;
uint32_t sharpCycleStartUs = 0;
uint32_t sharpSumMv = 0;
uint8_t sharpCount = 0;
uint32_t sharpSampleMv = 0;

uint32_t mq2LastMs = 0;
uint32_t mq2SumMv = 0;
uint8_t mq2Count = 0;

uint32_t lastPublishMs = 0;
uint32_t lastMqttRetryMs = 0;
uint32_t lastSerialLogMs = 0;
uint32_t packetCounter = 0;
bool sht31Available = false;

// 5. INISIALISASI & FUNGSI I2S DMA AUDIO (INMP441)

bool setupI2S() {
  i2s_config_t i2s_config = {.mode =
                                 (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                             .sample_rate = I2S_SAMPLE_RATE,
                             .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
                             .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                             .communication_format = I2S_COMM_FORMAT_STAND_I2S,
                             .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                             .dma_buf_count = 4,
                             .dma_buf_len = I2S_BUFFER_LEN,
                             .use_apll = false,
                             .tx_desc_auto_clear = false,
                             .fixed_mclk = 0};

  i2s_pin_config_t pin_config = {.bck_io_num = PIN_I2S_SCK,
                                 .ws_io_num = PIN_I2S_WS,
                                 .data_out_num = I2S_PIN_NO_CHANGE,
                                 .data_in_num = PIN_I2S_SD};

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK)
    return false;

  err = i2s_set_pin(I2S_PORT, &pin_config);
  return (err == ESP_OK);
}

float readAcousticRms() {
  int32_t sampleBuffer[I2S_BUFFER_LEN];
  size_t bytesRead = 0;

  esp_err_t result = i2s_read(I2S_PORT, &sampleBuffer, sizeof(sampleBuffer),
                              &bytesRead, 5 / portTICK_PERIOD_MS);
  if (result != ESP_OK || bytesRead == 0)
    return 0.0f;

  int samplesCount = bytesRead / sizeof(int32_t);
  int64_t sumSquares = 0;

  for (int i = 0; i < samplesCount; i++) {
    int32_t val = sampleBuffer[i] >> 14; // Buang LSB dan normalkan 24-bit MSB
    sumSquares += (int64_t)val * val;
  }

  if (samplesCount == 0)
    return 0.0f;
  return sqrt((double)sumSquares / samplesCount);
}

// 6. MESIN STATUS SENSOR DEBU, GAS MQ-2, & SHT31 (100% NON-BLOCKING)

// --- GP2Y1010AU0F State Machine (berbasis micros()) ---
void updateSharpDust() {
  uint32_t nowUs = micros();

  switch (sharpState) {
  case SHARP_IDLE:
    if (nowUs - sharpCycleStartUs >= SHARP_PERIOD_US) {
      digitalWrite(PIN_SHARP_LED, LOW); // LED IR ON (Aktif LOW)
      sharpCycleStartUs = nowUs;
      sharpState = SHARP_LED_ON;
    }
    break;

  case SHARP_LED_ON:
    if (nowUs - sharpCycleStartUs >= SHARP_SAMPLE_US) {
      sharpSampleMv = analogReadMilliVolts(PIN_SHARP_ADC);
      sharpState = SHARP_WAIT_OFF;
    }
    break;

  case SHARP_WAIT_OFF:
    if (nowUs - sharpCycleStartUs >= SHARP_PULSE_US) {
      digitalWrite(PIN_SHARP_LED, HIGH); // LED IR OFF
      sharpSumMv += sharpSampleMv;
      sharpCount++;

      if (sharpCount >= SHARP_AVG_COUNT) {
        float avgMv = (float)sharpSumMv / sharpCount;
        telemetry.dustVoSensor = (avgMv / 1000.0f) / SHARP_DIV_RATIO;

        // Karakteristik kurva Sharp: Densitas = (Vo - Vo_clean) * 0.17 mg/m³
        if (telemetry.dustVoSensor > sharpV0) {
          telemetry.dustMgM3 = (telemetry.dustVoSensor - sharpV0) * 0.17f;
        } else {
          telemetry.dustMgM3 = 0.0f;
        }
        sharpSumMv = 0;
        sharpCount = 0;
      }
      sharpState = SHARP_IDLE;
    }
    break;
  }
}

// --- MQ-2 Non-Blocking Averaging ---
void updateMQ2() {
  uint32_t nowMs = millis();
  if (nowMs - mq2LastMs < MQ2_PERIOD_MS)
    return;
  mq2LastMs = nowMs;

  mq2SumMv += analogReadMilliVolts(PIN_MQ2_ADC);
  mq2Count++;

  if (mq2Count >= 20) { // Rata-rata 20 sampel (~1 detik)
    float avgMv = (float)mq2SumMv / mq2Count;
    telemetry.gasVolt = (avgMv / 1000.0f) / MQ2_DIV_RATIO;

    if (telemetry.gasVolt > 0.05f && telemetry.gasVolt < MQ2_VCC) {
      telemetry.gasRsKohm =
          (MQ2_RL_OHM * (MQ2_VCC - telemetry.gasVolt) / telemetry.gasVolt) /
          1000.0f;
    } else {
      telemetry.gasRsKohm = 999.0f;
    }
    mq2SumMv = 0;
    mq2Count = 0;
  }
}

// --- SHT31 Suhu & Kelembapan ---
void readSHT31() {
  if (!sht31Available)
    return;
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    telemetry.temperature = t;
    telemetry.humidity = h;
  }
}

// 7. KONTROL AKTUATOR (SERVO PAN SG90)

void setServoAngle(uint8_t deg) {
  if (deg > 180)
    deg = 180;
  telemetry.servoAngle = deg;
  servoPan.write(deg);
}

// 8. USART KOMUNIKASI KE ESP32-CAM (UART2)

void triggerCameraCapture() {
  Serial.println(F("[CAMERA] Mengirim perintah jepretan 'SNAP_SHOT' ke "
                   "ESP32-CAM via UART2..."));
  SerialCam.println(F("SNAP_SHOT"));
}

void processCameraUART() {
  if (SerialCam.available()) {
    String resp = SerialCam.readStringUntil('\n');
    resp.trim();
    if (resp.length() > 0) {
      Serial.print(F("[CAM RESP] "));
      Serial.println(resp);

      // Publish hasil jepretan kamera ke topik MQTT Alert
      if (mqttClient.connected()) {
        char camPayload[128];
        snprintf(camPayload, sizeof(camPayload),
                 "{\"device\":\"%s\",\"type\":\"CAMERA_CAPTURE\",\"response\":"
                 "\"%s\"}",
                 DEVICE_ID, resp.c_str());
        mqttClient.publish(TOPIC_ALERT, camPayload, false);
      }

      if (resp.indexOf("ACK_OK") >= 0) {
        telemetry.camAck = true;
      }
    }
  }
}

// Fitur Uji Cepat via Serial Monitor USB (Ketik langsung di PC)
void processSerialCLI() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "TRIGGER_CAM" || cmd == "SNAP") {
      Serial.println(F("[CLI TEST] Menembak trigger SNAP_SHOT ke kamera..."));
      triggerCameraCapture();
    } else if (cmd == "FLASH_ON") {
      Serial.println(F("[CLI TEST] Menyalakan Flash LED Kamera..."));
      SerialCam.println(F("FLASH_ON"));
    } else if (cmd == "FLASH_OFF") {
      Serial.println(F("[CLI TEST] Mematikan Flash LED Kamera..."));
      SerialCam.println(F("FLASH_OFF"));
    } else if (cmd == "CAM_PING") {
      Serial.println(F("[CLI TEST] Mengirim PING ke Kamera..."));
      SerialCam.println(F("PING"));
    }
  }
}

// 9. LOGIKA SENSOR FUSION & CASCADING TRIGGER

void evaluateFireRisk() {
  FireRiskLevel currentRisk = RISK_NORMAL;

  // Syarat BAHAYA (CRITICAL API / HOTSPOT):
  if (telemetry.temperature >= THRESH_TEMP_DANGER ||
      (telemetry.dustMgM3 >= THRESH_DUST_DANGER &&
       telemetry.gasVolt >= THRESH_GAS_DANGER)) {
    currentRisk = RISK_BAHAYA;
  }
  // Syarat WASPADA (ASAP / PENINGKATAN SUHU / AKUSTIK ANOMALI):
  else if (telemetry.dustMgM3 >= THRESH_DUST_WARN ||
           telemetry.gasVolt >= THRESH_GAS_WARN ||
           (telemetry.temperature >= THRESH_TEMP_WARN) ||
           telemetry.acousticRms >= THRESH_ACOUSTIC_RMS) {
    currentRisk = RISK_WASPADA;
  }

  // Jika status berubah, lakukan tindakan cascading otomatis
  if (currentRisk != telemetry.risk) {
    telemetry.risk = currentRisk;
    Serial.printf("\n>>> PERUBAHAN TINGKAT RISIKO: [%s] <<<\n",
                  currentRisk == RISK_BAHAYA    ? "BAHAYA KEBAKARAN"
                  : currentRisk == RISK_WASPADA ? "WASPADA ASAP"
                                                : "NORMAL / AMAN");

    if (currentRisk == RISK_BAHAYA) {
      triggerCameraCapture(); // Ambil bukti foto
      setServoAngle(90);      // Arahkan servo ke 90°

      // Kirim pesan alert seketika via MQTT jika terhubung
      if (mqttClient.connected()) {
        char alertMsg[128];
        snprintf(alertMsg, sizeof(alertMsg),
                 "{\"alert\":\"BAHAYA_API\",\"t\":%.1f,\"d\":%.3f,\"g\":%.2f}",
                 telemetry.temperature, telemetry.dustMgM3, telemetry.gasVolt);
        mqttClient.publish(TOPIC_ALERT, alertMsg, false);
      }
    } else if (currentRisk == RISK_WASPADA) {
      setServoAngle(45); // Servo geser ke 45°
    } else {
      setServoAngle(0);
    }
  }
}

// 10. PROTOKOL MQTT (UPLINK TELEMETRI & DOWNLINK COMMAND)

// Callback: Menangani perintah masuk dari Dashboard / MQTT Client
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.println(F("=================================================="));
  Serial.printf("[MQTT DOWNLINK] Topik: %s | Pesan: %s\n", topic,
                message.c_str());

  if (String(topic) == TOPIC_CMD) {
    if (message.startsWith("SET_SERVO:")) {
      int angle = message.substring(10).toInt();
      setServoAngle(constrain(angle, 0, 180));
      Serial.printf("[CMD] Servo diarahkan ke: %d deg\n", angle);
    } else if (message == "TRIGGER_CAM") {
      triggerCameraCapture();
    } else if (message == "PING") {
      mqttClient.publish(TOPIC_STATUS,
                         "{\"response\":\"PONG\",\"status\":\"OK\"}");
    }
  }
  Serial.println(F("=================================================="));
}

// Koneksi Wi-Fi Non-Blocking
void ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED)
    return;

  static uint32_t lastWiFiAttemptMs = 0;
  uint32_t nowMs = millis();

  if (nowMs - lastWiFiAttemptMs >= 5000) { // Coba tiap 5 detik tanpa memblokir
    lastWiFiAttemptMs = nowMs;
    Serial.printf("[WIFI] Mencoba menyambung ke: %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// Koneksi MQTT Non-Blocking
void ensureMQTTConnected() {
  if (WiFi.status() != WL_CONNECTED || mqttClient.connected())
    return;

  uint32_t nowMs = millis();
  if (nowMs - lastMqttRetryMs < 5000)
    return; // Jeda retry 5 detik
  lastMqttRetryMs = nowMs;

  Serial.print(F("[MQTT] Menghubungkan ke broker HiveMQ Cloud... "));

  espClient.setInsecure(); // Abaikan validasi CA untuk kemudahan uji prototype

  // Last Will and Testament: Kirim pesan JSON "offline" jika koneksi terputus
  // tiba-tiba
  const char *willPayload =
      "{\"device\":\"SIAGA-HUTAN\",\"status\":\"offline\"}";

  String clientId = "ESP32S3-SIAGA-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS, TOPIC_STATUS,
                         1, true, willPayload)) {
    Serial.println(F("TERHUBUNG!"));

    // Publikasi status online (Retained)
    mqttClient.publish(TOPIC_STATUS,
                       "{\"device\":\"SIAGA-HUTAN\",\"status\":\"online\"}",
                       true);

    // Langganan (Subscribe) ke topik perintah dari Dashboard
    mqttClient.subscribe(TOPIC_CMD);
    mqttClient.subscribe("TRIGGER_CAM"); // <-- TAMBAHKAN BARIS INI
    Serial.printf("[MQTT] Berhasil subscribe ke: %s dan TRIGGER_CAM\n",
                  TOPIC_CMD);

  } else {
    Serial.printf("GAGAL, rc=%d. Mencoba lagi nanti...\n", mqttClient.state());
  }
}

// Publikasi Data Telemetri dalam Format JSON
void publishTelemetryJSON() {
  if (!mqttClient.connected())
    return;

  packetCounter++;
  telemetry.packetSeq = packetCounter;

  char payload[256];
  snprintf(
      payload, sizeof(payload),
      "{\"device\":\"%s\",\"seq\":%lu,\"temp\":%.2f,\"hum\":%.2f,\"dust\":%.3f,"
      "\"gas_v\":%.2f,\"gas_rs\":%.1f,\"rms\":%.0f,\"risk\":%d,\"servo\":%d}",
      DEVICE_ID, (unsigned long)telemetry.packetSeq, telemetry.temperature,
      telemetry.humidity, telemetry.dustMgM3, telemetry.gasVolt,
      telemetry.gasRsKohm, telemetry.acousticRms, (int)telemetry.risk,
      telemetry.servoAngle);

  mqttClient.publish(TOPIC_TELEM, payload);
  Serial.print(F("[MQTT UPLINK] "));
  Serial.println(payload);
}

// 11. SERIAL MONITOR DIAGNOSTIK

void printSerialLog() {
  uint32_t nowMs = millis();
  if (nowMs - lastSerialLogMs < 2000)
    return; // Cetak tiap 2 detik
  lastSerialLogMs = nowMs;

  Serial.println(F("-----------------------------------------------------------"
                   "---------------"));
  Serial.printf(" SiAGA Hutan [WIFI MODE] | Uptime: %lu s | Status: [%s]\n",
                nowMs / 1000,
                telemetry.risk == RISK_BAHAYA    ? "BAHAYA"
                : telemetry.risk == RISK_WASPADA ? "WASPADA"
                                                 : "NORMAL");
  Serial.printf(" SHT31 (I2C 0x44) : T=%.2f °C | RH=%.2f %%\n",
                telemetry.temperature, telemetry.humidity);
  Serial.printf(" GP2Y1010 (ADC1_1): Debu=%.3f mg/m³ (Vo=%.3f V)\n",
                telemetry.dustMgM3, telemetry.dustVoSensor);
  Serial.printf(" MQ-2 (ADC1_0)    : Gas=%.2f V | Rs=%.1f kΩ %s\n",
                telemetry.gasVolt, telemetry.gasRsKohm,
                (nowMs >= MQ2_WARMUP_MS) ? "[SIAP]" : "[WARMUP]");
  Serial.printf(" INMP441 (I2S DMA): Audio RMS=%.1f\n", telemetry.acousticRms);
  Serial.printf(" Aktuator & Node  : Servo=%d° | CamACK=%s\n",
                telemetry.servoAngle, telemetry.camAck ? "YES" : "NO");
  Serial.printf(" Wi-Fi & MQTT     : WiFi=%s (%s) | MQTT=%s\n",
                (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "DISCONNECTED",
                WiFi.localIP().toString().c_str(),
                mqttClient.connected() ? "CONNECTED" : "DISCONNECTED");
  Serial.println(F("-----------------------------------------------------------"
                   "---------------\n"));
}

// 12. SETUP & LOOP UTAMA

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(
      F("\n================================================================="));
  Serial.println(
      F("   FIRMWARE PROTOTYPE STANDALONE WI-FI & MQTT - SiAGA HUTAN      "));
  Serial.println(
      F("================================================================="));

  // 1. Inisialisasi ADC1 ESP32-S3 (Aman dipakai bersama Wi-Fi)
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MQ2_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_SHARP_ADC, ADC_11db);

  // 2. Pin Pulsa Sensor Debu Sharp
  pinMode(PIN_SHARP_LED, OUTPUT);
  digitalWrite(PIN_SHARP_LED, HIGH); // Standby OFF (Aktif LOW)

  // 3. Inisialisasi TWI / Bus I2C & SHT31
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  if (sht31.begin(SHT31_I2C_ADDR)) {
    sht31Available = true;
    Serial.println(F("[INIT] Sensor SHT31 terdeteksi pada alamat 0x44."));
  } else {
    sht31Available = false;
    Serial.println(
        F("[WARN] SHT31 TIDAK ditemukan! Cek koneksi GPIO 8 (SDA) & 9 (SCL)."));
  }

  // 4. Inisialisasi Servo Pan SG90 via ESP32PWM Timer
  ESP32PWM::allocateTimer(1);
  servoPan.setPeriodHertz(50); // 50 Hz standar servo
  servoPan.attach(PIN_SERVO_PAN, 500, 2400);
  setServoAngle(0);

  // 6. Inisialisasi Mikrofon Digital MEMS INMP441 via I2S DMA
  if (setupI2S()) {
    Serial.println(F("[INIT] Driver I2S DMA INMP441 berhasil diinisialisasi."));
  } else {
    Serial.println(F("[ERROR] Gagal inisialisasi I2S audio INMP441!"));
  }

  // 7. Inisialisasi USART2 untuk ESP32-CAM (GPIO 17 & 18)
  SerialCam.begin(CAM_BAUDRATE, SERIAL_8N1, PIN_CAM_RX, PIN_CAM_TX);
  Serial.println(F("[INIT] UART2 ESP32-CAM aktif (115200 baud)."));

  // 8. Inisialisasi Klien MQTT
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);

  // Mulai koneksi Wi-Fi awal
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Nilai awal variabel telemetri
  telemetry.temperature = 28.0f;
  telemetry.humidity = 70.0f;
  telemetry.dustMgM3 = 0.0f;
  telemetry.gasVolt = 0.0f;
  telemetry.risk = RISK_NORMAL;
  telemetry.camAck = false;

  Serial.println(F("[READY] Seluruh modul & sensor prototipe aktif!\n"));
}

void loop() {
  // A. Pembacaan Pulsa Sensor Debu Berbasis micros() (Prioritas Utama,
  // Non-Blocking)
  updateSharpDust();

  // B. Eksekusi Tugas Lain Saat Pulsa Sensor Debu Sedang IDLE
  if (sharpState == SHARP_IDLE) {
    updateMQ2();
    readSHT31();
    telemetry.acousticRms = readAcousticRms();

    // C. Evaluasi Engine Deteksi Kebakaran & Cascading Trigger
    evaluateFireRisk();

    // D. Tangani Komunikasi Serial dengan ESP32-CAM & CLI Tester
    processCameraUART();
    processSerialCLI();

    // E. Jaga Koneksi Wi-Fi & MQTT (Otomatis Reconnect Non-Blocking)
    ensureWiFiConnected();
    ensureMQTTConnected();
    mqttClient.loop(); // Memproses keepalive dan downlink callback MQTT

    // F. Pengiriman Data Telemetri Berkala ke Cloud MQTT
    uint32_t nowMs = millis();
    uint32_t currentInterval = (telemetry.risk == RISK_NORMAL)
                                   ? INTERVAL_PUB_NORMAL
                                   : INTERVAL_PUB_ALERT;

    if (nowMs - lastPublishMs >= currentInterval) {
      lastPublishMs = nowMs;
      publishTelemetryJSON();
    }

    // G. Tampilkan Log Diagnostik ke Serial Monitor PC
    printSerialLog();
  }
}

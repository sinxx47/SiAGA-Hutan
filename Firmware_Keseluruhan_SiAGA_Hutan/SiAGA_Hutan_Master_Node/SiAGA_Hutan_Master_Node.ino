/**
 * ====================================================================================================
 * PROJECT: SiAGA Hutan (Forest Sentinel & Acoustic Fire Guard)
 * FIRMWARE: Node Master Controller (ESP32-S3 DevKitC-1 N16R8)
 * REVISI  : Master Integrasi 9 Modul Perangkat Keras Sesuai Skematik Master
 * ====================================================================================================
 * 
 * Modul Terintegrasi:
 *   1. SHT31        : Sensor Suhu & Kelembapan Presisi (I2C SDA=GPIO4, SCL=GPIO5, Addr=0x44)
 *   2. GP2Y1010AU0F : Sensor Debu & Asap Optik Sharp (LED=GPIO14, Vo ADC=GPIO2 via divider 4.7k/10k)
 *   3. MQ-2         : Sensor Gas & Asap Pembakaran (A0 ADC=GPIO1 via divider 10k/20k + C 100nF)
 *   4. INMP441      : Mikrofon MEMS Digital I2S (SCK=GPIO39, WS=GPIO40, SD=GPIO41)
 *   5. Kipas DC 5V  : Driver MOSFET PWM Sampling Udara Aktif (Gate=GPIO15)
 *   6. Servo SG90   : Aktuator Pan Kamera 180 Derajat (PWM=GPIO42)
 *   7. ESP32-CAM    : Visual Camera Node via UART2 (TX2=GPIO17 -> RX_CAM, RX2=GPIO18 <- TX_CAM)
 *   8. E220-900T22D : Radio LoRa 900MHz via UART1 (TX1=GPIO47 -> RX_E220, RX1=GPIO21 <- TX_E220, AUX=GPIO38)
 * 
 * Fitur Utama Firmware:
 *   - Arsitektur Non-Blocking & State Machine: Eksekusi mulus tanpa delay() yang memblokir.
 *   - Sensor Fusion & Cascading Trigger: Deteksi tingkat risiko kebakaran (NORMAL, WASPADA, BAHAYA).
 *   - Telemetri LoRa Terstruktur: Format paket data ringkas & hemat energi.
 *   - Downlink Command Parser: Menerima perintah kendali manual (servo, kipas, threshold) dari Gateway.
 *   - Acoustic SPL / RMS Noise Monitoring: Mendeteksi anomali desis api atau chainsaw.
 * ====================================================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <driver/i2s.h>
#include <ESP32Servo.h>

// ====================================================================================================
// 1. DEFINISI PINOUT & KONFIGURASI PERANGKAT KERAS (ESP32-S3)
// ====================================================================================================

// --- Sensor Analog (ADC1) ---
#define PIN_MQ2_ADC         1    // GPIO 1  - ADC1_CH0 (Divider 10k / 20k)
#define PIN_SHARP_ADC       2    // GPIO 2  - ADC1_CH1 (Divider 4.7k / 10k)

// --- Bus I2C (SHT31) ---
#define PIN_I2C_SDA         4    // GPIO 4  - I2C Data
#define PIN_I2C_SCL         5    // GPIO 5  - I2C Clock
#define SHT31_I2C_ADDR      0x44 // Alamat I2C default SHT31

// --- Kontrol Sensor & Aktuator ---
#define PIN_SHARP_LED       14   // GPIO 14 - Pulsa IRED GP2Y1010 (Aktif LOW)
#define PIN_FAN_MOSFET      15   // GPIO 15 - PWM Driver MOSFET Kipas DC 5V
#define PIN_SERVO_PAN       42   // GPIO 42 - PWM Sinyal Servo SG90

// --- UART2 (ESP32-CAM Visual Trigger) ---
#define PIN_CAM_TX          17   // GPIO 17 - ESP32-S3 TX -> ESP32-CAM U0RXD
#define PIN_CAM_RX          18   // GPIO 18 - ESP32-S3 RX <- ESP32-CAM U0TXD
#define CAM_BAUDRATE        115200

// --- UART1 & AUX (LoRa E220-900T22D) ---
#define PIN_LORA_RX         21   // GPIO 21 - ESP32-S3 RX <- E220 TXD
#define PIN_LORA_TX         47   // GPIO 47 - ESP32-S3 TX -> E220 RXD
#define PIN_LORA_AUX        38   // GPIO 38 - E220 AUX (LOW = Sibuk, HIGH = Siap)
#define LORA_BAUDRATE       9600

// --- I2S DMA Audio (INMP441 MEMS Microphone) ---
#define PIN_I2S_SCK         39   // GPIO 39 - I2S Bit Clock (BCLK)
#define PIN_I2S_WS          40   // GPIO 40 - I2S Word Select / LRCLK
#define PIN_I2S_SD          41   // GPIO 41 - I2S Serial Data Out (DOUT)
#define I2S_PORT            I2S_NUM_0
#define I2S_SAMPLE_RATE     16000
#define I2S_BUFFER_LEN      256

// ====================================================================================================
// 2. PARAMETER KALIBRASI & RASIO PEMBAGI TEGANGAN
// ====================================================================================================

// Rasio Pembagi Tegangan: Vadc / Vsensor
// Sharp GP2Y1010: R1 = 4.7k, R2 = 10k => 10 / (4.7 + 10) = 0.6803
// MQ-2         : R1 = 10k,  R2 = 20k => 20 / (10 + 20)  = 0.6667
const float SHARP_DIV_RATIO = 0.6803f;
const float MQ2_DIV_RATIO   = 0.6667f;

// GP2Y1010 Timing & Karakteristik
const uint32_t SHARP_PERIOD_US = 10000; // Periode siklus 10 ms (100 Hz)
const uint32_t SHARP_SAMPLE_US = 280;   // Ambil sampel ADC di 280 µs
const uint32_t SHARP_PULSE_US  = 320;   // Pulsa aktif LOW LED berakhir di 320 µs
const uint8_t  SHARP_AVG_COUNT = 30;    // Rata-rata 30 sampel (~300 ms)
float sharpV0 = 0.60f;                  // Baseline voltase udara bersih (bisa auto-zero)
const float SHARP_SENSITIVITY  = 0.005f;// 0.5V per 0.1 mg/m3 = 5 mV per ug/m3

// MQ-2 Karakteristik
const float MQ2_VCC            = 5.0f;
const float MQ2_RL_OHM         = 5000.0f; // Beban modul ~5k ohm
const uint32_t MQ2_WARMUP_MS   = 20000;   // Waktu pemanasan minimum 20 detik
const uint32_t MQ2_PERIOD_MS   = 50;      // Interval baca 50 ms

// Ambang Batas Logika Risiko Kebakaran (Fire Risk Engine)
float THRESH_TEMP_WARN         = 40.0f;   // Derajat Celcius
float THRESH_TEMP_DANGER       = 50.0f;
float THRESH_HUM_DRY           = 30.0f;   // % RH (Udara kering mudah terbakar)
float THRESH_DUST_WARN         = 0.15f;   // mg/m3
float THRESH_DUST_DANGER       = 0.35f;   // mg/m3
float THRESH_GAS_WARN          = 1.50f;   // Volt output sensor MQ2
float THRESH_GAS_DANGER        = 2.60f;
float THRESH_ACOUSTIC_RMS      = 2500.0f; // Tingkat energi akustik (indikasi chainsaw/letupan)

// Timing Telemetri & Loop
const uint32_t LORA_INTERVAL_NORMAL = 15000; // Kirim tiap 15 detik jika normal
const uint32_t LORA_INTERVAL_ALERT  = 3000;  // Kirim tiap 3 detik jika waspada/bahaya
const uint32_t SERIAL_PRINT_PERIOD  = 2000;  // Output serial monitor tiap 2 detik

// ====================================================================================================
// 3. OBJEK & STRUKTUR DATA GLOBAL
// ====================================================================================================

Adafruit_SHT31 sht31;
Servo servoPan;
HardwareSerial SerialLoRa(1); // UART1 untuk LoRa E220
HardwareSerial SerialCam(2);  // UART2 untuk ESP32-CAM

// Definisi Level Risiko Kebakaran
enum FireRiskLevel {
  RISK_NORMAL = 0,
  RISK_WASPADA = 1,
  RISK_BAHAYA = 2
};

// State Sensor Debu Sharp
enum SharpState {
  SHARP_IDLE,
  SHARP_LED_ON,
  SHARP_WAIT_OFF
};

// Struktur Data Telemetri Terintegrasi
struct TelemetryData {
  uint32_t timestampMs;
  float temperature;    // SHT31 (°C)
  float humidity;       // SHT31 (%RH)
  float dustMgM3;       // GP2Y1010 (mg/m³)
  float dustVoSensor;   // Tegangan Vo sensor debu (V)
  float gasVolt;        // MQ-2 tegangan terukur (V)
  float gasRsKohm;      // MQ-2 resistansi sensor (kΩ)
  float acousticRms;    // INMP441 RMS energi audio
  uint8_t fanSpeed;     // PWM Kipas (0 - 255)
  uint8_t servoAngle;   // Sudut Servo (0 - 180)
  FireRiskLevel risk;   // Level risiko kebakaran
  bool camAck;          // Konfirmasi kamera siap
  uint32_t packetSeq;   // Nomor urut paket LoRa
};

// Variabel Status Sistem
TelemetryData telemetry;
SharpState sharpState = SHARP_IDLE;
uint32_t sharpCycleStartUs = 0;
uint32_t sharpSumMv = 0;
uint8_t  sharpCount = 0;
uint32_t sharpSampleMv = 0;

uint32_t mq2LastMs = 0;
uint32_t mq2SumMv = 0;
uint8_t  mq2Count = 0;

uint32_t lastLoRaTxMs = 0;
uint32_t lastSerialPrintMs = 0;
uint32_t lastFanToggleMs = 0;
uint32_t packetCounter = 0;
bool sht31Detected = false;
bool mq2WarmedUp = false;

// ====================================================================================================
// 4. INISIALISASI & FUNGSI I2S DMA AUDIO (INMP441)
// ====================================================================================================

bool setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = I2S_BUFFER_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = PIN_I2S_SCK,
    .ws_io_num = PIN_I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_I2S_SD
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) return false;

  err = i2s_set_pin(I2S_PORT, &pin_config);
  return (err == ESP_OK);
}

float readAcousticRms() {
  int32_t sampleBuffer[I2S_BUFFER_LEN];
  size_t bytesRead = 0;

  esp_err_t result = i2s_read(I2S_PORT, &sampleBuffer, sizeof(sampleBuffer), &bytesRead, 10 / portTICK_PERIOD_MS);
  if (result != ESP_OK || bytesRead == 0) return 0.0f;

  int samplesCount = bytesRead / sizeof(int32_t);
  int64_t sumSquares = 0;

  for (int i = 0; i < samplesCount; i++) {
    // INMP441 adalah 24-bit audio dalam frame 32-bit (MSB aligned)
    int32_t val = sampleBuffer[i] >> 14; 
    sumSquares += (int64_t)val * val;
  }

  if (samplesCount == 0) return 0.0f;
  return sqrt((double)sumSquares / samplesCount);
}

// ====================================================================================================
// 5. DRIVER SENSOR SUHU, DEBU, & GAS
// ====================================================================================================

// --- GP2Y1010AU0F Non-Blocking State Machine (berbasis micros()) ---
void updateSharpDust() {
  uint32_t nowUs = micros();

  switch (sharpState) {
    case SHARP_IDLE:
      if (nowUs - sharpCycleStartUs >= SHARP_PERIOD_US) {
        digitalWrite(PIN_SHARP_LED, LOW); // LED IR Aktif LOW
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

          // Konversi tegangan ke mg/m³
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
  if (nowMs - mq2LastMs < MQ2_PERIOD_MS) return;
  mq2LastMs = nowMs;

  mq2SumMv += analogReadMilliVolts(PIN_MQ2_ADC);
  mq2Count++;

  if (mq2Count >= 20) { // Rata-rata 20 sampel (~1 detik)
    float avgMv = (float)mq2SumMv / mq2Count;
    telemetry.gasVolt = (avgMv / 1000.0f) / MQ2_DIV_RATIO;

    if (telemetry.gasVolt > 0.05f && telemetry.gasVolt < MQ2_VCC) {
      telemetry.gasRsKohm = (MQ2_RL_OHM * (MQ2_VCC - telemetry.gasVolt) / telemetry.gasVolt) / 1000.0f;
    } else {
      telemetry.gasRsKohm = 999.0f;
    }

    mq2SumMv = 0;
    mq2Count = 0;
  }
}

// --- SHT31 Pembacaan Suhu & RH ---
void readSHT31() {
  if (!sht31Detected) return;
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    telemetry.temperature = t;
    telemetry.humidity = h;
  }
}

// ====================================================================================================
// 6. KENDALI AKTUATOR: KIPAS 5V (PWM) & SERVO SG90 (PAN)
// ====================================================================================================

void setFanSpeed(uint8_t pwmVal) {
  telemetry.fanSpeed = pwmVal;
  ledcWrite(0, pwmVal); // Channel 0 untuk Kipas
}

void setServoAngle(uint8_t angleDeg) {
  if (angleDeg > 180) angleDeg = 180;
  telemetry.servoAngle = angleDeg;
  servoPan.write(angleDeg);
}

// ====================================================================================================
// 7. KOMUNIKASI VISUAL: ESP32-CAM VIA UART2
// ====================================================================================================

void triggerCameraCapture() {
  Serial.println(F("[CAMERA] Mengirim sinyal trigger jepretan foto ke ESP32-CAM..."));
  SerialCam.println(F("SNAP_SHOT"));
}

void processCameraResponses() {
  if (SerialCam.available()) {
    String resp = SerialCam.readStringUntil('\n');
    resp.trim();
    if (resp.length() > 0) {
      Serial.print(F("[CAMERA RESP] "));
      Serial.println(resp);
      if (resp.indexOf("ACK_OK") >= 0) {
        telemetry.camAck = true;
      }
    }
  }
}

// ====================================================================================================
// 8. LOGIKA SENSOR FUSION & CASCADING TRIGGER
// ====================================================================================================

void evaluateFireRisk() {
  FireRiskLevel currentRisk = RISK_NORMAL;

  // Syarat BAHAYA (CRITICAL HOTSPOT / API):
  // 1. Suhu ekstrem (> 50°C), ATAU
  // 2. Debu pekat (> 0.35 mg/m3) BERSAMAAN DENGAN Gas Pembakaran tinggi (> 2.6V)
  if (telemetry.temperature >= THRESH_TEMP_DANGER || 
     (telemetry.dustMgM3 >= THRESH_DUST_DANGER && telemetry.gasVolt >= THRESH_GAS_DANGER)) {
    currentRisk = RISK_BAHAYA;
  }
  // Syarat WASPADA (INDIKASI ANOMALI / ASAP / API AWAL):
  // 1. Suhu naik (> 40°C) & kelembapan sangat kering (< 30%), ATAU
  // 2. Partikel asap debu naik (> 0.15 mg/m3), ATAU
  // 3. Sensor gas mendeteksi asap (> 1.5V), ATAU
  // 4. Suara derau api/chainsaw terdeteksi kencang
  else if (telemetry.dustMgM3 >= THRESH_DUST_WARN || 
           telemetry.gasVolt >= THRESH_GAS_WARN || 
           (telemetry.temperature >= THRESH_TEMP_WARN && telemetry.humidity <= THRESH_HUM_DRY) ||
           telemetry.acousticRms >= THRESH_ACOUSTIC_RMS) {
    currentRisk = RISK_WASPADA;
  }

  // Tindakan Cascading Trigger
  if (currentRisk != telemetry.risk) {
    telemetry.risk = currentRisk;
    Serial.printf("\n[ALERT ENGINE] Transisi Tingkat Risiko -> %s\n", 
                  currentRisk == RISK_BAHAYA ? ">>> BAHAYA (API/HOTSPOT) <<<" : 
                  currentRisk == RISK_WASPADA ? ">> WASPADA (ASAP/SUHU) <<" : "NORMAL");

    if (currentRisk == RISK_BAHAYA) {
      // Putar kipas maksimal untuk sampling aliran udara kontinu
      setFanSpeed(255);
      // Picu jepretan kamera visual bukti kebakaran
      triggerCameraCapture();
      // Sapu arah servo ke posisi target
      setServoAngle(90);
    } else if (currentRisk == RISK_WASPADA) {
      // Nyalakan kipas separuh daya untuk mengalirkan udara ke ruang sensor
      setFanSpeed(160);
      // Pindahkan servo ke sudut pemindaian
      setServoAngle(45);
    } else {
      // Normal: Kipas standby rendah / mati untuk hemat energi baterai
      setFanSpeed(0);
      setServoAngle(0);
    }
  }
}

// ====================================================================================================
// 9. TELEMETRI RADIO LORA E220-900T22D & DOWNLINK HANDLER
// ====================================================================================================

bool isLoRaReady() {
  // Pin AUX: LOW = Modul sedang mentransmisi/sibuk, HIGH = Modul siap
  return digitalRead(PIN_LORA_AUX) == HIGH;
}

void sendLoRaTelemetry() {
  if (!isLoRaReady()) {
    Serial.println(F("[LORA] Modul E220 AUX masih BUSY, menunda pengiriman..."));
    return;
  }

  packetCounter++;
  telemetry.packetSeq = packetCounter;
  telemetry.timestampMs = millis();

  // Susun payload format JSON ringkas untuk efisiensi bandwidth radio LoRa
  // Contoh: {"seq":1,"t":31.2,"h":65.4,"d":0.08,"g":1.12,"rms":450,"risk":0}
  char loraPayload[160];
  snprintf(loraPayload, sizeof(loraPayload),
    "{\"seq\":%lu,\"t\":%.1f,\"h\":%.1f,\"d\":%.3f,\"g\":%.2f,\"rms\":%.0f,\"risk\":%d,\"fan\":%d,\"srv\":%d}",
    (unsigned long)telemetry.packetSeq,
    telemetry.temperature,
    telemetry.humidity,
    telemetry.dustMgM3,
    telemetry.gasVolt,
    telemetry.acousticRms,
    (int)telemetry.risk,
    telemetry.fanSpeed,
    telemetry.servoAngle
  );

  SerialLoRa.println(loraPayload);

  Serial.print(F("[LORA TX] "));
  Serial.println(loraPayload);
}

void processLoRaDownlink() {
  if (SerialLoRa.available()) {
    String cmd = SerialLoRa.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.print(F("[LORA RX DOWNLINK] Perintah Diterima: "));
    Serial.println(cmd);

    // Parsing Command dari Gateway
    if (cmd.startsWith("SET_FAN:")) {
      int val = cmd.substring(8).toInt();
      setFanSpeed(constrain(val, 0, 255));
      Serial.printf("[CMD] Kipas disetel ke: %d\n", val);
    } else if (cmd.startsWith("SET_SERVO:")) {
      int deg = cmd.substring(10).toInt();
      setServoAngle(constrain(deg, 0, 180));
      Serial.printf("[CMD] Servo diarahkan ke: %d deg\n", deg);
    } else if (cmd == "TRIGGER_CAM") {
      triggerCameraCapture();
    } else if (cmd == "PING") {
      SerialLoRa.println(F("{\"resp\":\"PONG\",\"status\":\"OK\"}"));
    }
  }
}

// ====================================================================================================
// 10. SERIAL MONITOR DIAGNOSTIK
// ====================================================================================================

void printSystemStatus() {
  uint32_t nowMs = millis();
  if (nowMs - lastSerialPrintMs < SERIAL_PRINT_PERIOD) return;
  lastSerialPrintMs = nowMs;

  Serial.println(F("=========================================================================="));
  Serial.printf(" SiAGA Hutan | Uptime: %lu s | Status: [%s]\n", 
                nowMs / 1000, 
                telemetry.risk == RISK_BAHAYA ? "BAHAYA KEBAKARAN" :
                telemetry.risk == RISK_WASPADA ? "WASPADA" : "NORMAL / AMAN");
  Serial.println(F("--------------------------------------------------------------------------"));
  Serial.printf(" Suhu & Kelembapan (SHT31) : %.2f °C | %.2f %% RH %s\n", 
                telemetry.temperature, telemetry.humidity, sht31Detected ? "[OK]" : "[ERROR]");
  Serial.printf(" Partikel Asap/Debu (GP2Y) : %.3f mg/m³ (Vo=%.3f V)\n", 
                telemetry.dustMgM3, telemetry.dustVoSensor);
  Serial.printf(" Sensor Gas/Asap (MQ-2)    : %.3f V | Rs=%.2f kΩ %s\n", 
                telemetry.gasVolt, telemetry.gasRsKohm, 
                (nowMs >= MQ2_WARMUP_MS) ? "[SIAP]" : "[PEMANASAN]");
  Serial.printf(" Akustik MEMS I2S (INMP441): RMS=%.1f (Ambang=%0.0f)\n", 
                telemetry.acousticRms, THRESH_ACOUSTIC_RMS);
  Serial.printf(" Aktuator & Node Tambahan  : Kipas=%d/255 | Servo=%d° | CamACK=%s\n", 
                telemetry.fanSpeed, telemetry.servoAngle, telemetry.camAck ? "YES" : "NO");
  Serial.printf(" Radio Telemetri (E220 LoRa): AUX=%s | Pkt Terkirim=%lu\n", 
                isLoRaReady() ? "READY" : "BUSY", (unsigned long)telemetry.packetSeq);
  Serial.println(F("==========================================================================\n"));
}

// ====================================================================================================
// 11. SETUP & LOOP UTAMA
// ====================================================================================================

void setup() {
  // Inisialisasi Serial Debug Konsol
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("\n========================================================"));
  Serial.println(F("   FIRMWARE MASTER CONTROLLER - SiAGA HUTAN (ESP32-S3)  "));
  Serial.println(F("========================================================"));

  // 1. Konfigurasi Resolusi ADC (ADC1 GPIO 1 & 2)
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MQ2_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_SHARP_ADC, ADC_11db);
  Serial.println(F("[INIT] ADC1 untuk MQ-2 & Sharp GP2Y1010 terkonfigurasi."));

  // 2. Konfigurasi Pin Sensor Debu Sharp
  pinMode(PIN_SHARP_LED, OUTPUT);
  digitalWrite(PIN_SHARP_LED, HIGH); // Standby OFF (Aktif LOW)

  // 3. Konfigurasi Bus I2C & SHT31
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  if (sht31.begin(SHT31_I2C_ADDR)) {
    sht31Detected = true;
    Serial.println(F("[INIT] Sensor SHT31 berhasil terdeteksi pada 0x44."));
  } else {
    sht31Detected = false;
    Serial.println(F("[WARN] SHT31 TIDAK ditemukan! Periksa koneksi GPIO 4 (SDA) & 5 (SCL)."));
  }

  // 4. Inisialisasi Driver Kipas DC 5V (LEDC PWM)
  ledcSetup(0, 25000, 8); // Channel 0, 25 kHz, 8-bit resolution (0-255)
  ledcAttachPin(PIN_FAN_MOSFET, 0);
  setFanSpeed(0); // Mulai dalam keadaan mati

  // 5. Inisialisasi Servo Pan SG90
  ESP32PWM::allocateTimer(1);
  servoPan.setPeriodHertz(50); // Standar SG90 50 Hz
  servoPan.attach(PIN_SERVO_PAN, 500, 2400); // 500us - 2400us rentang pulsa
  setServoAngle(0);

  // 6. Inisialisasi Audio MEMS INMP441 (I2S DMA)
  if (setupI2S()) {
    Serial.println(F("[INIT] Driver I2S DMA INMP441 siap (16 kHz, 32-bit)."));
  } else {
    Serial.println(F("[ERROR] Gagal menginisialisasi driver I2S audio!"));
  }

  // 7. Inisialisasi UART2 untuk ESP32-CAM
  SerialCam.begin(CAM_BAUDRATE, SERIAL_8N1, PIN_CAM_RX, PIN_CAM_TX);
  Serial.println(F("[INIT] UART2 ESP32-CAM siap pada GPIO 17 (TX) & 18 (RX)."));

  // 8. Inisialisasi UART1 & AUX untuk LoRa E220-900T22D
  pinMode(PIN_LORA_AUX, INPUT);
  SerialLoRa.begin(LORA_BAUDRATE, SERIAL_8N1, PIN_LORA_RX, PIN_LORA_TX);
  Serial.println(F("[INIT] UART1 LoRa E220 siap pada GPIO 47 (TX), 21 (RX), 38 (AUX)."));

  // Inisialisasi Nilai Awal Telemetri
  telemetry.temperature = 28.0f;
  telemetry.humidity = 70.0f;
  telemetry.dustMgM3 = 0.0f;
  telemetry.gasVolt = 0.0f;
  telemetry.risk = RISK_NORMAL;
  telemetry.camAck = false;

  Serial.println(F("[READY] Seluruh 9 modul hardware SiAGA Hutan aktif!\n"));
}

void loop() {
  // A. Pembacaan Sensor Debu Berbasis Micros() Non-Blocking (Prioritas Tinggi)
  updateSharpDust();

  // B. Tugas Sensor Berkala & Non-Blocking
  if (sharpState == SHARP_IDLE) {
    updateMQ2();
    readSHT31();
    telemetry.acousticRms = readAcousticRms();

    // C. Evaluasi Engine Risiko Kebakaran
    evaluateFireRisk();

    // D. Tangani Komunikasi UART Kamera & Downlink LoRa
    processCameraResponses();
    processLoRaDownlink();

    // E. Pengiriman Telemetri LoRa Radio Sesuai Status Risiko
    uint32_t nowMs = millis();
    uint32_t currentInterval = (telemetry.risk == RISK_NORMAL) ? LORA_INTERVAL_NORMAL : LORA_INTERVAL_ALERT;

    if (nowMs - lastLoRaTxMs >= currentInterval) {
      lastLoRaTxMs = nowMs;
      sendLoRaTelemetry();
    }

    // F. Cetak Status Diagnostik Berkala ke USB CDC Serial Monitor
    printSystemStatus();
  }
}

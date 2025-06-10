#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <SinricProTemperaturesensor.h>

// Configuração de Wi-Fi primário
#define WIFI_SSID1        "CLYP_2G"
#define WIFI_PASSWORD1    "Clyp@2023"

// Configuração de Wi-Fi secundário
#define WIFI_SSID2        "PROXXIMA-CLYP_2G"
#define WIFI_PASSWORD2    "Clyp@2022"

// Configuração de Wi-Fi terciário
#define WIFI_SSID3        "ARGOS BANCADA_2G"
#define WIFI_PASSWORD3    "Argos172"

#define WIFI_TIMEOUT_MS   10000 // Tempo limite para conexão Wi-Fi (10 segundos)

#define BAUD_RATE         115200
#define APP_KEY    "519b2393-5138-4808-afed-6a36e9c12898"
#define APP_SECRET "601f2aef-806a-4cb4-b8ec-2cf483b62dc4-2820fd6d-a75f-4bf6-b79c-36f232cfcb68"
#define SWITCH_ID  "67bda66542d73b1d7c573a1c"
#define TEMP_SENSOR_ID    "67bda66542d73b1d7c573a1c"

#define DHT_PIN           21
#define RELAY_PIN         5

#define SENSOR_SEND_INTERVAL_MS 30000

DHT dht(DHT_PIN, DHT11);
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", -10800);
AsyncWebServer server(80);
bool wifiEnabled = true;

bool connectWiFi(const char* ssid, const char* password) {
  Serial.printf("Conectando ao Wi-Fi: %s...", ssid);
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado: " + WiFi.localIP().toString());
    return true;
  }
  Serial.println("\nFalha na conexão Wi-Fi!");
  return false;
}

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  dht.begin();

  if (!connectWiFi(WIFI_SSID1, WIFI_PASSWORD1)) {
    Serial.println("Tentando Wi-Fi secundário...");
    if (!connectWiFi(WIFI_SSID2, WIFI_PASSWORD2)) {
      Serial.println("Tentando Wi-Fi terciário...");
      if (!connectWiFi(WIFI_SSID3, WIFI_PASSWORD3)) {
        Serial.println("Nenhuma rede conectada. Reiniciando...");
        ESP.restart();
      }
    }
  }
  ntpClient.begin();
  server.begin();
  SinricProSwitch &mySwitch = SinricPro[SWITCH_ID];
  mySwitch.onPowerState([](const String &deviceId, bool &state) {
    Serial.printf("Alexa: Device %s turned %s\n", deviceId.c_str(), state ? "ON" : "OFF");
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    return true;
  });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void loop() {
  if (wifiEnabled) {
    SinricPro.handle();
  }
}

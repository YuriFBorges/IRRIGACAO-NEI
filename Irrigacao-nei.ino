#include <WiFi.h>          // Para conexão Wi-Fi
#include <WiFiClient.h>    // Cliente Wi-Fi
#include <Adafruit_MQTT.h> // Biblioteca principal Adafruit MQTT
#include <Adafruit_MQTT_Client.h> // Cliente MQTT
#include <DHT.h>           // Biblioteca para o sensor DHT
#include <DHT_U.h>         // Biblioteca unificada para o DHT

// --- Configurações Wi-Fi ---
#define WLAN_SSID       ""          // SSID da sua rede Wi-Fi
#define WLAN_PASS       ""        // Senha da sua rede Wi-Fi

// --- Configurações Adafruit IO ---
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // Porta MQTT padrão
#define AIO_USERNAME    ""          // Seu username Adafruit IO
#define AIO_KEY         "" // Sua chave Adafruit IO

// --- Definições de Pinos ---
#define SOIL_MOISTURE_PIN 34 // Pino ADC para o sensor de umidade do solo
#define DHT_PIN 23           // Pino para o sensor DHT22
#define LDR_PIN 32           // Pino ADC para o LDR
#define SOLENOID_PIN 21      // Pino para o módulo relé da solenoide

// --- Definições do DHT22 ---
#define DHTTYPE DHT22        // Define o tipo de sensor DHT (DHT22)
DHT dht(DHT_PIN, DHTTYPE);   // Inicializa o objeto DHT

// --- Variáveis de Limiares ---
// Valores de 0 a 4095 para o ADC do ESP32 (12 bits)
// Ajuste estes valores conforme a calibração do seu sensor e ambiente
const int UMIDADE_SOLO_MINIMA = 2000; // Se a leitura for MENOR que este valor, considera seco (precisa regar)
const int LUMINOSIDADE_MAXIMA = 1000; // Se a leitura do LDR for MENOR que este valor, considera escuro
const int TEMPERATURA_MINIMA_SOLENOIDE = 15; // Exemplo: solenoide só liga acima de 15°C
const int TEMPERATURA_MAXIMA_SOLENOIDE = 35; // Exemplo: solenoide só liga abaixo de 35°C

// --- Configuração MQTT ---
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// --- Feeds do Adafruit IO (crie estes feeds no seu dashboard Adafruit IO) ---
Adafruit_MQTT_Publish soilMoistureFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/umidade-solo");
Adafruit_MQTT_Publish temperatureFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperatura-ar");
Adafruit_MQTT_Publish humidityFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/umidade-ar");
Adafruit_MQTT_Publish luminosityFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/luminosidade");
Adafruit_MQTT_Publish solenoidStatusFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/solenoide-status");

// Feed para controle da solenoide (recebendo comandos do Adafruit IO)
Adafruit_MQTT_Subscribe solenoidControlFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/controle-solenoide");

// --- Funções Auxiliares ---
void connectToWiFi() {
  Serial.print("Conectando ao WiFi: ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void MQTT_connect() {
  int8_t ret;

  // Sai se já estiver conectado ao MQTT
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Conectando ao Adafruit IO... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // loop enquanto não conectar
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Tentando novamente em 5 segundos...");
    mqtt.disconnect();
    delay(5000); // espera 5 segundos
    retries--;
    if (retries == 0) {
      Serial.println("Falha na conexão MQTT após várias tentativas. Reiniciando ESP...");
      ESP.restart(); // Reinicia o ESP32 se não conseguir conectar
    }
  }
  Serial.println("Adafruit IO Conectado!");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando ESP32 com sensores e Adafruit IO...");

  connectToWiFi(); // Conecta ao Wi-Fi

  dht.begin(); // Inicia o sensor DHT

  pinMode(SOLENOID_PIN, OUTPUT);
  digitalWrite(SOLENOID_PIN, LOW); // Garante que a solenoide comece desligada (assumindo relé ativo em LOW)
                                  // Se seu relé for ativo em HIGH, mude para HIGH para desligar

  // Assina o feed de controle da solenoide
  mqtt.subscribe(&solenoidControlFeed);
}

void loop() {
  MQTT_connect(); // Garante que a conexão MQTT esteja ativa
  mqtt.processPackets(1000); // Processa pacotes MQTT para receber dados

  // --- Leitura do Sensor de Umidade do Solo ---
  int umidadeSolo = analogRead(SOIL_MOISTURE_PIN);
  Serial.print("Umidade do Solo (ADC): ");
  Serial.println(umidadeSolo);
  // Publica no Adafruit IO, verificando se a publicação foi bem-sucedida
  if (! soilMoistureFeed.publish(umidadeSolo)) {
    Serial.println("Falha ao publicar umidade do solo!");
  }

  // --- Leitura do DHT22 (Temperatura e Umidade do Ar) ---
  float umidadeAr = dht.readHumidity();
  float temperatura = dht.readTemperature();

  if (isnan(umidadeAr) || isnan(temperatura)) {
    Serial.println("Erro ao ler o sensor DHT!");
  } else {
    Serial.print("Umidade do Ar: ");
    Serial.print(umidadeAr);
    Serial.print(" %");
    Serial.print("  Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" °C");
    if (! humidityFeed.publish(umidadeAr)) { // Publica no Adafruit IO
      Serial.println("Falha ao publicar umidade do ar!");
    }
    if (! temperatureFeed.publish(temperatura)) { // Publica no Adafruit IO
      Serial.println("Falha ao publicar temperatura!");
    }
  }

  // --- Leitura do LDR ---
  int luminosidade = analogRead(LDR_PIN);
  Serial.print("Luminosidade (ADC): ");
  Serial.println(luminosidade);
  if (! luminosityFeed.publish(luminosidade)) { // Publica no Adafruit IO
    Serial.println("Falha ao publicar luminosidade!");
  }

  // --- Lógica de Acionamento da Solenoide (Automático) ---
  bool acionarSolenoide = false;
  if (umidadeSolo < UMIDADE_SOLO_MINIMA &&
      luminosidade > LUMINOSIDADE_MAXIMA &&
      temperatura >= TEMPERATURA_MINIMA_SOLENOIDE &&
      temperatura <= TEMPERATURA_MAXIMA_SOLENOIDE) {
    acionarSolenoide = true;
    Serial.println("Condições automáticas satisfeitas para acionar a solenoide.");
  } else {
    Serial.println("Condições automáticas NÃO satisfeitas para acionar a solenoide.");
  }

  // --- Verificação de Comando Manual do Adafruit IO ---
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) { // Verifica se há mensagens no feed de controle
    if (subscription == &solenoidControlFeed) {
      Serial.print("Comando de controle da solenoide recebido: ");
      Serial.println((char *)solenoidControlFeed.lastread);

      String command = (char *)solenoidControlFeed.lastread;
      if (command == "ON") {
        acionarSolenoide = true; // Força ligar a solenoide
        Serial.println("Solenoide acionada manualmente (ON).");
      } else if (command == "OFF") {
        acionarSolenoide = false; // Força desligar a solenoide
        Serial.println("Solenoide desligada manualmente (OFF).");
      }
    }
  }

  // --- Ação Final da Solenoide ---
  int currentSolenoidState = digitalRead(SOLENOID_PIN); // Lê o estado atual do pino
  int desiredSolenoidState = acionarSolenoide ? HIGH : LOW;

  if (currentSolenoidState != desiredSolenoidState) {
    digitalWrite(SOLENOID_PIN, desiredSolenoidState);
    if (desiredSolenoidState == HIGH) {
      Serial.println("Acionando Solenoide!");
      solenoidStatusFeed.publish(1); // Publica status "ligado"
    } else {
      Serial.println("Solenoide Desligada.");
      solenoidStatusFeed.publish(0); // Publica status "desligado"
    }
  } else {
    // Se o estado não mudou, apenas publica o estado atual para manter o feed atualizado
    solenoidStatusFeed.publish(currentSolenoidState == HIGH ? 1 : 0);
  }

  Serial.println("------------------------------------");
  delay(10000); // Espera 10 segundos antes da próxima leitura e envio
}
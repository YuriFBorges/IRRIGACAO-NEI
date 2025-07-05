#include <WiFi.h>                 // Para conexão Wi-Fi
#include <WiFiClient.h>           // Cliente Wi-Fi
#include <Adafruit_MQTT.h>        // Biblioteca principal Adafruit MQTT
#include <Adafruit_MQTT_Client.h> // Cliente MQTT
#include <DHT.h>                  // Biblioteca para sensor DHT
#include <AdafruitIO_WiFi.h>      // Adicionado para AdafruitIO_WiFi
#include <AdafruitIO.h>           // Biblioteca principal Adafruit IO

// ---------- CONFIGURAÇÃO DO ADAFRUIT IO ----------
#define IO_USERNAME  ""
#define IO_KEY       ""

// Wi-Fi
#define WIFI_SSID   ""
#define WIFI_PASS   ""

// ---------- SENSORES ----------
#define DHTTYPE DHT22
const int dhtPin = 27;
const int ldrPin = 34;
const int soilPin = 35;
DHT dht(dhtPin, DHTTYPE);

// ---------- RELE ----------
const int relePin = 5; // Pino GPIO do ESP32 conectado ao relé (Ex: GPIO5, D5)
                       

// ---------- CLIENTES E ADAFRUIT IO ----------
#define AIO_SERVER      ""
#define AIO_SERVERPORT   // Porta MQTT padrão

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ---------- FEEDS ----------
AdafruitIO_Feed *luminosidade = io.feed("luminosidade");
AdafruitIO_Feed *temperatura = io.feed("temperatura");
AdafruitIO_Feed *umidade_ar = io.feed("umidade_ar");
AdafruitIO_Feed *umidade_solo = io.feed("umidade_solo");

// *** NOVO: Feed para receber comandos do relé ***
AdafruitIO_Feed *rele_comando = io.feed("rele_comando");

// Variáveis para controle do relé
bool releAtivo = false;
unsigned long tempoInicioRele = 0;
unsigned long duracaoReleMs = 0; // Duração em milissegundos

// ---------- FUNÇÕES DE CALLBACK ----------

// *** NOVO: Função de callback para o feed 'rele_comando' ***
void handleReleCommand(AdafruitIO_Data *data) {
  String command = data->value(); // Ou data->stringValue(); para maior clareza

  Serial.print("Comando recebido para o relé: ");
  Serial.println(command);

  // Tenta converter o comando para um número (tempo em segundos)
  int tempoSegundos = command.toInt();

  if (tempoSegundos > 0) {
    duracaoReleMs = (unsigned long)tempoSegundos * 1000; // Converte segundos para milissegundos
    tempoInicioRele = millis(); // Registra o início do acionamento
    digitalWrite(relePin, HIGH); // Aciona o relé (HIGH para relé ativo em LOW, LOW para relé ativo em HIGH - depende do seu módulo)
                                 // Se seu relé for acionado com LOW, use: digitalWrite(relePin, LOW);
    releAtivo = true;
    Serial.printf("Relé acionado por %d segundos.\n", tempoSegundos);
  } else if (command == "0" || command.equalsIgnoreCase("off")) {
    digitalWrite(relePin, LOW); // Desliga o relé
    releAtivo = false;
    duracaoReleMs = 0; // Reseta a duração
    Serial.println("Comando para desligar o relé recebido. Relé desligado.");
  } else {
    Serial.println("Comando inválido para o relé. Envie um número (segundos) ou '0'/'off'.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configura o pino do relé como OUTPUT
  pinMode(relePin, OUTPUT);
  digitalWrite(relePin, LOW); // Garante que o relé esteja desligado no início

  // Inicializa sensores
  dht.begin();

  // Conecta ao Wi-Fi e Adafruit IO
  Serial.print("Conectando ao Adafruit IO...");
  io.connect();
  unsigned long startTime = millis();
  while (io.status() != AIO_CONNECTED && millis() - startTime < 10000) {
    Serial.print(".");
    delay(500);
  }

  if (io.status() != AIO_CONNECTED) {
    Serial.println("\nFalha ao conectar ao Adafruit IO!");
    while (true); // Para o programa (pode ser ajustado para retry)
  }
  Serial.println("\nConectado ao Adafruit IO!");

  // *** NOVO: Atribui a função de callback ao feed do relé ***
  rele_comando->onMessage(handleReleCommand);
}

void loop() {
  io.run(); // Mantém a conexão com Adafruit IO e processa mensagens recebidas

  // --- Lógica de controle do relé baseada no tempo ---
  if (releAtivo && millis() - tempoInicioRele >= duracaoReleMs) {
    digitalWrite(relePin, LOW); // Desliga o relé
    releAtivo = false;
    Serial.println("Tempo do relé esgotado. Relé desligado.");
  }

  // --- Leitura e envio de sensores (a cada 10 segundos) ---
  static unsigned long lastSensorReadTime = 0;
  const long sensorReadInterval = 10000; // 10 segundos

  if (millis() - lastSensorReadTime >= sensorReadInterval) {
    lastSensorReadTime = millis();

    // Lê sensores
    int valorLDR = analogRead(ldrPin);
    int porcentagemLuz = map(valorLDR, 0, 4095, 0, 100);

    float temp = dht.readTemperature();
    float umidade = dht.readHumidity();

    int valorSolo = analogRead(soilPin);
    int porcentagemSolo = map(valorSolo, 4095, 1500, 0, 100);
    porcentagemSolo = constrain(porcentagemSolo, 0, 100);

    // Verifica leituras do DHT
    if (isnan(temp) || isnan(umidade)) {
      Serial.println("Erro ao ler DHT22.");
      // Não retorna aqui, apenas não envia os dados inválidos.
    } else {
      Serial.printf("Luminosidade: %d%%\n", porcentagemLuz);
      Serial.printf("Temperatura: %.1f°C\n", temp);
      Serial.printf("Umidade do ar: %.1f%%\n", umidade);
      Serial.printf("Umidade do solo: %d%%\n", porcentagemSolo);
      Serial.println("-----------------------------");

      // Envia para Adafruit IO
      luminosidade->save(porcentagemLuz);
      temperatura->save(temp);
      umidade_ar->save(umidade);
      umidade_solo->save(porcentagemSolo);
    }
  }
  // O loop é rápido, então não precisamos de delay(10000) no final
  // A verificação de tempo para leitura dos sensores já faz isso.
}
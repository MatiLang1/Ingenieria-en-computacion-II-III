#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "secrets.h" 

#define DHTPIN 4
#define DHTTYPE DHT11

#define LDR_PIN 34
#define NIVEL_PIN 35

#define LED_VERDE 26
#define LED_ROJO 27
#define BUZZER 25

const char* ssid = WIFI_SSID; 
const char* password = WIFI_PASSWORD; 

const char* mqtt_server = MQTT_SERVER_IP; 
const int mqtt_port = 1882; 

// Configuración PWM para Buzzer Pasivo (Compatible ESP32 Core v3.0)
const int pwmFreq = 2000;
const int pwmResolution = 8;
const int pwmDuty = 128; // 50% duty cycle

// Buzzer vars
unsigned long tiempoInicioBuzzer = 0;
bool buzzerState = false;
const unsigned long DURACION_BUZZER = 15000; 

// LED vars
unsigned long ultimo_parpadeo = 0;
bool ledState = LOW;
const unsigned long intervalo_parapadeo = 500; 

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

String estadoSistema = "NORMAL";

unsigned long ultima_publicacion = 0;
const unsigned long intervalo_publicacion = 5000; 

void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == "semillero/control/buzzer" && msg.indexOf("OFF") != -1) {
    ledcWrite(BUZZER, 0); // Apagar PWM (Core v3 usa Pin)
    buzzerState = false; 
    client.publish("semillero/estado/buzzer", "{ \"buzzer\": \"OFF (Manual)\" }");
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a MQTT IP: ");
    Serial.println(mqtt_server);
    if (client.connect("ESP32_Semillero")) {
      Serial.println("Conectado a broker MQTT");
      client.subscribe("semillero/control/buzzer");
    } else {
      Serial.print("Falló MQTT, rc=");
      Serial.print(client.state());
      Serial.println(" reintentando en 2s");
      delay(2000);
    }
  }
}

#define TEMP_AVISO 28 
#define TEMP_ALERTA 35
#define LUZ_AVISO 2000
#define LUZ_ALERTA 2900
#define NIVEL_AVISO 900
#define NIVEL_ALERTA 400

void evaluarEstado(float temp, int luz, int nivel) {
  bool alerta = (temp > TEMP_ALERTA) || (luz > LUZ_ALERTA) || (nivel < NIVEL_ALERTA);
  bool aviso = (temp > TEMP_AVISO) || (luz > LUZ_AVISO) || (nivel < NIVEL_AVISO);

  if (alerta) {
    estadoSistema = "ALERTA";
    
    // Forzamos PWM ON SIEMPRE que haya alerta (por si acaso se apago)
    ledcWrite(BUZZER, pwmDuty); 

    // Solo logueamos/publicamos si el estado cambió a TRUE
    if (!buzzerState) {
        buzzerState = true;
        Serial.println("ALERTA detectada! Buzzer ON (PWM)");
        // Forzamos el envio inmediato
        client.publish("semillero/estado/buzzer", "{ \"buzzer\": \"ON\" }");
    }
    
    tiempoInicioBuzzer = millis(); // Reinicia temporizador
  } 
  else if (aviso) {
     estadoSistema = "AVISO";
    Serial.println("Estado del sistema: AVISO");
  }
  else {
    estadoSistema = "OK";
    Serial.println("Estado del sistema: OK");
  }
}

void publishAll(float temp, int luz, int nivel) {
  char buf[80];

  if (isnan(temp)) {
    sprintf(buf, "{ \"value\": \"-\" }");
  } else {
    sprintf(buf, "{ \"value\": %.2f }", temp);
  }
  if (client.publish("semillero/sensores/temperatura", buf, true)) {
    Serial.println("Publicado temperatura: OK");
  } else {
    Serial.println("Publicado temperatura: FALLO");
  }

  if (luz == -1) {
    sprintf(buf, "{ \"value\": \"-\" }");
  } else {
    sprintf(buf, "{ \"value\": %d }", luz);
  }
  if (client.publish("semillero/sensores/luz", buf, true)) {
    Serial.println("Publicado luz: OK");
  } else {
    Serial.println("Publicado luz: FALLO");
  }

  if (nivel == -1) {
    sprintf(buf, "{ \"value\": \"-\" }");
  } else {
    sprintf(buf, "{ \"value\": %d }", nivel);
  }
   if (client.publish("semillero/sensores/nivel", buf, true)) {
    Serial.println("Publicado nivel: OK");
  } else {
    Serial.println("Publicado nivel: FALLO");
  }

  sprintf(buf, "{ \"estado\": \"%s\" }", estadoSistema.c_str());
  client.publish("semillero/estado/sistema", buf, true);

  // Tambien publicamos aqui para asegurar redundancia
  client.publish("semillero/estado/buzzer",
    buzzerState ? "{ \"buzzer\": \"ON\" }" : "{ \"buzzer\": \"OFF\" }", true);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 iniciando...");

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  
  // Agregar pinMode por seguridad
  pinMode(BUZZER, OUTPUT);

  // Configuración PWM (Core v3.0+)
  // ledcAttach(pin, freq, resolution);
  if (!ledcAttach(BUZZER, pwmFreq, pwmResolution)) {
      Serial.println("Error al adjuntar LEDC!");
  }

  dht.begin();

  Serial.println("Conectando a WiFi...");
  setupWiFi();
  Serial.println("WiFi conectado");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Test Sonido Inicio
  ledcWrite(BUZZER, pwmDuty);
  delay(200);
  ledcWrite(BUZZER, 0);

  Serial.println("Setup finalizado");
}

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

  // Logica de apagado del buzzer (timer)
  if (buzzerState) {
    // Si ya pasaron 15s desde la ÚLTIMA recarga
    if (millis() - tiempoInicioBuzzer >= DURACION_BUZZER) {
      ledcWrite(BUZZER, 0); // Apagar PWM (Core v3 usa Pin)
      buzzerState = false;
      client.publish("semillero/estado/buzzer", "{ \"buzzer\": \"OFF\" }");
      Serial.println("Buzzer apagado por Tiempo");
    }
  }

  // Logica de los LEDs
  digitalWrite(LED_VERDE, HIGH); 

  if (estadoSistema == "ALERTA") {
    digitalWrite(LED_ROJO, HIGH); 
  } 
  else if (estadoSistema == "AVISO") {
    if (millis() - ultimo_parpadeo >= intervalo_parapadeo) {
      ultimo_parpadeo = millis();
      ledState = !ledState;
      digitalWrite(LED_ROJO, ledState);
    }
  } 
  else {
    digitalWrite(LED_ROJO, LOW); 
  }

  if (millis() - ultima_publicacion >= intervalo_publicacion) {
    ultima_publicacion = millis();
    float temp = dht.readTemperature();
    // Determinamos si los analógicos están "desconectados". 
    // Cuando un pin analógico está al aire sin pullup/pulldown, varía o lee 0. 
    // Usualmente si es un divisor de tensión (LDR) o un sensor de nivel simple, leerá 0 o cerca de 0 al desconectarse. 
    // Sin embargo, podemos considerar que 0 absoluto durante un tiempo o menor a 5 es desconexión.
    int rawLuz = analogRead(LDR_PIN);
    int rawNivel = analogRead(NIVEL_PIN);

    int luz = (rawLuz < 5) ? -1 : rawLuz;
    int nivel = (rawNivel < 5) ? -1 : rawNivel;

    evaluarEstado(temp, luz, nivel); 
    publishAll(temp, luz, nivel);
  }
}

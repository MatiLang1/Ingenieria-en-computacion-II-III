#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "secrets.h" // Importamos las variables de entorno (usamos .h pq el ide de Arduino no maneja .env)

#define DHTPIN 4
#define DHTTYPE DHT11

#define LDR_PIN 34
#define NIVEL_PIN 35

#define LED_VERDE 26
#define LED_ROJO 27
#define BUZZER 25

// Obtenemos los valores del archivo "secrets.h" para proteger las credenciales
const char* ssid = WIFI_SSID; 
const char* password = WIFI_PASSWORD; 

const char* mqtt_server = MQTT_SERVER_IP; 
const int mqtt_port = 1883; // Puerto de la ESP32 en MQTT

//variables globales para manejo del buzzer pasivo
unsigned long tiempoInicioBuzzer = 0;
bool buzzerState = false;
const unsigned long DURACION_BUZZER = 15000; //15 segundos

//variables globales para manejo del parpadeo del LED rojo en el loop
unsigned long ultimo_parpadeo = 0;
bool ledState = LOW;
const unsigned long intervalo_parapadeo = 500; // 500 ms parpadeo

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);


String estadoSistema = "NORMAL";

unsigned long ultima_publicacion = 0;
const unsigned long intervalo_publicacion = 60000; // 1 minuto

void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];

if (String(topic) == "semillero/control/buzzer" && msg.indexOf("OFF") != -1) {
    noTone(BUZZER);
    buzzerState = false; //cancelamos el temporizador
    client.publish("semillero/estado/buzzer", "{ \"buzzer\": \"OFF (Manual)\" }");
}
}

void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("ESP32_Semillero")) {
      client.subscribe("semillero/control/buzzer");
    } else {
      delay(2000);
    }
  }
}

// Definimos los valores umbrales para los sensores (POSIBLEMENTE HAY QUE ADAPTARLOS A LA ESP32, ESTOS SON LOS VALORES DE ARDUINO, CONSIDERA 700/1024 = 68% POR LO QUE EN ESP32 QUE USA 4096 SERIA 2848 SU 68%, REVISAR SI HAY QUE ESCALAR ESTOS VALORES SOLO DE LUZ Y DE NIVEL EL DE TEMPERATURA NO SE MODIFICA)
#define TEMP_AVISO 28 // Grados °C
#define TEMP_ALERTA 35
#define LUZ_AVISO 700
#define LUZ_ALERTA 900
#define NIVEL_AVISO 300
#define NIVEL_ALERTA 100

void evaluarEstado(float temp, int luz, int nivel) {
  bool alerta = (temp > TEMP_ALERTA) || (luz > LUZ_ALERTA) || (nivel < NIVEL_ALERTA);
  bool aviso = (temp > TEMP_AVISO) || (luz > LUZ_AVISO) || (nivel < NIVEL_AVISO);

  if (alerta) {
    estadoSistema = "ALERTA";
    // Si entra en alerta (aunque sea un segundo), activamos el buzzer
    buzzerState = true;
    tiempoInicioBuzzer = millis(); // siempre actualiza el tiempo inicial (si pasaron 5 s quedan 10 s de buzzer, lo recarga a 15 s)
    tone(BUZZER, 1000); // activamos el tono del buzzer
    Serial.println("ALERTA detectada!");
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
  char buf[80]; //declaramos un buffer (un espacio de memoria temporal de type char un string con capacidad para 80 caracteres). Es un array de caracteres donde almacenamos el string completo que enviaremos por mqtt

  //sprintf nos permite formatear un string (como el printf pero en lugar de imprimir en consola, lo GUARDA en una variable, en este caso en buf). Asi formateamos el string completo que vamos a mandar por mqtt. Las / invertidas es para que el lenguaje entienda que las comillas de value son partes del string, el %.2f es reemplazado por el valor de la variable que le pasas (en este caso temp) el 2 indica 2 decimales la f es floar, en luz y nivel le pasas la d porque es int y en estado sistema le pasas la s pq es un string
  sprintf(buf, "{ \"value\": %.2f }", temp);
  client.publish("semillero/sensores/temperatura", buf);


  //El método .publish() de la librería PubSubClient recibe dos parámetros: El topic (string). y el payload que tiene el contenido que se envia, dicho payload debe ser un array de caracteres
  sprintf(buf, "{ \"value\": %d }", luz);
  client.publish("semillero/sensores/luz", buf);

  //sprintf sobreescribe buf cada vez que se usa para los distintos topics
  sprintf(buf, "{ \"value\": %d }", nivel);
  client.publish("semillero/sensores/nivel", buf);

  sprintf(buf, "{ \"estado\": \"%s\" }", estadoSistema.c_str());
  client.publish("semillero/estado/sistema", buf);

  client.publish("semillero/estado/buzzer",
    buzzerState ? "{ \"buzzer\": \"ON\" }" : "{ \"buzzer\": \"OFF\" }");
}

void setup() {
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  dht.begin();
  setupWiFi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnectMQTT();
  client.loop();

  //logica de apagado del buzzer
  if (buzzerState) {
    // Si ya pasaron 15s desde la ÚLTIMA recarga
    if (millis() - tiempoInicioBuzzer >= DURACION_BUZZER) {
      noTone(BUZZER);
      buzzerState = false;
      client.publish("semillero/estado/buzzer", "{ \"buzzer\": \"OFF\" }");
      Serial.println("Buzzer apagado por Tiempo");
    }
  }

  // Logica de los LEDs
  digitalWrite(LED_VERDE, HIGH); // Led Verde siempre encendido

  // Led Rojo depende del estado del sistema
  if (estadoSistema == "ALERTA") {
    digitalWrite(LED_ROJO, HIGH); // Rojo prendido fijo
  } 
  else if (estadoSistema == "AVISO") {
    // Rojo parpadeando cada 500ms
    if (millis() - ultimo_parpadeo >= intervalo_parapadeo) {
      ultimo_parpadeo = millis();
      ledState = !ledState;
      digitalWrite(LED_ROJO, ledState);
    }
  } 
  else {
    // OK
    digitalWrite(LED_ROJO, LOW); // Rojo apagado
  }

  //Publicacion periodica cada 1 minuto (60000 ms)
  if (millis() - ultima_publicacion >= intervalo_publicacion) {
    ultima_publicacion = millis();
    float temp = dht.readTemperature();
    int luz = analogRead(LDR_PIN);
    int nivel = analogRead(NIVEL_PIN);

    evaluarEstado(temp, luz, nivel);
    publishAll(temp, luz, nivel);
  }
}

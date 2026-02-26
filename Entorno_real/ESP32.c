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
const int mqtt_port = 1883; 

// Configuración PWM para Buzzer Pasivo
const int pwmFreq = 2000;
const int pwmResolution = 8;
const int pwmDuty = 128; // 50% duty cycle para q parpadee

// Variables para Buzzer
unsigned long tiempoInicioBuzzer = 0;
bool buzzerState = false;
const unsigned long DURACION_BUZZER = 15000; // 15 segundos

// Variables para LED
unsigned long ultimo_parpadeo = 0;
bool ledState = LOW;
const unsigned long intervalo_parapadeo = 500; // 500 milisegundos

WiFiClient espClient; // creamos el objeto espClient (instancia de WiFiClient)
PubSubClient client(espClient); // creamos el objeto client (instancia de PubSubClient) y le pasamos el objeto espClient para q este ultimo se encargue de la conexion WiFi (client se encarga de la comunicacion MQTT y espClient de la conexion WiFi, aca usamos inyeccion de dependencias)
DHT dht(DHTPIN, DHTTYPE); // creamos el objeto dht (instancia de DHT) y le pasamos los parametros (el pin y el tipo de sensor)

String estadoSistema = "OK"; // variable para almacenar el estado del sistema

unsigned long ultima_publicacion = 0;
const unsigned long intervalo_publicacion = 5000; 

//Configuracion de WiFi (el objeto Wifi es una instancia q ya trae la libreria)
void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}


// client llama a esta funcion cuandoel broker le avisa a la esp32 q llego un nuevo msj. La libreria PubSubClient manda los parametros topic, payload y length (envia la longitud del msj pq al mandar bytes en el payload no se sabe donde termina)
// FUNCION QUE RECIBE EL MSJ QUE PUBLICA LA PAGINA WEB
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i]; // convertimos los bytes en string (en caracteres y los concatenamos para formar el mensaje)

  // String(topic) pasa topic de char* a String asi validamos q sea el topico correcto, dsp hacemos una AND y en la segunda comdicion validamos q la palabra OFF este en el msj
  if (String(topic) == "semillero/control/buzzer" && msg.indexOf("OFF") != -1) {
    ledcWrite(BUZZER, 0); // apagamos el buzzer
    buzzerState = false; // cambiamos flag de buzzer
    client.publish("semillero/estado/buzzer", "{ \"buzzer\": \"OFF (Manual)\" }");
  }
}

// FUNCION QUE CONECTA LA ESP32 AL BROKER MQTT (reintenta conex cada 2 segundos)
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a MQTT IP: ");
    Serial.println(mqtt_server);
    //el metodo connect acepta 3 parametros (ID, user y password), en el broker habilitamos conex anonimas asique con pasarle el ID es suficiente (en este caso el ID es "ESP32_Semillero" el cual usa el broker para reconocer a nuestra ESP32)
    if (client.connect("ESP32_Semillero")) { // si se conecta al broker
      Serial.println("Conectado a broker MQTT");
      client.subscribe("semillero/control/buzzer"); // nos suscribimos al topic donde la pag web publica
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

  // SI ENTRA EN ESTADO DE ALERTA
  if (alerta) {
    estadoSistema = "ALERTA";
    
    // Prendemos buzzer (mandamos una señal pwm, le pasamos de argumento el pin y el duty cycle)
    ledcWrite(BUZZER, pwmDuty); 

    // Solo publicamos si el estado cambió a TRUE
    if (!buzzerState) {
        buzzerState = true;
        Serial.println("ALERTA detectada! Buzzer ON");
        // publicamos el estado del buzzer para q se pueda visualizar de forma inmediata en la seccion "Tiempo Real" de la pagina web
        client.publish("semillero/estado/buzzer", "{ \"buzzer\": \"ON\" }");
    }
    
    tiempoInicioBuzzer = millis(); // Reinicia temporizador
  } 

  // SI ENTRA EN ESTADO DE AVISO
  else if (aviso) {
    estadoSistema = "AVISO";
    Serial.println("Estado del sistema: AVISO");
  }

  // SI ENTRA EN ESTADO OK (al no entrar en Alerta ni Aviso)
  else {
    estadoSistema = "OK";
    Serial.println("Estado del sistema: OK");
  }
}

void publishAll(float temp, int luz, int nivel) {
  char buf[80]; // en C para concatenar debemos crear un buffer. Lo hacemos para almacenar los mensajes (reservamos memoria para 80 caracteres)

  // sprintf formatea el mensaje pero no lo imprime, lo guarda en el buffer "buf" y luego publicamos el buffer el cual contiene el mensaje/payload. En los siguiente sprintf se hace lo mismo sobreescribiendo "buf"
  // El true es para q el broker retenga el ultimo valor publicado
  sprintf(buf, "{ \"value\": %.2f }", temp);
  if (client.publish("semillero/sensores/temperatura", buf, true)) {
    Serial.println("Publicado temperatura: OK");
  } else {
    Serial.println("Publicado temperatura: FALLO");
  }

  //en cada publicacion se hace lo mismo, formateamos el msj usando el buffer "buf" (q se sobreescribe por cada topic) y publicamos
  sprintf(buf, "{ \"value\": %d }", luz);
  if (client.publish("semillero/sensores/luz", buf, true)) {
    Serial.println("Publicado luz: OK");
  } else {
    Serial.println("Publicado luz: FALLO");
  }

  sprintf(buf, "{ \"value\": %d }", nivel);
   if (client.publish("semillero/sensores/nivel", buf, true)) {
    Serial.println("Publicado nivel: OK");
  } else {
    Serial.println("Publicado nivel: FALLO");
  }

  sprintf(buf, "{ \"estado\": \"%s\" }", estadoSistema.c_str());
  client.publish("semillero/estado/sistema", buf, true);

  client.publish("semillero/estado/buzzer",
    buzzerState ? "{ \"buzzer\": \"ON\" }" : "{ \"buzzer\": \"OFF\" }", true);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 iniciando...");

  // Configuración de pines de LEDs
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  
  // Agregamos pinMode por seguridad
  pinMode(BUZZER, OUTPUT);

  // Configuramos PWM en el pin del buzzer. ledcAttach(pin, freq, resolution)
  if (!ledcAttach(BUZZER, pwmFreq, pwmResolution)) {
      Serial.println("Error al adjuntar LEDC!");
  }

  dht.begin(); // inicializamos el sensor de temperatura

  Serial.println("Conectando a WiFi");
  setupWiFi();
  Serial.println("WiFi conectado");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port); // establecemos la direccion del broker (IP + Puerto)
  client.setCallback(callback); // establecemos la funcion que se ejecutara cuando se reciba un mensaje MQTT

}

void loop() {
  if (!client.connected()) reconnectMQTT(); // si no esta conectado al broker, intenta reconectarse
  client.loop(); // mantiene viva la conexion con el broker

  // Logica de apagado del buzzer (timer de 15s)
  if (buzzerState) {
    if (millis() - tiempoInicioBuzzer >= DURACION_BUZZER) {
      ledcWrite(BUZZER, 0); // Apagamos PWM
      buzzerState = false;
      // publicamos el estado del buzzer de forma instantanea para q se pueda visualizar en la seccion "Tiempo Real" de la pagina web
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

  // Leemos sensores y publicamos valores periodicamente (cada 5 segundos)
  if (millis() - ultima_publicacion >= intervalo_publicacion) {
    ultima_publicacion = millis();

    // Guardamos en variables las lecturas de los sensores y las pasamos a las funciones evaluarEstado y publishAll
    float temp = dht.readTemperature();
    int luz = analogRead(LDR_PIN);
    int nivel = analogRead(NIVEL_PIN);
    evaluarEstado(temp, luz, nivel); 
    publishAll(temp, luz, nivel); 
  }
}

import paho.mqtt.client as mqtt
import time
import json
import random

BROKER = "mosquitto_real"
# BROKER = "broker" (deberia andar con cualquiera de los dos)
PORT = 1883

TOPICS = {
    "temp": "semillero/sensores/temperatura",
    "luz": "semillero/sensores/luz",
    "nivel": "semillero/sensores/nivel",
    "estado_sistema": "semillero/estado/sistema",
    "estado_buzzer": "semillero/estado/buzzer",
    "control_buzzer": "semillero/control/buzzer"
}

# Umbrales (Iguales a ESP32.c)
TEMP_AVISO = 28
TEMP_ALERTA = 35
LUZ_AVISO = 2000
LUZ_ALERTA = 2900
NIVEL_AVISO = 900
NIVEL_ALERTA = 400

# Variables de estado
estado_sistema = "OK"
buzzer_state = False
tiempo_inicio_buzzer = 0
DURACION_BUZZER = 15.0 # 15 segundos

led_state = False
ultimo_parpadeo = 0
intervalo_parpadeo = 0.5 # 500 ms

def on_connect(client, userdata, flags, reason_code, properties):
    print("Conectado a broker MQTT")
    client.subscribe(TOPICS["control_buzzer"])

def on_message(client, userdata, msg):
    global buzzer_state
    payload_str = msg.payload.decode()
    if msg.topic == TOPICS["control_buzzer"] and "OFF" in payload_str:
        print("Mensaje de control recibido: Apagando buzzer manualmente.")
        buzzer_state = False
        print("LOG: Buzzer OFF")
        client.publish(TOPICS["estado_buzzer"], '{ "buzzer": "OFF (Manual)" }', retain=True)

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

print("Intentando conectar a MQTT...")
while True:
    try:
        client.connect(BROKER, PORT, 60)
        break
    except ConnectionRefusedError:
        print("Broker no listo, reintentando en 2s...")
        time.sleep(2)

client.loop_start() # Bucle en segundo plano para escuchar mensajes

ultima_publicacion = 0
intervalo_publicacion = 5.0 # 5 segundos

print("Iniciando mock de la ESP32 en Python...")

while True:
    current_time = time.time()
    
    # Manejo del temporizador del buzzer
    if buzzer_state:
        if current_time - tiempo_inicio_buzzer >= DURACION_BUZZER:
            buzzer_state = False
            print("LOG: Buzzer OFF por Tiempo (15s)")
            client.publish(TOPICS["estado_buzzer"], '{ "buzzer": "OFF" }', retain=True)

    # Lógica de LEDs por logs (Ejecutándose fluidamente en el loop principal)
    if estado_sistema == "ALERTA":
        # Led Rojo Fijo
        # Para no spamear infinitamente el log, solo imprimimos el estado en cada ciclo de publicación o si se pide explicitamente, 
        # pero simularemos el LED con un rate para que haya evidencia, o mejor, simplemente seteamos el estado lógico y lo logueamos una vez por lectura.
        pass
    elif estado_sistema == "AVISO":
        if current_time - ultimo_parpadeo >= intervalo_parpadeo:
            ultimo_parpadeo = current_time
            led_state = not led_state
            # print("LOG: Led Rojo Titilando" if led_state else "LOG: Led Rojo Apagado") # Retirado para no floodear el log cada 500ms
            
    # Lectura periódica y publicación (cada 5s)
    if current_time - ultima_publicacion >= intervalo_publicacion:
        ultima_publicacion = current_time
        
        # Generar valores aleatorios
        temp = random.uniform(20.0, 40.0)
        luz = random.randint(1000, 4000)
        nivel = random.randint(200, 1500)
        
        alerta = (temp > TEMP_ALERTA) or (luz > LUZ_ALERTA) or (nivel < NIVEL_ALERTA)
        aviso = (temp > TEMP_AVISO) or (luz > LUZ_AVISO) or (nivel < NIVEL_AVISO)
        
        if alerta:
            estado_sistema = "ALERTA"
            print("LOG: Led Rojo Prendido Fijo")
            if not buzzer_state:
                buzzer_state = True
                print("LOG: Buzzer ON")
                client.publish(TOPICS["estado_buzzer"], '{ "buzzer": "ON" }', retain=True)
                tiempo_inicio_buzzer = time.time()
        elif aviso:
            estado_sistema = "AVISO"
            print("LOG: Led Rojo Prendido Titilando") # Se loguea resumido cada lectura para no trabar stdout
        else:
            estado_sistema = "OK"
            print("LOG: Led Rojo Apagado")
            
        print(f"Estado del sistema: {estado_sistema}")

        # Publicar valores
        client.publish(TOPICS["temp"], f'{{ "value": {temp:.2f} }}', retain=True)
        client.publish(TOPICS["luz"], f'{{ "value": {int(luz)} }}', retain=True)
        client.publish(TOPICS["nivel"], f'{{ "value": {int(nivel)} }}', retain=True)
        client.publish(TOPICS["estado_sistema"], f'{{ "estado": "{estado_sistema}" }}', retain=True)
        client.publish(TOPICS["estado_buzzer"], '{ "buzzer": "ON" }' if buzzer_state else '{ "buzzer": "OFF" }', retain=True)
        
        print(f"Publicado -> Temp: {temp:.2f}, Luz: {int(luz)}, Nivel: {int(nivel)}")
        
    time.sleep(0.1) # Para no consumir el 100% del CPU en el While True

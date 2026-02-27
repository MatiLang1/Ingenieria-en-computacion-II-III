import time
import random
import paho.mqtt.client as mqtt

BROKER = "mqtt-broker" #nombre del contenedor del broker MQTT (definido en el nombre del servicio en el docker compose)
PORT = 1883 #puerto del broker MQTT (definido en el servicio "mqtt-broker" en el docker compose, es el puerto virtual aunq tambien esta mapeado al de la PC)

TOPICS = {
    "temp": "semillero/sensores/temperatura",
    "luz": "semillero/sensores/luz",
    "nivel": "semillero/sensores/nivel",
    "estado_sistema": "semillero/estado/sistema",
    "buzzer": "semillero/estado/buzzer"
}

#Umbrales de Aviso y Alerta
TEMP_AVISO = 28
TEMP_ALERTA = 35
LUZ_AVISO = 700
LUZ_ALERTA = 900
NIVEL_AVISO = 300
NIVEL_ALERTA = 100

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
while True:
    try:
        # el 60 es el keep alive del publicador (si no manda nada por 60 segundos se desconecta del broker)
        client.connect(BROKER, PORT, 60)
        break
    # si se atrapa el error "ConnectionRefusedError" puntualmente, se ejecuta logica de reconexion
    except ConnectionRefusedError:
        print("Broker no listo, reintentando en 2s")
        time.sleep(2)

while True:
    # Generar valores aleatorios
    temp = round(random.uniform(10, 40), 1)
    luz = random.randint(0, 1023)
    nivel = random.randint(0, 1023)

    # Determinar Estado_sistema y Buzzer
    estado_sistema = "OK"
    buzzer = "OFF"
    
    #Logica de Alerta, Aviso y OK
    is_alerta = (temp > TEMP_ALERTA) or (luz > LUZ_ALERTA) or (nivel < NIVEL_ALERTA)
    is_aviso = (temp > TEMP_AVISO) or (luz > LUZ_AVISO) or (nivel < NIVEL_AVISO)

    if is_alerta:
        estado_sistema = "ALERTA"
        buzzer = "ON"
    elif is_aviso:
        estado_sistema = "AVISO"
        buzzer = "OFF"

    payload = {
        "temperatura": temp,
        "luz": luz,
        "nivel": nivel,
        "estado_sistema": estado_sistema,
        "buzzer": buzzer
    }

    #este payload lo podria convertir a JSON con json.dumps(payload) y enviar dicho objeto en un solo topic. Implica pasar el diccionario a formato json y ese json pasa a formato bytes cuando usamos el metodo publish (ya q MQTT solo maneja bytes)

    #el suscriptor siempre recibe bytes en msg.payload ya MQTT envia y recibe bytes, hay que convertirlo a texto plano segun lo q enviamos y eso lo hacemos con el metodo .decode(). Luego (si ese texto plano es un JSON) hay que parsearlo con json.loads para convertirlo en un diccionario de python
 
    #ENVIO DE CADA VALOR POR SEPARADO EN FORMATO TEXTO PLANO
    # Ej de envio: (my/topic/ valor) -- semillero/sensores/temperatura 25.5
    client.publish(TOPICS["temp"], payload["temperatura"])
    client.publish(TOPICS["luz"], payload["luz"])
    client.publish(TOPICS["nivel"], payload["nivel"])
    client.publish(TOPICS["estado_sistema"], payload["estado_sistema"])
    client.publish(TOPICS["buzzer"], payload["buzzer"])

    # print("Publicado:", payload) #imprimimos el diccionario payload

    # LOGS (imprimimos topico + valor de cada uno)
    print(TOPICS["temp"] + " " + str(payload["temperatura"]))
    print(TOPICS["luz"] + " " + str(payload["luz"]))
    print(TOPICS["nivel"] + " " + str(payload["nivel"]))
    print(TOPICS["estado_sistema"] + " " + str(payload["estado_sistema"]))
    print(TOPICS["buzzer"] + " " + str(payload["buzzer"]))
    time.sleep(60) # publicamos cada 60 segundos

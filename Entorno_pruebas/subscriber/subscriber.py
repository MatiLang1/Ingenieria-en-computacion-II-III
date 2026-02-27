#Codigo suscriptor MQTT (este genera un JSON con los datos recibidos, aprovechando el volumen de docker)
import time
import json
import os
from collections import deque
import paho.mqtt.client as mqtt
from datetime import datetime

BROKER = "mqtt-broker"
PORT = 1883
MAX_REGISTROS = 500
JSON_FILE = "registros_mqtt.json"

# Usamos deque para tener una lista de 500 registros y q se borre el mas antiguo al agregar uno nuevo
registros = deque(maxlen=MAX_REGISTROS)

# FUNCION PARA GUARDAR EN DISCO (SOBREESCRIBE LA COLA DE REGISTROS ACTUAL EN EL JSON)
def guardar_en_disco():
    #Guardamos la cola de registros actual en un archivo JSON
    try:
        with open(JSON_FILE, "w") as f:
            # Convertimos la deque a lista para que sea serializable
            json.dump(list(registros), f, indent=4)
    except Exception as e:
        print(f"Error al guardar JSON: {e}")


# FUNCION QUE SE EJECUTA CUANDO LLEGA UN MENSAJE (la llama la libreria cuando ocurre el evento de recepcion de msj). Los parametro client y userdata son pasados por la libreria obligatoriamente aunque no los usemos
def on_message(client, userdata, msg):
    #estructuramos el registro con tiempo (decodificamos el payload de bytes a texto plano)
    nuevo_dato = {
        "fecha": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "topic": msg.topic,
        "valor": msg.payload.decode() # Ej: "25.5" (texto plano)
    }
    
    # agregamos el nuevo registro a la cola/lista
    registros.append(nuevo_dato)
    
    #actualizamos el JSON cada vez que llega un mensaje
    guardar_en_disco()
    
    print(f"Recibido y guardado: {nuevo_dato['topic']} - {nuevo_dato['valor']}")


client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message #guardamos en el atributo "on_message" de nuestro objeto "client" (de la libreria MQTT) nuestra funcion "on_message". Al hacerlo sin () no ejecutamos la funcion sino q la asignamos

#cargar registros previos del JSON si existen (para no perder datos al reiniciar)
if os.path.exists(JSON_FILE): #si existe el archivo JSON en tu SO (en el contenedor en este caso, el cual existe pq hicimos un volumen donde lo copiamos)
    try:
        with open(JSON_FILE, "r") as f: #abrimos el archivo JSON en modo lectura
            datos_previos = json.load(f) #cargamos los datos previos del JSON
            registros.extend(datos_previos) #agregamos los datos previos a la cola/lista
            print(f"Se cargaron {len(datos_previos)} registros previos desde el JSON")
    except: # atrapamos cualquier error y lo ignoramos para q siga la ejecucion
        pass

# REALIZAMOS LA CONEXION CON EL BROKER
while True:
    try:
        client.connect(BROKER, PORT, 60) #intentamos conectar con el broker (60s de keep alive, si no recibimos nada del broker en 60s, asumimos que se desconecto)
        break #si se conecta, salimos del bucle
    except ConnectionRefusedError: #si no se conecta, nos quedamos en el bucle y reintentamos cada 2s
        print("Broker no listo, reintentando en 2s")
        time.sleep(2)

client.subscribe("semillero/#")
# MEJOR ARQUITECTURA (suscribirse a cada topic por separado cumpliendo la arquitectura de la API), en este caso como es un entorno de test usamos el comodin # por comodidad y en caso de q agreguemos mas topicos a nuestra API no hay q modificar el codigo del suscriptor
# client.subscribe("semillero/sensores/temperatura")
# client.subscribe("semillero/sensores/luz")
# client.subscribe("semillero/sensores/nivel")
# client.subscribe("semillero/estado/sistema")
# client.subscribe("semillero/estado/buzzer")

print("Suscriptor conectado y esperando mensajes")
client.loop_forever()



#Codigo anterior (suscriptor MQTT, sin generar JSON)
# import time
# from collections import deque
# import paho.mqtt.client as mqtt

# BROKER = "mqtt-broker" #nombre del contenedor del broker MQTT (definido en el nombre del servicio en el docker compose)
# PORT = 1883 #puerto del broker MQTT (definido en el servicio "mqtt-broker" en el docker compose, es el puerto virtual aunq tambien esta mapeado al de la PC)

# MAX_REGISTROS = 500 #cantidad maxima de registros a guardar
# registros = deque(maxlen=MAX_REGISTROS) #cola de registros (deque es una cola con tamaño maximo)

# def on_message(client, userdata,msg): #se ejecuta cuando se recibe un mensaje
#     registro = {
#         "topic": msg.topic, #el topic ya viene en formato string (texto plano)
#         "valor": msg.payload.decode() #decodifica el payload que viene en formato bytes a texto plano
#     }
#     registros.append(registro)
#     print("Recibido:", registro)

# client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
# client.on_message = on_message


# #el 60 es el "Keep Alive" en s, le dice al broker: si no recibes ninguna señal mía (mensaje o ping) por más de 60 segundos, asume que me he desconectado. El cliente envía pings automáticos usando el meotdo .loop_forever() para mantener la conexión viva

# while True:
#     try:
#         client.connect(BROKER, PORT, 60)
#         break
#     except ConnectionRefusedError:
#         print("Broker no listo, reintentando en 2s")
#         time.sleep(2)

# client.subscribe("semillero/#") #se suscribe a todos los topics que empiecen con "semillero/"
# client.loop_forever() #esto envia los pings para mantener la conexion con el broker viva, si por mas de 60s el broker MQTT no recibe ningun ping, asume que el cliente se desconectó




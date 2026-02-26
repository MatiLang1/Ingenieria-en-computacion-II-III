// Logica Página Web + Suscriptor MQTT


//Cuando enviamos por MQTT, enviamos texto plano o un JSON a client.publish(...). La librería de MQTT, con el metodo publish, lo convierte automáticamente en bytes y lo manda al broker
// Cuando recibimos por MQTT, recibimos bytes puros. El message en client.on('message', (topic, message)) es un Buffer (un paquete de bytes en la memoria). Por eso hay q hacerle message.toString() (para convertirlo de bytes a JSON) y luego JSON.parse() para convertirlo de JSON a Objeto JS

// Importamos la libreria MQTT.js en el index.html (luego cargamos este JS en el html), los eventos como "connect", "offline", "message" son eventos definidos en esa libreria

const client = mqtt.connect('ws://192.168.0.111:9001'); // nos conectamos del JS (el suscriptor) al broker (asegurarse de que la IP coincida con la de la PC)

// inicializamos registros desde LocalStorage si existen
// obtenemos el historial del semillero haciendo el get al localstorage segun su indice del html "historial_semillero"
let savedData = localStorage.getItem('historial_semillero');
// si existe el historial, lo parseamos (pasamos de json a una lista de objetos JS, cada objeto JS contiene los valores de hora, temperatura, etc), si no, creamos un array vacio
const registros = savedData ? JSON.parse(savedData) : [];
const MAX = 500;

// renderizamos la tabla inicialmente si hay datos guardados. Usamos el evento "DOMContentLoaded" para asegurar que la tabla exista antes de renderizarla. DOMContentLoaded es un evento que se dispara cuando el HTML de la pag se cargo por completo
// document es un Objeto global que ya viene integrado en los navegadores web. Representa toda la página HTML renderizada, permite que JS pueda interactuar con el HTML (como buscar elementos o cambiar textos)
// el navegador escucha hasta q ocurre el evento y ahi ejecuta la funcion
document.addEventListener('DOMContentLoaded', () => {
    if (registros.length > 0) {
        renderTabla();
    }
});

// creamos un mapa/diccionario "bufferMuestra", este buffer agrupa los tópicos individuales en una sola muestra antes de mandarlos a la tabla (asi mostramos los 5 topicos juntos, le agregamos la hora y asi formamos un registro con 6 campos)
let bufferMuestra = {
    temperatura: null,
    luz: null,
    nivel: null,
    estado: null,
    buzzer: null
};

// mapa/diccionario "ultimo" para actualizar los SPANS superiores (los de tiempo real)individualmente
let ultimo = {
    temperatura: "--",
    luz: "--",
    nivel: "--",
    estado: "--",
    buzzer: "--"
};

// del objeto "client" utilizamos el metodo "on" para escuchar eventos. En este caso, escuchamos el evento "connect" que se dispara cuando el cliente se conecta al broker, cuando ocurre el evento se dispara la funcion flecha
client.on('connect', () => {
    console.log("Conectado al Broker MQTT");

    //Nos suscribimos a todos los tópicos del semillero
    client.subscribe('semillero/#');

    // UI
    const statusDiv = document.getElementById('connection-status'); //guardamos en "statusDiv" el elemento HTML que tiene el id "connection-status" (guardamos al elemento HTML convertido como objeto JS y permite acceder a las propiedades del elemento)

    if (statusDiv) statusDiv.innerHTML = "● En línea"; //si existe el elemento (statusDiv no es nul), lo actualizamos usando el innerHTML (es una propiedad de JS q permite controlar el contenido de la etiqueta HTML)

    // modificamos el objeto JS, el navegador tiene un traductor DOM (Document Object Model) que se encarga de leer las etiquetas HTML y crearlas como objetos JS
});

// funcion flecha que se ejecuta cuando el cliente se desconecta del broker (cuando ocurre el evento "offline")
client.on('offline', () => {
    const statusDiv = document.getElementById('connection-status'); // guardamos en "statusDiv" el elemento HTML que tiene el id "connection-status" (guardamos al elemento HTML convertido como objeto JS, esto permite acceder a las propiedades del elemento)

    if (statusDiv) statusDiv.innerHTML = "<span style='color:red'>● Desconectado</span>"; // escribimos en el elemento HTML con id "connection-status" el texto "● Desconectado" con un color rojo
});

// funcion flecha que se ejecuta cuando el cliente recibe un mensaje del broker (cuando ocurre el evento "message"). La libreria mqtt nos pasa los parametros topic y message q recibe del broker
client.on('message', (topic, message) => {
    console.log(`Mensaje recibido en ${topic}:`, message.toString()); //imprimimos en consola el log del topico y el mensaje recibido (en formato JSON)
    try {

        // "message" es un Buffer (paquete de bytes en memoria), por eso le aplicamos message.toString() para convertirlo a texto plano (en este caso a JSON), luego parseamos dicho JSON para convertirlo a un objeto JS
        // JSON es un tipo de texto plano
        const data = JSON.parse(message.toString());

        //guardamos en "valor" lo q extraemos del objeto JS "data" (guardamos el valor dependiendo del topico que mande el publisher)
        // El chequedo de undefined es pq luz, nivel y temp tienen clave "value" si es undefined verifica si es estado q tiene clave "estado" o buzzer q tiene clave "buzzer" si no es undefined es pq el valor corresponde a luz, temp o nivel
        // Chequea segun la clave del topico, la clave/atributo del objeto "data" q sea True es su valor el q se guarda en "valor" (data.value != undefined ? data.value si el atributo "value" del objeto "data" no es nulo asignale el valor del atributo value)
        const valor = data.value !== undefined ? data.value : (data.estado || data.buzzer);


        //Actualizamos el objeto "ultimo" para los indicadores individuales (valores de TIEMPO REAL)
        // si el topico termina en "temperatura" guardamos "valor" en el atributo "temperatura" del objeto "ultimo" y en el atributo "temperatura" del objeto "bufferMuestra". Asi con todos los topicos. topic es un string, el metodo endsWith() es propio de JS y devuelve True si el string termina con el argumento especificado
        if (topic.endsWith('temperatura')) {
            ultimo.temperatura = valor;
            bufferMuestra.temperatura = valor;
        }
        if (topic.endsWith('luz')) {
            ultimo.luz = valor;
            bufferMuestra.luz = valor;
        }
        if (topic.endsWith('nivel')) {
            ultimo.nivel = valor;
            bufferMuestra.nivel = valor;
        }
        if (topic.endsWith('sistema')) {
            ultimo.estado = valor;
            bufferMuestra.estado = valor;

            // Actualizar color del texto del estado (validamos de acuerdo a las clases del css)
            const elEstado = document.getElementById('estado');
            if (elEstado) {
                elEstado.className = "value"; // reset
                if (valor === "ALERTA") elEstado.classList.add("status-alert");
                else if (valor === "AVISO") elEstado.classList.add("status-warning");
                else elEstado.classList.add("status-normal");
            }
        }
        if (topic.endsWith('buzzer')) {
            ultimo.buzzer = valor;
            bufferMuestra.buzzer = valor;
        }

        // Actualizamos los números en la parte superior de la web
        actualizarIndicadoresSuperiores();

        //creamos una fila en la tabla si recibimos los 5 topics. Verificamos si todos los atributos del objeto "bufferMuestra" tienen sus valores. el objet.values() devuelve esto [24.5, 80, null, null, null] y el every() chequea q ningun valor del array sea nulo
        //muestraLista es booleana True si todos los valores son distintos de null, False si alguno es null
        const muestraLista = Object.values(bufferMuestra).every(v => v !== null);

        if (muestraLista) {
            const registroCompleto = {
                hora: new Date().toLocaleTimeString(),
                ...bufferMuestra
            };

            // con el metodo push agrego a un nuevo elemento al final de la lista (el nuevo registro q es el objeto "registroCompleto"), igual al append
            registros.push(registroCompleto);

            // si la cantidad de registros supera los 500, eliminamos el registro mas antiguo (shift elimina el primer elemento del array)
            if (registros.length > MAX) registros.shift();

            // Guardar en localstorage la cual es una memoria del navegador para que al reiniciar la pagina no se borren los registros
            //JSON.stringify(registros): convierte la lista de 500 registros (que es un array de objetos JS) a JSON
            // .setItem es el metodo para guardar en la clave "historial_semillero" el JSON q tiene los registros. localstorage es un objeto global del navegador q permite guardar datos en el navegador usando clave-valor
            localStorage.setItem('historial_semillero', JSON.stringify(registros));

            // Limpiamos el buffer para la próxima tanda de mensajes
            bufferMuestra = { temperatura: null, luz: null, nivel: null, estado: null, buzzer: null };

            renderTabla();
        }

    } catch (e) {
        console.error("Error al procesar mensaje JSON:", e);
    }
});

//funcion para actualizar los valores del HTML que se muestran en tiempo real. Busca los elementos con los IDs temp, luz, etc definidos en el index.html. Luego reemplaza el contenido (usando el metodo innerText) de esos elementos HTML con los valores mas recientes almacenados en el objeto "ultimo" (usamos los atributos del objeto "ultimo")
function actualizarIndicadoresSuperiores() {
    document.getElementById('temp').innerText = ultimo.temperatura;
    document.getElementById('luz').innerText = ultimo.luz;
    document.getElementById('nivel').innerText = ultimo.nivel;
    document.getElementById('estado').innerText = ultimo.estado;
    document.getElementById('buzzer').innerText = ultimo.buzzer;
}

function renderTabla() {
    const tbody = document.getElementById('tabla'); // asignamos la etiqueta con id="tabla" a la variable tbody, luego realizamos las modificaciones a partir de tbody
    if (!tbody) return; // validamos (si no existe tbody, salimos de la funcion)

    // invertimos el array usando el metodo reverse sobre nuestra lista de registros  llamada "registros" para que lo más nuevo aparezca arriba
    // los 3 puntos ... (Spread Operator) sirven para "desarmar" un arreglo y sacar todos sus elementos individuales
    const registrosInvertidos = [...registros].reverse();

    // tenemos una lista de objetos JS
    // con map() transformamos esa lista en pedazos sueltos de código HTML (con etiquetas tr y td)
    // con join('') pegamos esos pedazos de cod html para formar un solo bloque HTML
    // con innerHTML agarramos ese bloque y lo inyectamos en la variable "tbody" y el navegador lo dibuja como una tabla
    tbody.innerHTML = registrosInvertidos.map(r => `
        <tr>
            <td>${r.hora}</td>
            <td>${r.temperatura}</td>
            <td>${r.luz}</td>
            <td>${r.nivel}</td>
            <td>${r.estado}</td>
            <td>${r.buzzer}</td>
        </tr>
    `).join('');
}

//funcion que se ejecuta cuando se presiona el boton "Apagar buzzer". Se encarga de publicar por MQTT un mensaje con el valor "OFF" al topico "semillero/control/buzzer" para que la ESP32 lo reciba y apague el buzzer
function apagarBuzzer() {
    console.log("Apagando buzzer desde la Página web");
    client.publish('semillero/control/buzzer', JSON.stringify({ buzzer: "OFF" })
    );
}
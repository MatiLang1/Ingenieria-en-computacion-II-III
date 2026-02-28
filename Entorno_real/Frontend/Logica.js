const client = mqtt.connect('ws://192.168.0.24:9001'); //(asegurarse de que la IP coincida con la de la PC)

// Inicializamos registros desde LocalStorage si existen
let savedData = localStorage.getItem('historial_semillero');
const registros = savedData ? JSON.parse(savedData) : [];
const MAX = 500;

// Renderizamos la tabla inicialmente si hay datos guardados
// Usamos DOMContentLoaded para asegurar que la tabla exista antes de renderizar
document.addEventListener('DOMContentLoaded', () => {
    if (registros.length > 0) {
        renderTabla();
    }
});

//este buffer agrupa los tópicos individuales en una sola muestra antes de mandarlos a la tabla (asi mostramos los 5 topicos juntos, le agregamos la hora y asi formamos un registro con 6 los campos)
let bufferMuestra = {
    temperatura: null,
    luz: null,
    nivel: null,
    estado: null,
    buzzer: null
};

// Objeto para actualizar los SPANS superiores individualmente
let ultimo = {
    temperatura: "--",
    luz: "--",
    nivel: "--",
    estado: "--",
    buzzer: "--"
};

client.on('connect', () => {
    console.log("Conectado al Broker MQTT");
    client.subscribe('semillero/#');

    // UI Update on connect
    const statusDiv = document.getElementById('connection-status');
    if (statusDiv) statusDiv.innerHTML = "● En línea";
});

client.on('offline', () => {
    const statusDiv = document.getElementById('connection-status');
    if (statusDiv) statusDiv.innerHTML = "<span style='color:red'>● Desconectado</span>";
});

client.on('message', (topic, message) => {
    console.log(`Mensaje recibido en ${topic}:`, message.toString());
    try {
        const data = JSON.parse(message.toString());

        //extraemos el valor dependiendo del topico que mande la ESP32
        const valor = data.value !== undefined ? data.value : (data.estado || data.buzzer);

        //Actualizamos el objeto "ultimo" para los indicadores individuales
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

            // Actualizar color del texto del estado
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

        //creamos una fila en la tabla si recibimos los 5 topics. Verificamos si todos los campos del buffer tienen sus valores
        const muestraLista = Object.values(bufferMuestra).every(v => v !== null);

        if (muestraLista) {
            const registroCompleto = {
                hora: new Date().toLocaleTimeString(),
                ...bufferMuestra
            };

            registros.push(registroCompleto);
            if (registros.length > MAX) registros.shift();

            // Guardar en localstorage la cual es una memoria del navegador para que al reiniciar la pagina no se borren los registros
            //JSON.stringify(registros): convierte la lista de 500 registros (que es un array de objetos JS) a JSON
            //localStorage.setItem: guarda el JSON en el navegador (setItem(...) guarda ese texto bajo el nombre historial_semillero)
            localStorage.setItem('historial_semillero', JSON.stringify(registros));

            // Limpiamos el buffer para la próxima tanda de mensajes
            bufferMuestra = { temperatura: null, luz: null, nivel: null, estado: null, buzzer: null };

            renderTabla();
        }

    } catch (e) {
        console.error("Error al procesar mensaje JSON:", e);
    }
});

//funcion para actualizar los valores del HTML que se muestran en tiempo real. Busca los elementos con los IDs temp, luz, etc definidos en el index.html. Luego reemplaza el contenido (innerText) de esos elementos con los valores mas recientes almacenados en el objeto "ultimo"
function actualizarIndicadoresSuperiores() {
    document.getElementById('temp').innerText = ultimo.temperatura;
    document.getElementById('luz').innerText = ultimo.luz;
    document.getElementById('nivel').innerText = ultimo.nivel;
    document.getElementById('estado').innerText = ultimo.estado;
    document.getElementById('buzzer').innerText = ultimo.buzzer;
}

function renderTabla() {
    const tbody = document.getElementById('tabla');
    if (!tbody) return;

    // Invertimos el array para que lo más nuevo aparezca arriba
    const registrosInvertidos = [...registros].reverse();

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
    client.publish(
        'semillero/control/buzzer',
        JSON.stringify({ buzzer: "OFF" })
    );
}
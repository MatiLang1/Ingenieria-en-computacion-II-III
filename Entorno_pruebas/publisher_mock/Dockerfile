#imagen base sobre la cual se construye la imagen de este contenedor (descargamos imagen de python)
FROM python:3.11-slim

#directorio de trabajo (en esta ruta se pega todo el codigo y desde ella se levanta el contenedor)
WORKDIR /app

#evita que los prints se buffericen (es decir, que no se tenga que esperar a que el buffer se llene para mostrar los prints)
ENV PYTHONUNBUFFERED=1

#instala las dependencias (paho-mqtt en este caso)
RUN pip install --no-cache-dir paho-mqtt

#copiamos el archivo publisher.py al directorio de trabajo (al poner . se copia en el directorio actual que es "/app")
COPY publisher.py .

#comando que se ejecuta al iniciar el contenedor (al hacer docker run), con esta instruccion indicamos que el contenedor, apenas se levante, debe ejecutar python publisher.py
CMD ["python", "publisher.py"]
#ENTRYPOINT ["python", "publisher.py"] esta es otra forma de hacerlo (con CMD se puede sobreescribir el comando al momento de ejecutar el contenedor)


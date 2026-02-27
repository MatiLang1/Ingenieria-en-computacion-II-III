#descargamos imagen de python
FROM python:3.11-slim

#establecemos la ruta de trabajo
WORKDIR /app

#establecemos la variable de entorno
ENV PYTHONUNBUFFERED=1

#instalamos las dependencias (libreria paho-mqtt)
RUN pip install --no-cache-dir paho-mqtt

#copiamos el archivo subscriber.py a la ruta de trabajo (/app) usando . es en el directorio actual
COPY subscriber.py .

#ejecutamos el archivo subscriber.py (con esta instruccion indicamos que el contenedor, apenas se levante, debe ejecutar python subscriber.py)
CMD ["python", "subscriber.py"]

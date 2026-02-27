#descargamos la imagen de python
FROM python:3.11-slim

#establecemos la ruta de trabajo
WORKDIR /app

#establecemos la variable de entorno
ENV PYTHONUNBUFFERED=1 

#instalamos las dependencias (libreria paho-mqtt)
RUN pip install --no-cache-dir paho-mqtt

#copiamos el archivo test_system.py a la ruta de trabajo (/app) usando . es en el directorio actual
COPY test_system.py .

#ejecutamos el archivo test_system.py (con esta instruccion indicamos que el contenedor, apenas se levante, debe ejecutar python test_system.py)
CMD ["python", "test_system.py"]

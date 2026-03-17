# Este código usa Pytest, que es el estándar de la industria para testear en Python. assert significa "afirmar". En programación, es una forma de decir: "Yo afirmo que esto que sigue DEBE ser verdad. Si no lo es, avisame que hay un error", si la condición se cumple: El test pasa (luz verde). Si la condición NO se cumple: El programa se detiene y te dice: "Acá afirmaste que el estado era OK, pero el sensor me devolvió AVISO"

import pytest
from publisher import (
    determinar_estado,
    debe_activar_buzzer,
    buzzer_timeout_expirado,
)

def test_determinar_estado_ok():
    # Valores dentro de lo normal
    assert determinar_estado(temp=25, luz=1500, nivel=1000) == "OK"

def test_determinar_estado_aviso():
    # Temperatura de aviso, los demas bien
    assert determinar_estado(temp=30, luz=1500, nivel=1000) == "AVISO"
    # Luz de aviso
    assert determinar_estado(temp=25, luz=2500, nivel=1000) == "AVISO"
    # Nivel de aviso (bajo pero no critico)
    assert determinar_estado(temp=25, luz=1500, nivel=600) == "AVISO"

def test_determinar_estado_alerta():
    # Temperatura critica
    assert determinar_estado(temp=36, luz=1500, nivel=1000) == "ALERTA"
    # Luz critica
    assert determinar_estado(temp=25, luz=3000, nivel=1000) == "ALERTA"
    # Nivel critico
    assert determinar_estado(temp=25, luz=1500, nivel=300) == "ALERTA"

def test_debe_activar_buzzer():
    # Si esta en alerta y el buzzer esta apagado, debe activarse
    assert debe_activar_buzzer("ALERTA", False) is True
    # Si esta en alerta pero el buzzer ya esta prendido, no reactivar
    assert debe_activar_buzzer("ALERTA", True) is False
    # Si NO esta en alerta, no activar
    assert debe_activar_buzzer("OK", False) is False

def test_buzzer_timeout_expirado():
    tiempo_inicio = 100.0
    duracion = 15.0
    # Pasaron 10 s (no expiro)
    assert buzzer_timeout_expirado(tiempo_inicio, duracion, 110.0) is False
    # Pasaron 15 s (expiro)
    assert buzzer_timeout_expirado(tiempo_inicio, duracion, 115.0) is True
    # Pasaron 20 s (expiro)
    assert buzzer_timeout_expirado(tiempo_inicio, duracion, 120.0) is True

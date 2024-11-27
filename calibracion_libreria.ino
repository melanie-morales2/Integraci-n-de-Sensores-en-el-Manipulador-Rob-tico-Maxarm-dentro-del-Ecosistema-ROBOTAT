#include <WiFi.h>
#include "Calibracion.h"

Calibracion calibracion;

void setup() {
    Serial.begin(115200);
    Serial.println("Iniciando sistema...");

    calibracion.iniciarSensores();
}

void loop() {
    int distancia = calibracion.calibrarDistancia(ultrasound.GetDistance());
    Serial.print("Distancia calibrada: ");
    Serial.println(distancia);

    int sonido = calibracion.detectarSonido(analogRead(A0));
    Serial.print("Nivel de sonido: ");
    Serial.println(sonido);

    int luz = calibracion.calcularLuz(analogRead(A1));
    Serial.print("Nivel de luz: ");
    Serial.println(luz);

    int color = calibracion.detectarColor();
    if (color == RED) Serial.println("Color detectado: Rojo");
    else if (color == GREEN) Serial.println("Color detectado: Verde");
    else if (color == BLUE) Serial.println("Color detectado: Azul");
    else Serial.println("Color no detectado");

    delay(1000);
}

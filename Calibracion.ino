#include <WiFi.h>
#include <ArduinoJson.h>
#include "Ultrasound.h"
#include "Arduino_APDS9960.h" // Librería para el sensor de color
#include "ESPMax.h"
#include "Buzzer.h"
#include "ESP32PWMServo.h"
#include "SuctionNozzle.h"
#include <esp32-hal-ledc.h>

// Configuración de WiFi
const char* ssid = "A55 de Melanie";
const char* password = "melaniefer242";

// Configuración del servidor TCP
WiFiServer server(8888);

// Instancia del sensor Ultrasound
Ultrasound ultrasound;

// Pines de los sensores
#define touch_sensor_pin 23
#define sound_sensor_pin 32
#define light_sensor_pin 32

// Pines PWM para controlar el Glowy Sensor
#define RED_PIN   25
#define GREEN_PIN 26
#define BLUE_PIN  27

// Constantes para el sensor de color
#define RED   1
#define GREEN 2
#define BLUE  3

// Variables específicas para la rutina de movimiento
int x, y, z;
int angle_pul = 1500;
int detect_color = 0;
bool color_detect = true;

void setup() {
    Serial.begin(115200);
    Serial.println("Iniciando Arduino...");

    // Inicialización de sensores y WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado a WiFi.");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());

    ultrasound = Ultrasound();
    pinMode(touch_sensor_pin, INPUT_PULLUP);
    analogReadResolution(10);
    pinMode(sound_sensor_pin, INPUT);
    pinMode(light_sensor_pin, INPUT);
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    if (!APDS.begin()) {
        Serial.println("Error inicializando APDS-9960 sensor.");
    } else {
        Serial.println("Sensor de color inicializado correctamente.");
    }

    // Inicialización específica de la rutina del proveedor
    Buzzer_init();
    ESPMax_init();
    Nozzle_init();
    PWMServo_init();
    Valve_on();
    go_home(2000);
    Valve_off();
    delay(100);
    SetPWMServo(1, 1500, 1000);
    ultrasound.Color(255, 255, 255, 255, 255, 255); // Apagar luces RGB

    server.begin();
    Serial.println("Servidor TCP iniciado, esperando conexiones...");
}

void loop() {
    handleClient(); // Maneja las solicitudes del cliente
    handleColorSorting(); // Detecta colores y ejecuta la rutina de movimiento
}

// Función para gestionar la comunicación con el cliente
void handleClient() {
    WiFiClient client = server.available();

    if (client) {
        Serial.println("Cliente conectado.");
        while (client.connected()) {
            if (client.available()) {
                String data = client.readStringUntil('\n');
                Serial.println("Datos recibidos: " + data);

                String selectedSensor = getSelectedSensor(data);
                Serial.println("Sensor seleccionado: " + selectedSensor);

                String jsonData;
                if (selectedSensor == "Distancia") {
                    int rawValue = ultrasound.GetDistance();
                    float calibratedValue = calibrateDistance(rawValue);
                    jsonData = createJson(calibratedValue, 0, 0, 0, 0);
                } else if (selectedSensor == "Sonido") {
                    int soundValue = analogRead(sound_sensor_pin);
                    int soundPercentage = map(soundValue, 0, 1023, 0, 100);
                    jsonData = createJson(0, 0, soundPercentage, 0, 0);
                } else if (selectedSensor == "Luz") {
                    int lightValue = analogRead(light_sensor_pin);
                    int darknessPercentage = map(lightValue, 0, 1023, 100, 0);
                    jsonData = createJson(0, 0, 0, darknessPercentage, 0);
                } else if (selectedSensor == "Touch") {
                    int touchState = digitalRead(touch_sensor_pin);
                    jsonData = createJson(0, touchState, 0, 0, 0);
                } else if (selectedSensor == "Color") {
                    int colorDetected = ColorDetect();
                    jsonData = createJson(0, 0, 0, 0, colorDetected);
                }

                if (jsonData.length() > 0) {
                    client.println(jsonData);
                    delay(500);
                }
            }
        }
        client.stop();
        Serial.println("Cliente desconectado.");
    }
}

// Función para manejar la detección de colores y movimiento
void handleColorSorting() {
    float pos[3];
    int distance = 0;

    if (color_detect) { // Fase de detección de colores
        if (ColorDetect()) {
            float color_num = 0.0;
            for (int i = 0; i < 5; i++) {
                color_num += ColorDetect();
                delay(80);
            }
            color_num = color_num / 5.0;
            color_detect = false;

            if (color_num == 1.0) {
                x = 120; y = -140; z = 85; angle_pul = 2200;
                detect_color = RED;
                Serial.println("Red");
                ultrasound.Color(255, 0, 0, 255, 0, 0);
            } else if (color_num == 2.0) {
                x = 120; y = -80; z = 85; angle_pul = 2000;
                detect_color = GREEN;
                Serial.println("Green");
                ultrasound.Color(0, 255, 0, 0, 255, 0);
            } else if (color_num == 3.0) {
                x = 120; y = -20; z = 82; angle_pul = 1800;
                detect_color = BLUE;
                Serial.println("Blue");
                ultrasound.Color(0, 0, 255, 0, 0, 255);
            } else {
                detect_color = 0;
                color_detect = true;
                ultrasound.Color(255, 255, 255, 255, 255, 255);
            }
        } else {
            ultrasound.Color(255, 255, 255, 255, 255, 255);
            delay(200);
        }
    } else {
        for (int i = 0; i < 5; i++) {
            distance += ultrasound.GetDistance();
            delay(100);
        }
        int dis = int(distance / 5);
        if (60 < dis && dis < 80) {
            if (detect_color) {
                setBuzzer(100);
                delay(1000);
                pos[0] = 0; pos[1] = -160; pos[2] = 100;
                set_position(pos, 1500);
                delay(1500);
                pos[0] = 0; pos[1] = -160; pos[2] = 85;
                set_position(pos, 800);
                Pump_on();
                delay(1000);
                pos[0] = 0; pos[1] = -160; pos[2] = 180;
                set_position(pos, 1000);
                delay(1000);
                pos[0] = x; pos[1] = y; pos[2] = 180;
                set_position(pos, 1500);
                delay(1500);
                SetPWMServo(1, angle_pul, 800);
                delay(200);
                pos[0] = x; pos[1] = y; pos[2] = z;
                set_position(pos, 1000);
                delay(1000);
                Valve_on();
                pos[0] = x; pos[1] = y; pos[2] = 200;
                set_position(pos, 1000);
                delay(1000);
                Valve_off();
                go_home(1500);
                SetPWMServo(1, 1500, 800);
                delay(200);
                detect_color = 0;
                color_detect = true;
                ultrasound.Color(255, 255, 255, 255, 255, 255);
                delay(1500);
            }
        }
    }
}

// Otras funciones auxiliares
String getSelectedSensor(String data) {
    if (data.indexOf("Distancia") >= 0) return "Distancia";
    if (data.indexOf("Sonido") >= 0) return "Sonido";
    if (data.indexOf("Luz") >= 0) return "Luz";
    if (data.indexOf("Touch") >= 0) return "Touch";
    if (data.indexOf("Color") >= 0) return "Color";
    return "";
}

float calibrateDistance(int rawValue) {
    if (rawValue <= 91) {
        float a = 0.0029, b = 0.7581, c = -3.7035;
        return a * pow(rawValue, 2) + b * rawValue + c;
    } else {
        float m = 0.8195, b = 18.72;
        return m * rawValue + b;
    }
}

String createJson(float calibratedValue, int touchState, int soundValue, int lightValue, int colorDetected) {
    StaticJsonDocument<200> doc;
    doc["calibratedValue"] = calibratedValue;
    doc["touchState"] = touchState;
    doc["soundValue"] = soundValue;
    doc["lightValue"] = lightValue;
    doc["colorDetected"] = colorDetected;
    String jsonData;
    serializeJson(doc, jsonData);
    return jsonData;
}

int ColorDetect() {
    while (!APDS.colorAvailable()) delay(5);
    int r, g, b, c;
    APDS.readColor(r, g, b);
    r = map(r, 30, 3000, 0, 255);
    g = map(g, 50, 2600, 0, 255);
    b = map(b, 50, 3500, 0, 255);

    if (r > g) c = RED;
    else c = GREEN;
    if (c == GREEN && g < b) c = BLUE;
    if (c == RED && r < b) c = BLUE;

    if (c == BLUE && b > 60) return c;
    else if (c == GREEN && g > 60) return c;
    else if (c == RED && r > 60) return c;
    else return 0;
}

#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <TM1638.h>

// Пины подключения TM1638
#define DIO 13
#define CLK 18
#define STB 14
#define LED_PIN 2

TM1638 module(DIO, CLK, STB);

int ledBrightness = 0;
int ledStep = 5;
unsigned long lastUpdate = 0;
const int ledUpdateInterval = 10;

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);      // Настроить встроенный светодиод как выход
    digitalWrite(LED_PIN, HIGH);   // Включить светодиод — система готова

    module.setupDisplay(true, 7); // Включить дисплей, яркость 7
    module.setDisplayToString("HELLO  ");

    Serial.println("System initialized. Ready to work.");
}

void loop() {

    unsigned long now = millis();
    if (now - lastUpdate >= ledUpdateInterval) {
        lastUpdate = now;
        ledBrightness += ledStep;
        if (ledBrightness <= 0 || ledBrightness >= 155) {
            ledStep = -ledStep;
            ledBrightness += ledStep;
        }

    analogWrite(LED_PIN, ledBrightness);

    Serial.print("LED brightness: ");
    Serial.println(ledBrightness);
    }

    // Пример: отобразить нажатые кнопки
    byte keys = module.getButtons();
    module.setDisplayToDecNumber(ledBrightness, 0, false);
    delay(200);
    Serial.print("TM1638 keys: ");
    Serial.println(keys, BIN);
}
    

void app_main() {}
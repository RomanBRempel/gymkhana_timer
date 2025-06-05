#include <Arduino.h>
#include <TM1638.h>
#include <webtimer.h>
WebTimer web(80);

#define DIO 13
#define CLK 18
#define STB 14
#define LED_PIN 2
#define SENSOR_PIN 4 // Любой свободный пин для входа

TM1638 module(DIO, CLK, STB);

enum State { WAIT, RUN, STOP, SHOW };
State timerState = WAIT;

unsigned long myTimerStart = 0;
unsigned long myTimerStop = 0;
unsigned long ignoreUntil = 0;
float lastTime = 0.0;

void displayAllZerosBlink() {
    static bool on = false;
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 400) {
        lastBlink = millis();
        on = !on;
        if (on) module.setDisplayToString("00.00.0000");
        else   module.setDisplayToString("        ");
        module.setLEDs(0xFF);
    }
}

void displayTime(float t) {
    // Формат: MM.SS.DDDD (минуты.секунды.доли)
    int mins = (int)t / 60;
    float secf = t - mins * 60;
    int secs = (int)secf;
    int ms = (int)((secf - secs) * 10000); // 4 знака после запятой
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", mins, secs, ms);
    module.setDisplayToString(buf);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    module.setupDisplay(true, 5);
    module.setDisplayToString("GYMKHANA", 0, 0, FONT_DEFAULT);
    delay(1000);

    web.begin("GymkhanaTim", "12345678");
    web.setButtonCallback([](){
        // обработка виртуальной кнопки старт/стоп
         if (timerState == WAIT) {
    // Старт таймера
            timerState = RUN;
            myTimerStart = micros();
            ignoreUntil = millis() + 1000; // игнорировать вход 1 сек
            module.setLEDs(0x00);
        } else if (timerState == RUN) {
            // Принудительная остановка таймера
            timerState = STOP;
            myTimerStop = micros();
            lastTime = (myTimerStop - myTimerStart) / 1000000.0;
            web.addLog(lastTime);
        } else if (timerState == STOP || timerState == SHOW) {
            // Сброс к ожиданию
            timerState = WAIT;
            lastTime = 0.0;
        }
    });


}

void loop() {
    static bool sensorPrev = HIGH;
    byte keys = module.getButtons();

    switch (timerState) {
        case WAIT:
            displayAllZerosBlink();
            web.update(0.0, false); // Веб: ожидание, таймер не идёт
            if (keys) { // сброс результата по кнопке
                lastTime = 0.0;
            }
            if (digitalRead(SENSOR_PIN) == LOW) {
                timerState = RUN;
                myTimerStart = micros();
                ignoreUntil = millis() + 1000; // игнорировать вход 1 сек
                module.setLEDs(0x00);
            }
            break;

        case RUN: {
            float t = (micros() - myTimerStart) / 1000000.0;
            displayTime(t);
            web.update(t, true); // Веб: таймер идёт
            // Игнорируем любые изменения на SENSOR_PIN первую секунду
            if (millis() < ignoreUntil) break;
            // Ждём устойчивого высокого уровня
            if (digitalRead(SENSOR_PIN) == HIGH) {
                timerState = STOP;
                myTimerStop = micros();
                lastTime = (myTimerStop - myTimerStart) / 1000000.0;
            }
            break;
        }

        case STOP:
            displayTime(lastTime);
            web.update(lastTime, false); // Веб: таймер остановлен
            web.addLog(lastTime);
            module.setLEDs(0xFF);
            // Ожидаем появления низкого уровня (готовность к новому старту)
            if (digitalRead(SENSOR_PIN) == LOW) {
                timerState = SHOW;
            }
            break;

        case SHOW:
            displayTime(lastTime);
            web.update(lastTime, false); // Веб: показ результата
            // Ожидаем нажатия кнопки для сброса
            if (keys) {
                timerState = WAIT;
            }
            break;
    }
}
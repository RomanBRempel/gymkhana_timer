#include <Arduino.h>
#include <TM1638.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define DIO 13
#define CLK 18
#define STB 14
#define LED_PIN 2
#define SENSOR_PIN 19 
#define LOG_SIZE 10
float timeLog[LOG_SIZE];
int logCount = 0;

bool sensorPrev = HIGH;
const char* ssid = "GymkhanaTimer";
const char* password = "12345678";
AsyncWebServer server(80);

volatile bool webStart = false;
volatile bool webReset = false;

String getTimeString(float t) {
    int mins = (int)t / 60;
    float secf = t - mins * 60;
    int secs = (int)secf;
    int ms = (int)((secf - secs) * 10000);
    char buf[16];
    snprintf(buf, sizeof(buf), "%01d:%02d:%04d", mins, secs, ms);
    return String(buf);
}

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
        if (on) module.setDisplayToString("0.00.000");
        else   module.setDisplayToString("        ");
        
    }
}

void displayTime(float t) {
    // Формат: MM.SS.DDD (минуты.секунды.доли)
    int mins = (int)t / 60;
    float secf = t - mins * 60;
    int secs = (int)secf;
    int ms = (int)((secf - secs) * 10000); // 4 знака после запятой
    char buf[9];
    snprintf(buf, sizeof(buf), "%01d.%02d.%03d", mins, secs, ms);
    module.setDisplayToString(buf);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing...");
    pinMode(LED_PIN, OUTPUT);
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    module.setupDisplay(true, 5);
    module.setDisplayToString("GYMKHANA", 0, 0, FONT_DEFAULT);
    
    WiFi.softAP(ssid, password);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    float showTime = lastTime;
    if (timerState == RUN) {
        showTime = (micros() - myTimerStart) / 1000000.0;
    }
    String timeStr = getTimeString(showTime);

    String html = "<html><head><meta http-equiv='refresh' content='1'>";
    html += "<style>";
    html += "body{font-family:sans-serif; background:#222; color:#fff; margin:0;}";
    html += ".container{display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh;}";
    html += ".title{font-size:48px; margin-bottom:24px;}";
    html += ".timer{font-size:120px; font-weight:bold; letter-spacing:8px; margin:40px 0 0 0;}";
    html += ".labels{display:flex;justify-content:center;font-size:32px;margin-top:8px;gap:80px;}";
    html += ".btn{font-size:48px;padding:24px 80px;margin-top:48px;border-radius:16px;border:none;background:#fff;color:#222;cursor:pointer;box-shadow:0 4px 24px #0008;}";
    html += ".btn:active{background:#eee;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='title'>GYMKHANA TIMER</div>";
    html += "<div class='timer'>" + timeStr + "</div>";
    html += "<div class='labels'><span>min</span><span>sec</span><span>frac</span></div>";

    // ЛОГ ПОСЛЕДНИХ РЕЗУЛЬТАТОВ
    html += "<div style='margin-top:40px;'>";
    html += "<div style='font-size:32px;margin-bottom:8px;'>Last results:</div>";
    html += "<div style='font-size:32px;'>";
    for (int i = logCount - 1; i >= 0; --i) {
        html += getTimeString(timeLog[i]);
        if (i > 0) html += " &nbsp; ";
    }
    html += "</div></div>";

    if (timerState == SHOW) {
        html += "<form action='/reset' method='POST'><button class='btn' type='submit'>RESET</button></form>";
    }
    html += "</div></body></html>";
    request->send(200, "text/html", html);
});
    
    
    server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request){
        webReset = true;
        request->redirect("/");
    });
    server.begin();
    Serial.println("Web server started");
    sensorPrev = digitalRead(SENSOR_PIN);

   
    
    };


void runningLeds() {
    static uint8_t pos = 0;
    static unsigned long lastMove = 0;
    if (millis() - lastMove > 100) {
        lastMove = millis();
        module.setLEDs(1 << pos);
        pos = (pos + 1) % 8;
    }
}

void loop() {
    
    static unsigned long highStart = 0;
    byte keys = module.getButtons();

    bool currSensor = digitalRead(SENSOR_PIN);
    
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
        lastPrint = millis();
        Serial.print("SENSOR_PIN: ");
        Serial.println(currSensor == HIGH ? "HIGH (not blocked)" : "LOW (blocked)");
    }


    // Бегущий огонь всегда, если сенсор не перекрыт
    if (currSensor == HIGH) {
        runningLeds();
    }

    // --- Web управление ---
    if (webReset) {
        timerState = WAIT;
        lastTime = 0.0;
        webReset = false;
    }
    // ----------------------

    switch (timerState) {
        case WAIT: {
            
            if (currSensor == HIGH && sensorPrev != LOW) {
                if (highStart == 0) highStart = millis();
            } else {
                highStart = 0;
            }
            displayAllZerosBlink();

            if (keys) lastTime = 0.0;

            
            if (sensorPrev == HIGH && currSensor == LOW &&  (millis() - highStart > 100)) {
                timerState = RUN;
                myTimerStart = micros();
                ignoreUntil = millis() + 1000;
                module.setLEDs(0x00);
                highStart = 0;
                Serial.println("START!");
            }
            break;
        }

        case RUN: {
            float t = (micros() - myTimerStart) / 1000000.0;
            displayTime(t);

            if (millis() < ignoreUntil) break;

            
            if (sensorPrev == HIGH && currSensor == LOW) {
                timerState = STOP;
                myTimerStop = micros();
                lastTime = (myTimerStop - myTimerStart) / 1000000.0;
            }
            if (currSensor == HIGH) {
                if (highStart == 0) highStart = millis();
            } else {
                highStart = 0;
            }
            break;
        }

        case STOP:
            displayTime(lastTime);
            module.setLEDs(0xFF);
            // Логирование результата
            if (logCount < LOG_SIZE) {
                timeLog[logCount++] = lastTime;
            } else {
                // Сдвиг логов, если переполнено
                for (int i = 1; i < LOG_SIZE; ++i) timeLog[i - 1] = timeLog[i];
                timeLog[LOG_SIZE - 1] = lastTime;
            }
            timerState = SHOW;
            break;

        case SHOW:
            displayTime(lastTime);
            if (keys) {
                timerState = WAIT;
            }
            break;
    }

    
    sensorPrev = currSensor; 
}
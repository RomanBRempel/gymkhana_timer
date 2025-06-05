#include "webtimer.h"

WebTimer::WebTimer(uint16_t port)
    : server(port), timerRunning(false), logIndex(0) {
    memset(logResults, 0, sizeof(logResults));
}

void WebTimer::begin(const char* ssid, const char* pass) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) delay(500);

    setupRoutes();
    server.begin();
}

void WebTimer::update(float timerValue, bool running) {
    timerRunning = running;
    int mins = (int)timerValue / 60;
    float secf = timerValue - mins * 60;
    int secs = (int)secf;
    int ms = (int)((secf - secs) * 10000);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", mins, secs, ms);
    timerStr = buf;
}

void WebTimer::addLog(float value) {
    logResults[logIndex] = value;
    logIndex = (logIndex + 1) % LOG_SIZE;
}

void WebTimer::setButtonCallback(std::function<void()> cb) {
    buttonCallback = cb;
}

String WebTimer::getLogHtml() {
    String html = "<ul>";
    for (int i = 0; i < LOG_SIZE; i++) {
        int idx = (logIndex + i) % LOG_SIZE;
        if (logResults[idx] > 0.0) {
            int mins = (int)logResults[idx] / 60;
            float secf = logResults[idx] - mins * 60;
            int secs = (int)secf;
            int ms = (int)((secf - secs) * 10000);
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d.%02d.%04d", mins, secs, ms);
            html += "<li>";
            html += buf;
            html += "</li>";
        }
    }
    html += "</ul>";
    return html;
}

void WebTimer::setupRoutes() {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        String html = "<html><head><meta http-equiv='refresh' content='1'></head><body>";
        html += "<h1>Таймер: " + timerStr + "</h1>";
        html += "<form action='/toggle' method='POST'><button type='submit'>";
        html += timerRunning ? "Стоп" : "Старт";
        html += "</button></form>";
        html += "<h2>Лог:</h2>";
        html += getLogHtml();
        html += "</body></html>";
        request->send(200, "text/html", html);
    });

    server.on("/toggle", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (buttonCallback) buttonCallback();
        request->redirect("/");
    });
}
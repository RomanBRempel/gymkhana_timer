#pragma once
#include <Arduino.h>
#include <AsyncWebServer.h>

class WebTimer {
public:
    WebTimer(uint16_t port = 80);
    void begin(const char* ssid, const char* pass);
    void update(float timerValue, bool running);
    void addLog(float value);
    void setButtonCallback(std::function<void()> cb);

private:
    AsyncWebServer server;
    String timerStr;
    bool timerRunning;
    static const int LOG_SIZE = 10;
    float logResults[LOG_SIZE];
    int logIndex;
    std::function<void()> buttonCallback;

    String getLogHtml();
    void setupRoutes();
};
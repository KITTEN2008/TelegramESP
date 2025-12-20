#include "TeleBot.h" //подключение библиотеки

TeleBot bot("YOUR_BOT_TOKEN"); //создание объекта(бота)

void setup() {
    bot.conWiFi("WiFi_SSID", "WiFi_PASS"); //подключение к Wi-Fi
    bot.begin(); //запуск бота
    bot.com("/start", [](Msg &msg) { //добавление команды "/start"
        bot.send(msg.chat_id, "Привет!"); //действие, ответ на команду
    });
}

void loop() {
    bot.loop(); //включение цикла обработки для старта бота
}

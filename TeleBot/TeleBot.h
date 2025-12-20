#ifndef TELEBOT_H
#define TELEBOT_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Опционально: поддержка SD карты
#ifdef TELEBOT_SD_ENABLE
#include <FS.h>
#include <SD.h>
#endif

// Максимальный размер сообщения
#define MAX_MSG_SIZE 4096

// Статусы WiFi
enum WiFiStat {
  WIFI_OFF,
  WIFI_CONNECTING,
  WIFI_ON,
  WIFI_ERROR
};

// Конфигурация WiFi
struct WiFiConf {
  const char* ssid;
  const char* password;
  const char* hostname = NULL;
  int timeout = 20000;
  bool staticIP = false;
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;
};

// Поддерживаемые расширения файлов
enum FileType {
  FILE_TXT,
  FILE_JSON,
  FILE_CSV,
  FILE_LOG,
  FILE_INI,
  FILE_HTML,
  FILE_XML,
  FILE_BIN
};

// Структура сообщения
struct Msg {
  long chat_id;
  String text;
  String user;
  String name;
  long msg_id;
  bool is_inline;
  String inline_data;
  String inline_id;
};

// Типы обработчиков
typedef void (*MsgHandler)(Msg &msg);
typedef void (*WiFiHandler)(WiFiStat status);

class TeleBot {
  public:
    // Конструкторы
    TeleBot(const char* token, WiFiClientSecure &client);
    TeleBot(const char* token);
    
    // Основные методы
    bool begin();
    void loop();
    
    // Отправка сообщений
    bool send(long chat_id, const String &text, 
              const String &parse = "", 
              const String &keys = "");
    
    bool send(long chat_id, const String &text, const String &keys);
    
    bool sendIn(long chat_id, const String &text, const String &keys);
    
    // Отправка медиа
    bool photo(long chat_id, const String &photo_url, 
               const String &caption = "");
    
    bool document(long chat_id, const String &doc_url,
                  const String &caption = "");
    
    bool location(long chat_id, float lat, float lon);
    
    // Действия чата
    bool sendChat(long chat_id, const String &action);
    
    // Работа с сообщениями
    bool edit(long chat_id, long msg_id, const String &text, 
              const String &keys = "");
    
    bool del(long chat_id, long msg_id);
    
    bool answer(const String &inline_id, const String &text = "");
    
    // Информация о боте
    String get();
    
    // Обработчики
    void on(MsgHandler handler);
    void com(const String &command, MsgHandler handler);
    void inl(MsgHandler handler);
    
    // Создание клавиатур
    static String createKey(const String keys[][2], int rows, 
                           bool resize = true, bool once = false);
    
    static String createIn(const String keys[][3], int rows,
                          bool delBtn = false);
    
    static String createURL(const String keys[][2], int rows);
    
    // Настройки
    void server(unsigned long interval);
    void debug(bool enable);
    void useDNS(bool enable);
    
    // WiFi методы
    bool conWiFi(const char* ssid, const char* pass);
    bool conWiFi(WiFiConf &conf);
    void deconWiFi();
    void autoWiFi(bool enable, unsigned long interval = 30000);
    bool isWiFi();
    void callWiFi(WiFiHandler handler);
    
    // SD карта методы (если включена поддержка)
    #ifdef TELEBOT_SD_ENABLE
    bool initSD(int csPin = 5, uint32_t freq = 4000000);
    bool initSD(const char* mountPoint = "/sd");
    String readSD(const String &path, FileType type = FILE_TXT);
    bool recordSD(const String &path, const String &data, 
                  FileType type = FILE_TXT);
    bool appendSD(const String &path, const String &data);
    bool deleteSD(const String &path);
    bool existsSD(const String &path);
    String listSD(const String &path = "/");
    String extF(); // Возвращает поддерживаемые расширения
    #endif
    
    // Утилиты
    String lastError();
    long lastUpdate();
    WiFiStat wifiStatus();
    
  private:
    const char* _token;
    WiFiClientSecure *_client;
    WiFiClientSecure _localClient;
    unsigned long _lastCheck = 0;
    unsigned long _checkTime = 1000;
    long _lastID = 0;
    bool _debug = false;
    bool _useDNS = true;
    String _error = "";
    
    // WiFi
    WiFiConf _wifiConf;
    WiFiStat _wifiStat = WIFI_OFF;
    WiFiHandler _wifiHandler = NULL;
    unsigned long _lastTry = 0;
    bool _autoReconnect = true;
    unsigned long _reconnectTime = 30000;
    
    // SD карта
    #ifdef TELEBOT_SD_ENABLE
    bool _sdInitialized = false;
    String _sdMountPoint = "/sd";
    #endif
    
    // Обработчики
    MsgHandler _msgHandler = NULL;
    struct ComHandler {
      String command;
      MsgHandler handler;
    };
    ComHandler _comHandlers[15];
    int _comCount = 0;
    MsgHandler _inlineHandler = NULL;
    
    // Внутренние методы
    bool _request(const String &method, const String &params, 
                  String &response);
    String _encode(const String &str);
    String _getUpdates();
    void _process(JsonObject &update);
    void _processMsg(JsonObject &msgObj);
    void _processInline(JsonObject &inlineObj);
    
    // WiFi методы
    void _initWiFi();
    void _wifiEvent(WiFiEvent_t event);
    void _setWiFiStat(WiFiStat status);
    bool _setStaticIP();
};

#endif

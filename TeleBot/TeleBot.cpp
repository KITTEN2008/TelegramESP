#include "TeleBot.h"
#include <HTTPClient.h>

TeleBot::TeleBot(const char* token, WiFiClientSecure &client) 
    : _token(token), _client(&client) {
}

TeleBot::TeleBot(const char* token) 
    : _token(token), _client(&_localClient) {
}

void TeleBot::_initWiFi() {
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    this->_wifiEvent(event);
  });
}

void TeleBot::_wifiEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      if (_debug) Serial.println("WiFi: Connected");
      _setWiFiStat(WIFI_ON);
      break;
      
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (_debug) Serial.println("WiFi: Disconnected");
      _setWiFiStat(WIFI_OFF);
      break;
      
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      if (_debug) {
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
      }
      _setWiFiStat(WIFI_ON);
      break;
      
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      if (_debug) Serial.println("WiFi: No IP");
      _setWiFiStat(WIFI_OFF);
      break;
      
    default:
      break;
  }
}

void TeleBot::_setWiFiStat(WiFiStat status) {
  _wifiStat = status;
  if (_wifiHandler) {
    _wifiHandler(status);
  }
}

bool TeleBot::conWiFi(const char* ssid, const char* password) {
  _wifiConf.ssid = ssid;
  _wifiConf.password = password;
  return conWiFi(_wifiConf);
}

bool TeleBot::conWiFi(WiFiConf &conf) {
  _wifiConf = conf;
  _initWiFi();
  
  _setWiFiStat(WIFI_CONNECTING);
  
  if (_debug) {
    Serial.print("WiFi: ");
    Serial.println(conf.ssid);
  }
  
  if (conf.hostname != NULL) {
    WiFi.setHostname(conf.hostname);
  }
  
  if (conf.staticIP) {
    if (!_setStaticIP()) {
      _setWiFiStat(WIFI_ERROR);
      _error = "Static IP error";
      return false;
    }
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(conf.ssid, conf.password);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < conf.timeout) {
    delay(500);
    if (_debug) Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    _setWiFiStat(WIFI_ON);
    
    if (_debug) {
      Serial.println("\nWiFi OK!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
    
    return true;
  } else {
    _setWiFiStat(WIFI_ERROR);
    _error = "WiFi timeout";
    if (_debug) Serial.println("\nWiFi FAIL!");
    return false;
  }
}

bool TeleBot::_setStaticIP() {
  if (!WiFi.config(_wifiConf.ip, _wifiConf.gateway, 
                   _wifiConf.subnet, _wifiConf.dns1, _wifiConf.dns2)) {
    if (_debug) Serial.println("Static IP FAIL!");
    return false;
  }
  return true;
}

void TeleBot::deconWiFi() {
  WiFi.disconnect(true);
  _setWiFiStat(WIFI_OFF);
  if (_debug) Serial.println("WiFi OFF");
}

void TeleBot::callWiFi(WiFiHandler handler) {
  _wifiHandler = handler;
}

WiFiStat TeleBot::wifiStatus() {
  return _wifiStat;
}

bool TeleBot::isWiFi() {
  return WiFi.status() == WL_CONNECTED;
}

void TeleBot::autoWiFi(bool enable, unsigned long interval) {
  _autoReconnect = enable;
  _reconnectTime = interval;
}

bool TeleBot::begin() {
  if (_useDNS) {
    _client->setInsecure();
  }
  
  if (_debug) {
    Serial.println("TeleBot started");
    Serial.print("Token: ");
    Serial.println(_token);
  }
  
  return true;
}

void TeleBot::loop() {
  // Авто-реконнект WiFi
  if (_autoReconnect && !isWiFi()) {
    unsigned long now = millis();
    if (now - _lastTry > _reconnectTime) {
      if (_debug) Serial.println("Auto WiFi...");
      WiFi.reconnect();
      _lastTry = now;
    }
    delay(100);
    return;
  }
  
  // Обработка сообщений
  if (isWiFi()) {
    unsigned long now = millis();
    
    if (now - _lastCheck > _checkTime) {
      String updates = _getUpdates();
      _lastCheck = now;
      
      if (updates.length() > 0) {
        if (_debug) {
          Serial.println("Updates:");
          Serial.println(updates);
        }
        
        DynamicJsonDocument doc(MAX_MSG_SIZE);
        DeserializationError error = deserializeJson(doc, updates);
        
        if (!error) {
          JsonArray result = doc["result"];
          
          for (JsonObject update : result) {
            _process(update);
          }
        } else if (_debug) {
          Serial.print("JSON error: ");
          Serial.println(error.c_str());
          _error = "JSON error: " + String(error.c_str());
        }
      }
    }
  }
}

String TeleBot::_getUpdates() {
  String url = "/bot" + String(_token) + "/getUpdates?timeout=5";
  
  if (_lastID > 0) {
    url += "&offset=" + String(_lastID + 1);
  }
  
  String response = "";
  
  if (_client->connect("api.telegram.org", 443)) {
    _client->println("GET " + url + " HTTP/1.1");
    _client->println("Host: api.telegram.org");
    _client->println("Connection: close");
    _client->println();
    
    unsigned long timeout = millis() + 5000;
    while (!_client->available() && millis() < timeout) {
      delay(10);
    }
    
    while (_client->available()) {
      String line = _client->readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }
    
    while (_client->available()) {
      response += _client->readString();
    }
    
    _client->stop();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      JsonArray results = doc["result"];
      for (JsonObject update : results) {
        long update_id = update["update_id"];
        if (update_id > _lastID) {
          _lastID = update_id;
        }
      }
    }
  }
  
  return response;
}

void TeleBot::_process(JsonObject &update) {
  if (update.containsKey("message")) {
    _processMsg(update["message"]);
  } else if (update.containsKey("callback_query")) {
    _processInline(update["callback_query"]);
  }
}

void TeleBot::_processMsg(JsonObject &msgObj) {
  Msg msg;
  msg.chat_id = msgObj["chat"]["id"];
  msg.msg_id = msgObj["message_id"];
  msg.is_inline = false;
  
  if (msgObj.containsKey("text")) {
    msg.text = msgObj["text"].as<String>();
  }
  
  if (msgObj["from"].containsKey("username")) {
    msg.user = msgObj["from"]["username"].as<String>();
  }
  
  if (msgObj["from"].containsKey("first_name")) {
    msg.name = msgObj["from"]["first_name"].as<String>();
  }
  
  // Обработка команд
  if (msg.text.startsWith("/")) {
    int spacePos = msg.text.indexOf(' ');
    String command = spacePos > 0 ? 
                     msg.text.substring(0, spacePos) : 
                     msg.text;
    
    for (int i = 0; i < _comCount; i++) {
      if (_comHandlers[i].command == command) {
        _comHandlers[i].handler(msg);
        return;
      }
    }
  }
  
  // Общий обработчик
  if (_msgHandler != NULL) {
    _msgHandler(msg);
  }
}

void TeleBot::_processInline(JsonObject &inlineObj) {
  Msg msg;
  msg.chat_id = inlineObj["message"]["chat"]["id"];
  msg.msg_id = inlineObj["message"]["message_id"];
  msg.is_inline = true;
  msg.inline_id = inlineObj["id"].as<String>();
  msg.inline_data = inlineObj["data"].as<String>();
  
  if (inlineObj["from"].containsKey("username")) {
    msg.user = inlineObj["from"]["username"].as<String>();
  }
  
  if (inlineObj["from"].containsKey("first_name")) {
    msg.name = inlineObj["from"]["first_name"].as<String>();
  }
  
  if (_inlineHandler != NULL) {
    _inlineHandler(msg);
  }
}

bool TeleBot::send(long chat_id, const String &text, 
                   const String &parse, const String &keys) {
  String params = "chat_id=" + String(chat_id) + 
                  "&text=" + _encode(text);
  
  if (parse.length() > 0) {
    params += "&parse_mode=" + parse;
  }
  
  if (keys.length() > 0) {
    params += "&reply_markup=" + _encode(keys);
  }
  
  String response;
  bool ok = _request("sendMessage", params, response);
  
  if (_debug) {
    Serial.print("Send: ");
    Serial.println(ok ? "OK" : "FAIL");
  }
  
  return ok;
}

bool TeleBot::send(long chat_id, const String &text, const String &keys) {
  return send(chat_id, text, "", keys);
}

bool TeleBot::sendIn(long chat_id, const String &text, const String &keys) {
  return send(chat_id, text, "", keys);
}

bool TeleBot::sendChat(long chat_id, const String &action) {
  String params = "chat_id=" + String(chat_id) + 
                  "&action=" + action;
  
  String response;
  return _request("sendChatAction", params, response);
}

bool TeleBot::answer(const String &inline_id, const String &text) {
  String params = "callback_query_id=" + inline_id;
  
  if (text.length() > 0) {
    params += "&text=" + _encode(text);
  }
  
  String response;
  return _request("answerCallbackQuery", params, response);
}

bool TeleBot::edit(long chat_id, long msg_id, const String &text, 
                   const String &keys) {
  String params = "chat_id=" + String(chat_id) + 
                  "&message_id=" + String(msg_id) + 
                  "&text=" + _encode(text);
  
  if (keys.length() > 0) {
    params += "&reply_markup=" + _encode(keys);
  }
  
  String response;
  return _request("editMessageText", params, response);
}

bool TeleBot::del(long chat_id, long msg_id) {
  String params = "chat_id=" + String(chat_id) + 
                  "&message_id=" + String(msg_id);
  
  String response;
  return _request("deleteMessage", params, response);
}

bool TeleBot::photo(long chat_id, const String &photo_url, const String &caption) {
  String params = "chat_id=" + String(chat_id) + 
                  "&photo=" + _encode(photo_url);
  
  if (caption.length() > 0) {
    params += "&caption=" + _encode(caption);
  }
  
  String response;
  return _request("sendPhoto", params, response);
}

bool TeleBot::document(long chat_id, const String &doc_url, const String &caption) {
  String params = "chat_id=" + String(chat_id) + 
                  "&document=" + _encode(doc_url);
  
  if (caption.length() > 0) {
    params += "&caption=" + _encode(caption);
  }
  
  String response;
  return _request("sendDocument", params, response);
}

bool TeleBot::location(long chat_id, float lat, float lon) {
  String params = "chat_id=" + String(chat_id) + 
                  "&latitude=" + String(lat, 6) + 
                  "&longitude=" + String(lon, 6);
  
  String response;
  return _request("sendLocation", params, response);
}

String TeleBot::get() {
  String response;
  if (_request("getMe", "", response)) {
    return response;
  }
  return "";
}

String TeleBot::createKey(const String keys[][2], int rows, 
                         bool resize, bool once) {
  DynamicJsonDocument doc(1024);
  JsonArray keyboard = doc.createNestedArray("keyboard");
  
  for (int i = 0; i < rows; i++) {
    JsonArray row = keyboard.createNestedArray();
    row.add(keys[i][0]);
    if (keys[i][1].length() > 0) {
      row.add(keys[i][1]);
    }
  }
  
  doc["resize_keyboard"] = resize;
  doc["one_time_keyboard"] = once;
  
  String output;
  serializeJson(doc, output);
  return output;
}

String TeleBot::createIn(const String keys[][3], int rows, bool delBtn) {
  DynamicJsonDocument doc(1024);
  JsonArray keyboard = doc.createNestedArray("inline_keyboard");
  
  for (int i = 0; i < rows; i++) {
    JsonArray row = keyboard.createNestedArray();
    JsonObject btn = row.createNestedObject();
    btn["text"] = keys[i][0];
    btn["callback_data"] = keys[i][1];
    
    if (keys[i][2].length() > 0) {
      btn["url"] = keys[i][2];
    }
  }
  
  if (delBtn) {
    JsonArray row = keyboard.createNestedArray();
    JsonObject btn = row.createNestedObject();
    btn["text"] = "❌ Удалить";
    btn["callback_data"] = "delete";
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}

String TeleBot::createURL(const String keys[][2], int rows) {
  DynamicJsonDocument doc(1024);
  JsonArray keyboard = doc.createNestedArray("inline_keyboard");
  
  for (int i = 0; i < rows; i++) {
    JsonArray row = keyboard.createNestedArray();
    JsonObject btn = row.createNestedObject();
    btn["text"] = keys[i][0];
    btn["url"] = keys[i][1];
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}

bool TeleBot::_request(const String &method, const String &params, 
                       String &response) {
  if (!_client->connect("api.telegram.org", 443)) {
    if (_debug) Serial.println("Connect FAIL");
    return false;
  }
  
  String req = "POST /bot" + String(_token) + "/" + method + " HTTP/1.1\r\n";
  req += "Host: api.telegram.org\r\n";
  req += "Content-Type: application/x-www-form-urlencoded\r\n";
  req += "Content-Length: " + String(params.length()) + "\r\n";
  req += "Connection: close\r\n\r\n";
  req += params;
  
  _client->print(req);
  
  unsigned long timeout = millis() + 5000;
  while (!_client->available() && millis() < timeout) {
    delay(10);
  }
  
  while (_client->available()) {
    String line = _client->readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  
  response = "";
  while (_client->available()) {
    response += _client->readString();
  }
  
  _client->stop();
  
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, response);
  
  if (!error && doc["ok"] == true) {
    return true;
  }
  
  return false;
}

String TeleBot::_encode(const String &str) {
  String encoded = "";
  char c;
  
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str[i];
    
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      encoded += '%';
      encoded += String((c >> 4) & 0xF, HEX);
      encoded += String(c & 0xF, HEX);
    }
  }
  
  return encoded;
}

void TeleBot::on(MsgHandler handler) {
  _msgHandler = handler;
}

void TeleBot::com(const String &command, MsgHandler handler) {
  if (_comCount < 15) {
    _comHandlers[_comCount].command = command;
    _comHandlers[_comCount].handler = handler;
    _comCount++;
  }
}

void TeleBot::inl(MsgHandler handler) {
  _inlineHandler = handler;
}

void TeleBot::server(unsigned long interval) {
  _checkTime = interval;
}

void TeleBot::debug(bool enable) {
  _debug = enable;
}

void TeleBot::useDNS(bool enable) {
  _useDNS = enable;
}

String TeleBot::lastError() {
  return _error;
}

long TeleBot::lastUpdate() {
  return _lastID;
}

// ==================== SD КАРТА МЕТОДЫ ====================

#ifdef TELEBOT_SD_ENABLE
bool TeleBot::initSD(int csPin, uint32_t freq) {
  if (_debug) {
    Serial.print("Initializing SD card... CS pin: ");
    Serial.println(csPin);
  }
  
  if (!SD.begin(csPin, SPI, freq)) {
    if (_debug) Serial.println("SD card initialization failed!");
    _error = "SD init failed";
    _sdInitialized = false;
    return false;
  }
  
  _sdInitialized = true;
  if (_debug) {
    Serial.println("SD card initialized successfully!");
    
    // Показываем информацию о карте
    uint8_t cardType = SD.cardType();
    Serial.print("SD Card Type: ");
    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
    } else if (cardType == CARD_MMC) {
      Serial.println("MMC");
    } else if (cardType == CARD_SD) {
      Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
  }
  
  return true;
}

bool TeleBot::initSD(const char* mountPoint) {
  _sdMountPoint = mountPoint;
  // Для ESP32 с внутренней файловой системой
  if (!SD.begin()) {
    if (_debug) Serial.println("SD card initialization failed!");
    _error = "SD init failed";
    _sdInitialized = false;
    return false;
  }
  
  _sdInitialized = true;
  if (_debug) Serial.println("SD card initialized!");
  return true;
}

String TeleBot::readSD(const String &path, FileType type) {
  if (!_sdInitialized) {
    _error = "SD not initialized";
    return "";
  }
  
  File file = SD.open(path);
  if (!file) {
    _error = "File not found: " + path;
    if (_debug) Serial.println("Failed to open file: " + path);
    return "";
  }
  
  String content = "";
  if (type == FILE_BIN) {
    // Для бинарных файлов читаем как байты
    while (file.available()) {
      content += (char)file.read();
    }
  } else {
    // Для текстовых файлов
    while (file.available()) {
      content += file.readString();
    }
  }
  
  file.close();
  
  if (_debug) {
    Serial.print("Read from SD: ");
    Serial.print(path);
    Serial.print(" (");
    Serial.print(content.length());
    Serial.println(" bytes)");
  }
  
  return content;
}

bool TeleBot::recordSD(const String &path, const String &data, FileType type) {
  if (!_sdInitialized) {
    _error = "SD not initialized";
    return false;
  }
  
  // Проверяем, существует ли директория
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash > 0) {
    String dirPath = path.substring(0, lastSlash);
    if (!SD.exists(dirPath)) {
      // Создаем директории рекурсивно
      String currentPath = "";
      int start = 0;
      while (start < dirPath.length()) {
        int end = dirPath.indexOf('/', start);
        if (end == -1) end = dirPath.length();
        
        currentPath += dirPath.substring(start, end);
        if (!SD.exists(currentPath)) {
          if (!SD.mkdir(currentPath)) {
            _error = "Failed to create directory: " + currentPath;
            return false;
          }
        }
        
        if (end < dirPath.length()) {
          currentPath += '/';
        }
        start = end + 1;
      }
    }
  }
  
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    _error = "Failed to create file: " + path;
    if (_debug) Serial.println("Failed to create file: " + path);
    return false;
  }
  
  size_t bytesWritten = file.print(data);
  file.close();
  
  if (bytesWritten == data.length()) {
    if (_debug) {
      Serial.print("Written to SD: ");
      Serial.print(path);
      Serial.print(" (");
      Serial.print(bytesWritten);
      Serial.println(" bytes)");
    }
    return true;
  } else {
    _error = "Write incomplete";
    if (_debug) {
      Serial.print("Write incomplete: ");
      Serial.print(bytesWritten);
      Serial.print(" of ");
      Serial.print(data.length());
      Serial.println(" bytes");
    }
    return false;
  }
}

bool TeleBot::appendSD(const String &path, const String &data) {
  if (!_sdInitialized) {
    _error = "SD not initialized";
    return false;
  }
  
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    _error = "Failed to open file: " + path;
    return false;
  }
  
  size_t bytesWritten = file.print(data);
  file.close();
  
  if (bytesWritten == data.length()) {
    if (_debug) {
      Serial.print("Appended to SD: ");
      Serial.print(path);
      Serial.print(" (");
      Serial.print(bytesWritten);
      Serial.println(" bytes)");
    }
    return true;
  }
  
  return false;
}

bool TeleBot::deleteSD(const String &path) {
  if (!_sdInitialized) {
    _error = "SD not initialized";
    return false;
  }
  
  if (!SD.exists(path)) {
    _error = "File not found: " + path;
    return false;
  }
  
  if (SD.rmdir(path)) {
    // Это директория
    if (_debug) Serial.println("Deleted directory: " + path);
    return true;
  } else if (SD.remove(path)) {
    // Это файл
    if (_debug) Serial.println("Deleted file: " + path);
    return true;
  } else {
    _error = "Failed to delete: " + path;
    return false;
  }
}

bool TeleBot::existsSD(const String &path) {
  if (!_sdInitialized) {
    _error = "SD not initialized";
    return false;
  }
  
  return SD.exists(path);
}

String TeleBot::listSD(const String &path) {
  if (!_sdInitialized) {
    _error = "SD not initialized";
    return "";
  }
  
  File root = SD.open(path);
  if (!root) {
    _error = "Failed to open directory: " + path;
    return "";
  }
  
  if (!root.isDirectory()) {
    root.close();
    _error = "Not a directory: " + path;
    return "";
  }
  
  String list = "Directory: " + path + "\n";
  list += "====================\n";
  
  File file = root.openNextFile();
  while (file) {
    list += file.name();
    if (file.isDirectory()) {
      list += "/ [DIR]\n";
    } else {
      list += " [";
      list += file.size();
      list += " bytes]\n";
    }
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  return list;
}

String TeleBot::extF() {
  return "txt, json, csv, log, ini, html, xml, bin, dat, cfg";
}
#endif

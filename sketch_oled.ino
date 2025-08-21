#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "config_module.h"
#include "web_module.h"
#include "oled_module.h"

#define I2C_SDA 4
#define I2C_SCL 5
#define RELAY_PIN 14
#define LED_PIN 2
#define OLED_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 32

Config config;
AsyncWebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

Adafruit_BMP280 bmp;
Adafruit_AHTX0 aht;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

bool relayState = false;
bool ledState = false;
float bmp_temp = 0, bmp_press = 0, aht_temp = 0, aht_hum = 0;
bool isOledPoweredOn = true;
unsigned long ledBlinkExpire = 0;       // 板载LED闪烁定时
unsigned long lastLedNormalBlink = 0;   // 1秒闪烁定时
unsigned long lastDebugPrint = 0;       // 调试信息定时
unsigned long lastLedOfflineBlink = 0;  // 断网频繁闪烁定时
int ledBlinkCount = 0;                  // 用于二次闪烁计数
unsigned long lastSend = 0;             // 数据发送间隔计时

// MQTT自动发现
void publishDiscovery() {
  if (!config.ha_mode || !mqttClient.connected()) return;
  
  String nodeId = config.device_name;
  String baseTopic = "homeassistant";
  
  // 温度传感器
  String temp_disc = baseTopic + "/sensor/" + nodeId + "_temperature/config";
  String temp_payload = "{\"name\":\"" + nodeId + " 温度\",\"state_topic\":\"" + baseTopic + "/sensor/" + nodeId + "/state\",\"unit_of_measurement\":\"°C\",\"value_template\":\"{{ value_json.aht_temp }}\",\"device_class\":\"temperature\"}";
  mqttClient.publish(temp_disc.c_str(), temp_payload.c_str(), true);

  // 湿度传感器
  String hum_disc = baseTopic + "/sensor/" + nodeId + "_humidity/config";
  String hum_payload = "{\"name\":\"" + nodeId + " 湿度\",\"state_topic\":\"" + baseTopic + "/sensor/" + nodeId + "/state\",\"unit_of_measurement\":\"%\",\"value_template\":\"{{ value_json.aht_hum }}\",\"device_class\":\"humidity\"}";
  mqttClient.publish(hum_disc.c_str(), hum_payload.c_str(), true);

  // 气压传感器
  String press_disc = baseTopic + "/sensor/" + nodeId + "_pressure/config";
  String press_payload = "{\"name\":\"" + nodeId + " 气压\",\"state_topic\":\"" + baseTopic + "/sensor/" + nodeId + "/state\",\"unit_of_measurement\":\"hPa\",\"value_template\":\"{{ value_json.bmp_press }}\",\"device_class\":\"pressure\"}";
  mqttClient.publish(press_disc.c_str(), press_payload.c_str(), true);

  // 继电器开关
  String relay_disc = baseTopic + "/switch/" + nodeId + "_relay/config";
  String relay_payload = "{\"name\":\"" + nodeId + " 继电器\",\"command_topic\":\"" + baseTopic + "/switch/" + nodeId + "/set\",\"state_topic\":\"" + baseTopic + "/switch/" + nodeId + "/state\"}";
  mqttClient.publish(relay_disc.c_str(), relay_payload.c_str(), true);
}

// 采集数据并发布
void publishSensorData() {
  if (!mqttClient.connected()) return;

  if (config.ha_mode) {
    // Home Assistant模式
    String payload = String("{\"bmp_temp\":") + bmp_temp +
                    ",\"bmp_press\":" + bmp_press +
                    ",\"aht_temp\":" + aht_temp +
                    ",\"aht_hum\":" + aht_hum + "}";
    String topic = "homeassistant/sensor/" + config.device_name + "/state";
    mqttClient.publish(topic.c_str(), payload.c_str(), true);
    
    // 发布继电器状态
    String relay_topic = "homeassistant/switch/" + config.device_name + "/state";
    mqttClient.publish(relay_topic.c_str(), relayState ? "ON" : "OFF", true);
  } else {
    // 自定义Topic模式
    String tempTopic = config.mqtt_topic + "/temperature";
    String humTopic = config.mqtt_topic + "/humidity";
    String pressTopic = config.mqtt_topic + "/pressure";
    String relayTopic = config.mqtt_topic + "/relay";

    mqttClient.publish(tempTopic.c_str(), String(aht_temp).c_str());
    mqttClient.publish(humTopic.c_str(), String(aht_hum).c_str());
    mqttClient.publish(pressTopic.c_str(), String(bmp_press).c_str());
    mqttClient.publish(relayTopic.c_str(), relayState ? "ON" : "OFF");
  }
}

// MQTT开关回调
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topStr = String(topic);
  
  if (config.ha_mode) {
    // Home Assistant模式
    String setTopic = "homeassistant/switch/" + config.device_name + "/set";
    if (topStr == setTopic) {
      if (payload[0] == '1' || (length == 2 && payload[0] == 'O' && payload[1] == 'N')) {
        onRelayChange(true);
      } else {
        onRelayChange(false);
      }
    }
  } else {
    // 自定义Topic模式
    String setTopic = config.mqtt_topic + "/relay/set";
    if (topStr == setTopic) {
      if (payload[0] == '1' || (length == 2 && payload[0] == 'O' && payload[1] == 'N')) {
        onRelayChange(true);
      } else {
        onRelayChange(false);
      }
    }
  }
}

// WiFi和MQTT自动重连
void reconnect() {
  // WiFi重连
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi连接断开，尝试重连...");
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());
    delay(1000);
    return;
  }
  
  // MQTT重连
  if (!mqttClient.connected()) {
    Serial.println("MQTT连接断开，尝试重连...");
    if (mqttClient.connect(config.device_name.c_str(), config.mqtt_user.c_str(), config.mqtt_pass.c_str())) {
      Serial.println("MQTT重连成功");
      
      if (config.ha_mode) {
        // 订阅Home Assistant主题
        String setTopic = "homeassistant/switch/" + config.device_name + "/set";
        mqttClient.subscribe(setTopic.c_str());
        publishDiscovery(); // 发布自动发现配置
      } else {
        // 订阅自定义主题
        String setTopic = config.mqtt_topic + "/relay/set";
        mqttClient.subscribe(setTopic.c_str());
      }
    }
  }
}

// MQTT连接函数
void connectToMqtt() {
  if (!mqttClient.connected()) {
    Serial.print("尝试连接到MQTT服务器: ");
    Serial.println(config.mqtt_server);

    if (mqttClient.connect(config.device_name.c_str(),
                           config.mqtt_user.c_str(),
                           config.mqtt_pass.c_str())) {
      Serial.println("MQTT连接成功");
      showStartupMessage(display, "MQTT Connected!");
      
      // 设置回调函数
      mqttClient.setCallback(mqttCallback);
      
      // 订阅主题
      if (config.ha_mode) {
        String setTopic = "homeassistant/switch/" + config.device_name + "/set";
        mqttClient.subscribe(setTopic.c_str());
        publishDiscovery(); // 发布自动发现配置
      } else {
        String setTopic = config.mqtt_topic + "/relay/set";
        mqttClient.subscribe(setTopic.c_str());
      }
      
      delay(1000);
    } else {
      Serial.print("MQTT连接失败, 错误代码: ");
      Serial.println(mqttClient.state());
      showStartupMessage(display, "MQTT Connected Failed");
      delay(1000);
    }
  }
}

// WiFi连接函数
void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    showStartupMessage(display, "Try to Connecting WIFI "+config.wifi_ssid);
    Serial.print("尝试连接到WiFi: ");
    Serial.println(config.wifi_ssid);

    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi连接成功");
      Serial.print("IP地址: ");
      Serial.println(WiFi.localIP());
      showStartupMessage(display, "WiFi Connected \nIP:" + WiFi.localIP().toString());
      delay(1000);
    } else {
      Serial.println("\nWiFi连接失败");
      showStartupMessage(display, "WiFi Connected Failed");
      delay(1000);
    }
  }
}

void debugPrint() {
  bool wifiConnected = (WiFi.status() == WL_CONNECTED) || (WiFi.localIP().toString() != "0.0.0.0");

  Serial.print("[调试] WiFi状态: ");
  Serial.print(wifiConnected ? "已连接" : "断开");
  Serial.print(" | IP: ");
  Serial.print(WiFi.localIP());
  Serial.print(" | MQTT: ");
  Serial.print(mqttClient.connected() ? "已连接" : "断开");
  Serial.print(" | 继电器: ");
  Serial.print(relayState ? "ON" : "OFF");
  Serial.print(" | LED: ");
  Serial.print(ledState ? "亮" : "灭");
  Serial.print(" | BMP280: ");
  Serial.print(bmp_temp);
  Serial.print("C, ");
  Serial.print(bmp_press);
  Serial.print("hPa");
  Serial.print(" | AHT20: ");
  Serial.print(aht_temp);
  Serial.print("C, ");
  Serial.print(aht_hum);
  Serial.println("%");
}

void setup() {
  Serial.begin(115200);

  // 初始化配置存储
  initConfigStorage();

  // 加载配置
  if (loadConfig(config)) {
    Serial.println("配置已从EEPROM加载");
  } else {
    Serial.println("使用默认配置");
  }

  Wire.begin(I2C_SDA, I2C_SCL);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);  // 板载LED默认灭

  // OLED初始化
  initOledDisplay(display);
  showStartupMessage(display, "Starting");

  // 传感器初始化
  if (!bmp.begin(0x77)) {
    Serial.println("BMP280初始化失败!");
    showStartupMessage(display, "BMP280 init Failed!");
    delay(1000);
  } else {
    Serial.println("BMP280 init Success");
  }

  if (!aht.begin()) {
    Serial.println("AHT20初始化失败!");
    showStartupMessage(display, "AHT20 initi failed");
    delay(1000);
  } else {
    Serial.println("AHT20 success");
  }

  // 使用配置中的MQTT服务器和端口设置MQTT客户端
  mqttClient.setServer(config.mqtt_server.c_str(), config.mqtt_port);
  mqttClient.setCallback(mqttCallback); // 设置回调函数

  // 连接到WiFi和MQTT
  connectToWiFi();
  connectToMqtt();

  // 设置Web服务器
  setupWebConfig(server, config);
  server.begin();

  showStartupMessage(display, "system Started@");
  delay(1000);
}

void setLED(bool on) {
  digitalWrite(LED_PIN, on ? LOW : HIGH);
  ledState = on;
}

void ledDoubleBlink() {
  ledBlinkCount = 4;          // 2次闪烁（亮灭亮灭）
  ledBlinkExpire = millis();  // 立即开始
}

void onRelayChange(bool state) {
  relayState = state;
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
  ledDoubleBlink();  // 继电器变化时LED闪烁2次

  // 发布MQTT消息
  if (mqttClient.connected()) {
    if (config.ha_mode) {
      String relay_topic = "homeassistant/switch/" + config.device_name + "/state";
      mqttClient.publish(relay_topic.c_str(), relayState ? "ON" : "OFF", true);
    } else {
      String topic = config.mqtt_topic + "/relay";
      String payload = relayState ? "ON" : "OFF";
      mqttClient.publish(topic.c_str(), payload.c_str());
    }
  }
}

void loop() {
  // 采集传感器数据
  bmp_temp = bmp.readTemperature();
  bmp_press = bmp.readPressure() / 100.0F;
  sensors_event_t aht_hum_event, aht_temp_event;
  aht.getEvent(&aht_hum_event, &aht_temp_event);
  aht_temp = aht_temp_event.temperature;
  aht_hum = aht_hum_event.relative_humidity;

  // 定期重新连接WiFi和MQTT
  static unsigned long lastReconnectAttempt = 0;
  if (millis() - lastReconnectAttempt > 10000) {
    lastReconnectAttempt = millis();
    reconnect(); // 使用新的重连函数
  }

  // 定期发布传感器数据到MQTT
  static unsigned long lastMqttPublish = 0;
  if (mqttClient.connected() && millis() - lastMqttPublish > 10000) {
    lastMqttPublish = millis();
    publishSensorData(); // 使用新的数据发布函数
  }

  // 板载LED控制逻辑
  bool wifiOk = (WiFi.status() == WL_CONNECTED) || (WiFi.localIP().toString() != "0.0.0.0");

  // 断网时高频闪烁
  if (!wifiOk) {
    if (millis() - lastLedOfflineBlink > 250) {  // 断网每0.25秒闪烁
      setLED(!ledState);
      lastLedOfflineBlink = millis();
    }
  } else {
    // 普通1秒闪烁
    if (millis() - lastLedNormalBlink > 1000 && ledBlinkCount == 0) {
      setLED(!ledState);
      lastLedNormalBlink = millis();
    }
  }

  // 二次闪烁（继电器状态变化时触发）
  if (ledBlinkCount > 0 && millis() - ledBlinkExpire >= 150) {
    setLED(!ledState);
    ledBlinkExpire = millis();
    ledBlinkCount--;
  }

  // 调试信息每3秒打印一次
  if (millis() - lastDebugPrint > 3000) {
    debugPrint();
    lastDebugPrint = millis();
  }

  if (config.oled_enable) {
    // OLED显示传感器信息
    if (!isOledPoweredOn) {
      display.ssd1306_command(SSD1306_DISPLAYON);  // 开启显示
      display.clearDisplay();
      display.display();
    }
    updateOledDisplay(display, bmp_temp, bmp_press, aht_temp, aht_hum, WiFi.localIP().toString());
    isOledPoweredOn = true;
  } else {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);  // 关闭显示
    isOledPoweredOn = false;
  }

  mqttClient.loop();
}
#ifndef CONFIG_MODULE_H
#define CONFIG_MODULE_H

#include <EEPROM.h>
#include <Arduino.h>

struct Config {
  String wifi_ssid = "";
  String wifi_pass = "";
  String mqtt_server = "";
  int mqtt_port = 1883;
  String mqtt_user = "";
  String mqtt_pass = "";
  String device_name = "esp8266sensor";
  String mqtt_topic = "test/topic";
  bool ha_mode = true;
  bool oled_enable = true;
  bool oled_inverse = false;
};

// 配置存储签名，用于验证数据有效性
#define CONFIG_SIGNATURE 0xAA55
#define EEPROM_SIZE 512  // 增加EEPROM大小

// 配置存储结构
struct ConfigData {
  uint16_t signature;
  char wifi_ssid[32];
  char wifi_pass[32];
  char mqtt_server[32];
  int mqtt_port;
  char mqtt_user[32];
  char mqtt_pass[32];
  char device_name[32];
  char mqtt_topic[64];
  bool ha_mode;
  bool oled_enable;
  bool oled_inverse;
};

// 函数声明
void saveConfig(const Config& config);
bool loadConfig(Config& config);
void initConfigStorage();

// 函数实现
inline void saveConfig(const Config& config) {
  ConfigData data;
  
  // 设置签名
  data.signature = CONFIG_SIGNATURE;
  
  // 安全地复制字符串数据，确保null终止
  strncpy(data.wifi_ssid, config.wifi_ssid.c_str(), sizeof(data.wifi_ssid) - 1);
  data.wifi_ssid[sizeof(data.wifi_ssid) - 1] = '\0';
  
  strncpy(data.wifi_pass, config.wifi_pass.c_str(), sizeof(data.wifi_pass) - 1);
  data.wifi_pass[sizeof(data.wifi_pass) - 1] = '\0';
  
  strncpy(data.mqtt_server, config.mqtt_server.c_str(), sizeof(data.mqtt_server) - 1);
  data.mqtt_server[sizeof(data.mqtt_server) - 1] = '\0';
  
  data.mqtt_port = config.mqtt_port;
  
  strncpy(data.mqtt_user, config.mqtt_user.c_str(), sizeof(data.mqtt_user) - 1);
  data.mqtt_user[sizeof(data.mqtt_user) - 1] = '\0';
  
  strncpy(data.mqtt_pass, config.mqtt_pass.c_str(), sizeof(data.mqtt_pass) - 1);
  data.mqtt_pass[sizeof(data.mqtt_pass) - 1] = '\0';
  
  strncpy(data.device_name, config.device_name.c_str(), sizeof(data.device_name) - 1);
  data.device_name[sizeof(data.device_name) - 1] = '\0';
  
  strncpy(data.mqtt_topic, config.mqtt_topic.c_str(), sizeof(data.mqtt_topic) - 1);
  data.mqtt_topic[sizeof(data.mqtt_topic) - 1] = '\0';
  
  data.ha_mode = config.ha_mode;
  data.oled_enable = config.oled_enable;
  data.oled_inverse = config.oled_inverse;
  
  // 写入EEPROM - 使用更安全的方式
  EEPROM.begin(EEPROM_SIZE);
  
  // 逐字节写入，避免内存对齐问题
  uint8_t* data_ptr = (uint8_t*)&data;
  for (size_t i = 0; i < sizeof(ConfigData); i++) {
    EEPROM.write(i, data_ptr[i]);
  }
  
  bool success = EEPROM.commit();
  EEPROM.end();
  
  if (success) {
    Serial.println("配置已保存到EEPROM");
  } else {
    Serial.println("EEPROM保存失败");
  }
}

inline bool loadConfig(Config& config) {
  ConfigData data;
  
  // 从EEPROM读取 - 使用更安全的方式
  EEPROM.begin(EEPROM_SIZE);
  
  // 逐字节读取
  uint8_t* data_ptr = (uint8_t*)&data;
  for (size_t i = 0; i < sizeof(ConfigData); i++) {
    data_ptr[i] = EEPROM.read(i);
  }
  
  EEPROM.end();
  
  // 验证签名
  if (data.signature != CONFIG_SIGNATURE) {
    Serial.println("未找到有效配置，使用默认值");
    return false;
  }
  
  // 复制到配置对象
  config.wifi_ssid = String(data.wifi_ssid);
  config.wifi_pass = String(data.wifi_pass);
  config.mqtt_server = String(data.mqtt_server);
  config.mqtt_port = data.mqtt_port;
  config.mqtt_user = String(data.mqtt_user);
  config.mqtt_pass = String(data.mqtt_pass);
  config.device_name = String(data.device_name);
  config.mqtt_topic = String(data.mqtt_topic);
  config.ha_mode = data.ha_mode;
  config.oled_enable = data.oled_enable;
  config.oled_inverse = data.oled_inverse;
  
  Serial.println("配置已从EEPROM加载");
  return true;
}

inline void initConfigStorage() {
  EEPROM.begin(EEPROM_SIZE);
  
  // 检查是否需要初始化
  uint16_t signature;
  uint8_t* sig_ptr = (uint8_t*)&signature;
  sig_ptr[0] = EEPROM.read(0);
  sig_ptr[1] = EEPROM.read(1);
  
  if (signature != CONFIG_SIGNATURE) {
    Serial.println("初始化EEPROM存储");
    // 清空配置区域
    ConfigData emptyData = {0};
    uint8_t* empty_ptr = (uint8_t*)&emptyData;
    
    for (size_t i = 0; i < sizeof(ConfigData); i++) {
      EEPROM.write(i, empty_ptr[i]);
    }
    
    EEPROM.commit();
  }
  
  EEPROM.end();
}

#endif
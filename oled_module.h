#ifndef OLED_MODULE_H
#define OLED_MODULE_H

#include <Adafruit_SSD1306.h>
#include "config_module.h"

// 屏幕参数
#define OLED_WIDTH 128
#define OLED_HEIGHT 32

// OLED显示参数
#define OLED_INVERT_INTERVAL 5000   // 反色切换间隔（5秒）

unsigned long oledLastInvert = 0;
bool oledInverted = false;

// 启动信息显示
void showStartupMessage(Adafruit_SSD1306 &display, const String &message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(message);
  display.display();
}

// 传感器信息显示
void showSensorInfo(Adafruit_SSD1306 &display, float bmp_temp, float bmp_press, float aht_temp, float aht_hum, const String &ipAddress, bool inverted) {
  // 设置文本颜色和背景
  display.setTextColor(inverted ? SSD1306_BLACK : SSD1306_WHITE, 
                       inverted ? SSD1306_WHITE : SSD1306_BLACK);
  
  // 清屏并填充背景
  if (inverted) {
    display.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
  } else {
    display.clearDisplay();
  }
  
  display.setTextSize(1);

  // BMP280数据 - 第一行
  display.setCursor(1, 1);
  display.print("BMP ");
  display.print(bmp_temp, 2);
  display.print("C ");
  display.print(bmp_press, 2);
  display.print("hPa");

  // AHT20数据 - 第二行
  display.setCursor(1, 12);
  display.print("AHT ");
  display.print(aht_temp, 2);
  display.print("C ");
  display.print(aht_hum, 2);
  display.print("%");

  // IP地址 - 第三行
  display.setCursor(1, 23);
  display.print("IP:");
  display.print(ipAddress);
  
  display.display();
}

// 主OLED显示函数
void updateOledDisplay(Adafruit_SSD1306 &display, float bmp_temp, float bmp_press, float aht_temp, float aht_hum, const String &ipAddress) {
  unsigned long now = millis();
  
  // 每5秒切换反色显示
  if (now - oledLastInvert > OLED_INVERT_INTERVAL) {
    oledInverted = !oledInverted;
    oledLastInvert = now;
  }
  
  // 显示传感器信息
  showSensorInfo(display, bmp_temp, bmp_press, aht_temp, aht_hum, ipAddress, oledInverted);
}

// 初始化OLED显示
void initOledDisplay(Adafruit_SSD1306 &display) {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED初始化失败!");
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

#endif
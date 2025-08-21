#ifndef WEB_MODULE_H
#define WEB_MODULE_H

#define RELAY_PIN 14
#define LED_PIN 2

#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include "config_module.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

extern PubSubClient mqttClient;
extern bool relayState;
extern float bmp_temp, bmp_press, aht_temp, aht_hum;
extern bool ledState;
extern void onRelayChange(bool state);

// 状态页
String statusPageHtml(const Config& config) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>设备状态</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function updateStatus() {
      fetch("/api/status")
        .then(r => r.json())
        .then(data => {
          document.getElementById('wifi_state').innerHTML = data.wifi_connected ?
            "<span class='status-dot status-ok'></span>已连接" : "<span class='status-dot status-bad'></span>断开";
          document.getElementById('ip_addr').innerText = data.ip;
          document.getElementById('mqtt_state').innerHTML = data.mqtt_connected ?
            "<span class='status-dot status-ok'></span>已连接" : "<span class='status-dot status-bad'></span>断开";
          document.getElementById('device_name').innerText = data.device_name || '';
          document.getElementById('mqtt_mode').innerText = data.mqtt_mode || '';
          document.getElementById('mqtt_topic').innerText = data.mqtt_topic || '';
          document.getElementById('oled_enable').innerText = data.oled_enable ? "开启" : "关闭";
          // 反显功能已去除
          document.getElementById('bmp_temp').innerText = (data.bmp_temp||0).toFixed(2) + " °C";
          document.getElementById('bmp_press').innerText = (data.bmp_press||0).toFixed(2) + " hPa";
          document.getElementById('aht_temp').innerText = (data.aht_temp||0).toFixed(2) + " °C";
          document.getElementById('aht_hum').innerText = (data.aht_hum||0).toFixed(2) + " %";
          document.getElementById('relay_state').innerText = data.relay_state ? "ON" : "OFF";
          document.getElementById('led_state').innerHTML = data.led_state ?
            "<span class='status-dot status-ok'></span>亮" : "<span class='status-dot status-bad'></span>灭";
        });
    }
    window.onload = function() {
      updateStatus();
      setInterval(updateStatus, 2000);
    };
  </script>
  <style>
    body{font-family:'Segoe UI',Arial,sans-serif;background:#f6f8fa;margin:0;}
    .container{max-width:520px;margin:30px auto;background:#fff;border-radius:8px;box-shadow:0 0 16px #ddd;padding:24px;}
    h2{color:#3949ab;}
    ul{list-style:none;padding:0;}
    li{margin:10px 0;font-size:1.1em;}
    .label{display:inline-block;width:115px;font-weight:bold;color:#666;}
    .status-dot{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:7px;vertical-align:middle;}
    .status-ok{background:#43a047;}
    .status-bad{background:#e53935;}
    .relaybtn{background:#3949ab;color:#fff;border:none;border-radius:4px;padding:8px 22px;font-size:1em;cursor:pointer;margin:8px 0 16px 0;}
    .relaybtn:hover{background:#5c6bc0;}
  </style>
</head>
<body>
  <div class="container">
    <h2>设备状态</h2>
    <div class="section">
      <ul>
        <li><span class="label">WiFi:</span> <span id="wifi_state"></span></li>
        <li><span class="label">IP地址:</span> <span id="ip_addr"></span></li>
        <li><span class="label">MQTT:</span> <span id="mqtt_state"></span></li>
        <li><span class="label">设备名:</span> <span id="device_name"></span></li>
        <li><span class="label">MQTT模式:</span> <span id="mqtt_mode"></span></li>
        <li><span class="label">MQTT Topic:</span> <span id="mqtt_topic"></span></li>
        <li><span class="label">屏幕状态:</span> <span id="oled_enable"></span></li>
        <!-- 反显功能已去除 -->
        <li><span class="label">板载LED:</span> <span id="led_state"></span></li>
      </ul>
    </div>
    <div class="section">
      <h3>传感器</h3>
      <ul>
        <li><span class="label">BMP280温度:</span> <span id="bmp_temp"></span></li>
        <li><span class="label">BMP280气压:</span> <span id="bmp_press"></span></li>
        <li><span class="label">AHT20温度:</span> <span id="aht_temp"></span></li>
        <li><span class="label">AHT20湿度:</span> <span id="aht_hum"></span></li>
      </ul>
    </div>
    <div class="section">
      <h3>继电器控制</h3>
      <form method="POST" action="/relay">
        <button class="relaybtn" type="submit" name="relay" value="on">打开继电器</button>
        <button class="relaybtn" type="submit" name="relay" value="off">关闭继电器</button>
      </form>
      <div>当前状态：<b><span id="relay_state"></span></b></div>
      <div style="margin-top:14px;"><a href="/setup">参数配置</a></div>
    </div>
    <div style="margin-top:24px;color:#aaa;font-size:0.95em;text-align:center;">Copilot智能物联网开发演示</div>
  </div>
</body>
</html>
)rawliteral";
  return html;
}

// 参数配置页
String setupPageHtml(const Config& config) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>参数配置</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body{font-family:'Segoe UI',Arial,sans-serif;background:#f6f8fa;margin:0;}
    .container{max-width:520px;margin:30px auto;background:#fff;border-radius:8px;box-shadow:0 0 16px #ddd;padding:24px;}
    h2{color:#3949ab;}
    .section{margin-bottom:34px;}
    label{display:inline-block;width:115px;}
    input,select{padding:4px 8px;margin-bottom:10px;}
    .switch{position:relative;display:inline-block;width:44px;height:24px;}
    .switch input{opacity:0;width:0;height:0;}
    .slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;-webkit-transition:.4s;transition:.4s;}
    .slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background:white;-webkit-transition:.4s;transition:.4s;}
    input:checked+.slider{background:#3949ab;}
    input:checked+.slider:before{transform:translateX(20px);}
    .slider.round{border-radius:24px;}
    .slider.round:before{border-radius:50%;}
    button{background:#3949ab;color:#fff;border:none;border-radius:4px;padding:8px 22px;font-size:1em;cursor:pointer;}
    button:hover{background:#5c6bc0;}
  </style>
  <script>
    function toggleTopicInput() {
      var mode = document.getElementById('mqtt_mode').value;
      var topicRow = document.getElementById('topic_row');
      if(mode === 'topic') {
        topicRow.style.display = '';
      } else {
        topicRow.style.display = 'none';
      }
    }
    window.onload = function() {
      toggleTopicInput();
      document.getElementById('mqtt_mode').addEventListener('change', toggleTopicInput);
    }
  </script>
</head>
<body>
  <div class="container">
    <h2>参数配置</h2>
    <form method="POST" action="/setup">
      <label>WiFi SSID:</label>
      <input name="wifi_ssid" value="{wifi_ssid}"><br>
      <label>WiFi 密码:</label>
      <input name="wifi_pass" type="password" value="{wifi_pass}"><br>
      <label>MQTT服务器:</label>
      <input name="mqtt_server" value="{mqtt_server}"><br>
      <label>MQTT端口:</label>
      <input name="mqtt_port" type="number" value="{mqtt_port}"><br>
      <label>MQTT用户名:</label>
      <input name="mqtt_user" value="{mqtt_user}"><br>
      <label>MQTT密码:</label>
      <input name="mqtt_pass" type="password" value="{mqtt_pass}"><br>
      <label>设备名称:</label>
      <input name="device_name" value="{device_name}"><br>
      <label>MQTT模式:</label>
      <select name="mqtt_mode" id="mqtt_mode">
        <option value="ha" {ha_selected}>HA传感器</option>
        <option value="topic" {topic_selected}>自定义Topic</option>
      </select><br>
      <div id="topic_row">
        <label>自定义Topic:</label>
        <input name="mqtt_topic" value="{mqtt_topic}">
      </div>
      <label>OLED屏幕:</label>
      <label class="switch"><input name="oled_enable" type="checkbox" {oled_enable_checked}><span class="slider round"></span></label><br>
      <!-- 反显功能已去除 -->
      <button type="submit">保存配置</button>
    </form>
    <div style="margin-top:24px;color:#aaa;font-size:0.95em;text-align:center;">Copilot智能物联网开发演示</div>
  </div>
</body>
</html>
)rawliteral";
  html.replace("{wifi_ssid}", config.wifi_ssid);
  html.replace("{wifi_pass}", config.wifi_pass);
  html.replace("{mqtt_server}", config.mqtt_server);
  html.replace("{mqtt_port}", String(config.mqtt_port));
  html.replace("{mqtt_user}", config.mqtt_user);
  html.replace("{mqtt_pass}", config.mqtt_pass);
  html.replace("{device_name}", config.device_name);
  html.replace("{mqtt_topic}", config.mqtt_topic);
  html.replace("{ha_selected}", config.ha_mode ? "selected" : "");
  html.replace("{topic_selected}", config.ha_mode ? "" : "selected");
  html.replace("{oled_enable_checked}", config.oled_enable ? "checked" : "");
  return html;
}

// API接口
void setupWebConfig(AsyncWebServer& server, Config& config) {
  server.on("/", HTTP_GET, [&config](AsyncWebServerRequest *request){
    request->send(200, "text/html", statusPageHtml(config));
  });

  server.on("/setup", HTTP_GET, [&config](AsyncWebServerRequest *request){
    request->send(200, "text/html", setupPageHtml(config));
  });

  server.on("/setup", HTTP_POST, [&config](AsyncWebServerRequest *request){
    if (request->hasParam("wifi_ssid", true)) config.wifi_ssid = request->getParam("wifi_ssid", true)->value();
    if (request->hasParam("wifi_pass", true)) config.wifi_pass = request->getParam("wifi_pass", true)->value();
    if (request->hasParam("mqtt_server", true)) config.mqtt_server = request->getParam("mqtt_server", true)->value();
    if (request->hasParam("mqtt_port", true)) config.mqtt_port = request->getParam("mqtt_port", true)->value().toInt();
    if (request->hasParam("mqtt_user", true)) config.mqtt_user = request->getParam("mqtt_user", true)->value();
    if (request->hasParam("mqtt_pass", true)) config.mqtt_pass = request->getParam("mqtt_pass", true)->value();
    if (request->hasParam("device_name", true)) config.device_name = request->getParam("device_name", true)->value();
    if (request->hasParam("mqtt_mode", true)) config.ha_mode = (request->getParam("mqtt_mode", true)->value() == "ha");
    if (request->hasParam("mqtt_topic", true)) config.mqtt_topic = request->getParam("mqtt_topic", true)->value();
    if (request->hasParam("oled_enable", true)) config.oled_enable = true; else config.oled_enable = false;
    saveConfig(config);
    request->send(200, "text/plain", "配置保存成功，设备重启中...");
    delay(800);
    ESP.restart();
  });

  server.on("/relay", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("relay", true)) {
      String val = request->getParam("relay", true)->value();
      onRelayChange(val == "on");
    }
    request->redirect("/");
  });

  // 动态数据API接口
  server.on("/api/status", HTTP_GET, [&config](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc;
    doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
    doc["ip"] = WiFi.localIP().toString();
    doc["mqtt_connected"] = mqttClient.connected();
    doc["device_name"] = config.device_name;
    doc["mqtt_mode"] = config.ha_mode ? "HA传感器" : "自定义Topic";
    doc["mqtt_topic"] = config.ha_mode ? ("homeassistant/sensor/" + config.device_name + "/state") : config.mqtt_topic;
    doc["oled_enable"] = config.oled_enable;
    doc["bmp_temp"] = bmp_temp;
    doc["bmp_press"] = bmp_press;
    doc["aht_temp"] = aht_temp;
    doc["aht_hum"] = aht_hum;
    doc["relay_state"] = relayState;
    doc["led_state"] = ledState;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  
}

#endif
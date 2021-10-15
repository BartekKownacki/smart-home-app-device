#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <ArduinoJson.h>
#include <FS.h>

ESP8266WebServer server(80);

String ssid = "";
String password = "";
IPAddress device_ip, host_ip, subnet, primary_dns, secondary_dns, test_host_ip;

IPAddress getClientIpAddress() {
  IPAddress clientIp = server.client().remoteIP();
  return clientIp;
}

String ip2Str(IPAddress ip){
  String response = "";
  response += ip[0]; response += ".";
  response += ip[1]; response += ".";
  response += ip[2]; response += ".";
  response += ip[3]; 
  return response;
}

int getIpBlock(int index, String str) {
    char separator = '.';
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = str.length()-1;
    for(int i=0; i<=maxIndex && found<=index; i++){
      if(str.charAt(i)==separator || i==maxIndex){
          found++;
          strIndex[0] = strIndex[1]+1;
          strIndex[1] = (i == maxIndex) ? i+1 : i;
      }
    }
    return found>index ? str.substring(strIndex[0], strIndex[1]).toInt() : 0;
}

IPAddress str2IP(String str) {
    IPAddress ret( getIpBlock(0,str),getIpBlock(1,str),getIpBlock(2,str),getIpBlock(3,str) );
    return ret;
}

bool readConfigFile() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }
  
  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);
  StaticJsonDocument<200> doc;
  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }
  ssid = (String)doc["ssid"];
  password = (String)doc["wifi_password"];
  device_ip = str2IP(doc["device_ip"]);
  host_ip = str2IP(doc["host_ip"]);
  subnet = str2IP(doc["subnet"]);
  primary_dns = str2IP(doc["primary_dns"]);
  secondary_dns = str2IP(doc["secondary_dns"]);
  test_host_ip = str2IP(doc["test_host_ip"]);
  return true;
}

void getTestResponse() {
  if(getClientIpAddress() != test_host_ip){
    server.send(400, "text/json", "{\"error\": \"400\"}");
  }
  else{
    server.send(200, "text/json", "{\"json_field\": \"json field value\"}");
  }
}

void postTestResponse(){
  String postBody = server.arg("plain");
  Serial.println(postBody);

  DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, postBody);
    if (error) {
        // if the file didn't open, print an error:
        Serial.print(F("Error parsing JSON "));
        Serial.println(error.c_str());
 
        String msg = error.c_str();
 
        server.send(400, F("text/html"),
                "Error in parsin json body! <br>" + msg);
 
    } else {
        JsonObject postObj = doc.as<JsonObject>();
        
        Serial.print(F("HTTP Method: "));
        Serial.println(server.method());
 
        if (server.method() == HTTP_POST) {
            if (postObj.containsKey("message")){
                const String bodyMessage = (String)postObj["message"];
                Serial.println(F("done."));
 
                DynamicJsonDocument doc(512);
                doc["status"] = "OK";
                doc["received message"] = bodyMessage ;
 
                Serial.print(F("Stream..."));
                String buf;
                serializeJson(doc, buf);
 
                server.send(201, F("application/json"), buf);
                Serial.print(F("done."));
 
            }else {
                DynamicJsonDocument doc(512);
                doc["status"] = "KO";
                doc["message"] = F("No data found, or incorrect!");
 
                Serial.print(F("Stream..."));
                String buf;
                serializeJson(doc, buf);
 
                server.send(400, F("application/json"), buf);
                Serial.print(F("done."));
            }
        }
    }
}

void getIndexResponse() {
    server.send(200, "text/json", ("{\"clientIpAddress\":\"" +  ip2Str(getClientIpAddress()) + "\"}"));
}
 
// Define routing
void restServerRouting() {
    server.on(F("/"), HTTP_GET, getIndexResponse);
    server.on(F("/testGet"), HTTP_GET, getTestResponse);
    server.on(F("/testPost"), HTTP_POST, postTestResponse);
}
 
// Manage not found URL
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
 
void setup(void) {
  Serial.begin(115200);

  Serial.println("");
   bool success = SPIFFS.begin();

  if (success) {
    Serial.println("SPIFF success");
    if(readConfigFile()){
        if (!WiFi.config(device_ip, host_ip, subnet, primary_dns, secondary_dns)) {
          Serial.println("STA Failed to configure");
        }
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
        }
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
       
        // Set server routing
        restServerRouting();
        // Set not found response
        server.onNotFound(handleNotFound);
        // Start server
        server.begin();
        Serial.println("HTTP server started");
      }
  }
  else {
    Serial.println("SPIFF error");
  }

  
}
 
void loop(void) {
  server.handleClient();
}

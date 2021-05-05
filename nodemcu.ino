#include <SoftwareSerial.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);

int suhu, tds, air, ldr, settds;
float ph, setphup, setphdown;
String arrData[25];

unsigned long previousMillis = 0;
const long interval = 1000;

// variables
int i = 0;
int statusCode;
String content, st;

// variable eeprom
String esid, epass;

WebSocketsServer webSocket = WebSocketsServer(81);
SoftwareSerial DataSerial (12, 13); //rt dan tx pin
// Establishing Local server at port 80 whenever required
ESP8266WebServer server(80);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                // send message to client
                webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
           {
              webSocket.broadcastTXT(payload);
              DataSerial.printf("%s\n",payload);
           }            
            break;
            case WStype_BIN:
            Serial.printf("[%u] get binary length: %u\n", num, length);
            hexdump(payload, length);
            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
    }
}

void setup() {
  Serial.begin(9600);
  DataSerial.begin(9600);

  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(512); //Initializing EEPROM
  delay(10);

  readEEPROM(); // read eeprom

  WiFi.begin(esid.c_str(), epass.c_str());

  (testWifi()) ? Serial.println("Succesfully Connected!!!") : Serial.println("Cannot Connect Wifi");

  setupAP();// Setup HotSpot
  
  if((WiFi.status() == WL_CONNECTED)){
    // LCD begin 
    lcd.backlight();
    lcd.init();
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    lcd.setCursor(0,0);
    lcd.print(" --HYDROXTECH-- ");
    lcd.setCursor(0,1);
    lcd.print(" ");
    lcd.print(WiFi.localIP());

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
  }
}

void readEEPROM(void){
  // Read eeprom
  Serial.println("============= READ EEPROM =============");
  for (int i = 0; i < 32; ++i)
    esid += char(EEPROM.read(i));

  Serial.println("SSID : " + esid);

  for (int i = 32; i < 64; ++i)
    epass += char(EEPROM.read(i));

  Serial.println("Password : " + epass);
  Serial.println("============= READ EEPROM =============");
}

void writeEEPROM(
  String qsid = "", 
  String qpass = ""
)
{
  if(qsid.length() > 0) {
    for (int i = 0; i < 32; ++i)
      EEPROM.write(i, 0);

    for (int i = 0; i < qsid.length(); ++i)
      EEPROM.write(i, qsid[i]);
  }
  if(qpass.length() > 0){
    for (int i = 32; i < 64; ++i)
      EEPROM.write(i, 0);

    for (int i = 0; i < qpass.length(); ++i)
      EEPROM.write(32 + i, qpass[i]);
  } 
  EEPROM.commit();
}

bool testWifi(void) {
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
  Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}

void scanNetworks(void){
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(WiFi.SSID(i));
      delay(10);
    }
  }

  Serial.println("");
  st = "[";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID for each network found
    st += "\"";
    st += WiFi.SSID(i);
    st += "\"";
    if(i != (n - 1)) st += ",";
  }
  st += "]";
}

void setupAP(void){
  WiFi.mode(WIFI_STA);
  scanNetworks();
  delay(100);
  WiFi.softAP("Hydro x tech", "");
  Serial.println("Initializing_Wifi_accesspoint");
  launchWeb();
}

void createWebServer()
{
 {
    server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>ESP8266 WiFi Connectivity Setup ";
      content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });

    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");

      if(
        qsid.length() > 0 ||
        qpass.length() > 0
      ){
        writeEEPROM(qsid,qpass);
              
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;

        ESP.reset();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
     
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
    });
  } 
}

void loop() {
  
  server.handleClient();
  if ((WiFi.status() == WL_CONNECTED))
  {
  webSocket.loop();
  // config millis
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= interval){
    previousMillis = currentMillis;

    //data dari arduino uno
    String data = "";
    while(DataSerial.available()>0)
    {
      data += char(DataSerial.read());
    }
    data.trim();

    if(data != "")
    {
      int index = 0;
      for(int i=0; i<= data.length(); i++)
      {
       char delimiter = '#' ;
       if(data[i] != delimiter)
        arrData[index] += data[i] ;
       else
        index++;  
      }
      // cek data
      if(index == 4)
      {
        webSocket.broadcastTXT("sh:"+ arrData[0] + ",tds:" + arrData[1] + ",ldr:" + arrData[2]+ ",ta:" + arrData[3]+ ",ph:" + arrData[4]);
      }    

        suhu = arrData[0].toInt();
        tds  = arrData[1].toInt();
        ldr  = arrData[2].toInt();
        air  = arrData[3].toInt();
        ph   = arrData[4].toFloat();
        
        arrData[0]= "";
        arrData[1]= "";
        arrData[2]= "";
        arrData[3]= "";
        arrData[4]= "";
      }
      //req data ke arduino uno
      DataSerial.println("send");
    }
  }
}

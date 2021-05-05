#include <stdio.h>
#include <string.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

// SET LCD 20x4
LiquidCrystal_I2C lcd(0x27,20,4);

/* SENSOR TEMP SETTING */
#define ONE_WIRE_BUS 2 // Data wire is plugged into digital pin 2 on the Arduino
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire device
DallasTemperature sensors(&oneWire); // Pass oneWire reference to DallasTemperature library
/* SENSOR PH SETTING */
#define pHAnalog A0 // pH meter Analog output to Arduino Analog Input 0
#define pHArrayLength 5 // times of collection
/* SENSOR TDS SETTING */
#define tdsAnalog A1
#define VREF 5.0 // analog reference voltage(Volt) of the ADC
#define SCOUNT 5 // sum of sample point
/* SENSOR LDR SETTING */
#define ldrAnalog A3
/* SENSOR WATER LEVEL */
#define sensorPower 12
#define sensorPin A2
/* RELAY SETTING */
#define tdsRelay 6
#define phupRelay 7
#define phdownRelay 5
#define lampRelay 11

/* VAR DATA FROM NODEMCU */
char mystr[20]; 
char vD[45];
int j =0;
/* STATUS BUTTON IN USER */
String relayTds,relayLamp ;
/* CONTROL SETTING & SENSOR CALIBRATION */
float pHCal, tdsCal;
float pHMax, pHMin, tdsMin;
/* SENSOR LDR VAR */
int ldrValue;
String ldrStatus;
/* SENSOR TDS VAR */
int analogBuffer[SCOUNT]; // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0, tdsValue = 0;
/* SENSOR PH VAR */
float phValue;
int pHArray[pHArrayLength];
int pHArrayIndex = 0;
/* SENSOR TEMP VAR */
float temp;
/* SENSOR W LEVEL VAR */
int Wlevel;
int val = 0;

void setup() {
  Serial.begin(9600);
  // Temp sensor begin
  sensors.begin();
  // Tds sensor begin
  pinMode(tdsAnalog,INPUT);
  // Set D12 as an OUTPUT
  pinMode(sensorPower, OUTPUT);
  // Set to LOW so no power flows through the sensor PIN A2
  digitalWrite(sensorPower, LOW);

  // Relay begin
  pinMode(tdsRelay,OUTPUT);
  digitalWrite(tdsRelay,HIGH);

  pinMode(phupRelay,OUTPUT);
  digitalWrite(phupRelay,HIGH);

  pinMode(phdownRelay,OUTPUT);
  digitalWrite(phdownRelay,HIGH);

  pinMode(lampRelay,OUTPUT);
  digitalWrite(lampRelay,HIGH);

  // lcd begin
  lcd.init();
  // turn on backlight and print message
  lcd.backlight();

}

void loop() { 
  setTemp();
  setpH();
  setTds();
  setLdr();
  setWheight();
  relayController();
  displayLcd();
  
  getDataSerial();
  // displayDebug();
  delay(1000);
}

template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++)
        EEPROM.write(ee++, *p++);
    return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++)
        *p++ = EEPROM.read(ee++);
    return i;
}
void readEEPROM(){
  // Read eeprom
  EEPROM_readAnything(15,pHCal);
  EEPROM_readAnything(25,tdsCal);
  EEPROM_readAnything(35,pHMax);
  EEPROM_readAnything(45,pHMin);
  EEPROM_readAnything(55,tdsMin);
  Serial.println(F("============= READ EEPROM ============="));
  Serial.println("phcal : " + String(pHCal));
  Serial.println("tdscal : " + String(tdsCal));
  Serial.println("phmax : " + String(pHMax));
  Serial.println("phmin : " + String(pHMin));
  Serial.println("tdsmin : " + String(tdsMin));
  Serial.println(F("============= READ EEPROM ============="));
}
void displayDebug() {
  Serial.println(F("============= DEBUG ============="));
  Serial.println("Temperature: " + String(temp));
  Serial.println("pH: " + String(phValue));
  Serial.println("Tds: " + String(tdsValue));
  Serial.println("Ldr: " + String(ldrValue));
  Serial.println("Ldr Status: " + String(ldrStatus));
  Serial.println("Tinggi Air: " + String(Wlevel));
}

void displayLcd(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("PH :");
  lcd.print(phValue);
  lcd.print(" TDS :");
  lcd.print(tdsValue);
  lcd.setCursor(0,1);
  lcd.print("SH :");
  lcd.print(temp);
  lcd.print((char)223);
  lcd.print("TS :");
  lcd.print(tdsMin); 
  lcd.setCursor(0,2);
  lcd.print("LDR:");
  lcd.print(ldrStatus);
  lcd.setCursor(0,3);
  lcd.print("PH SET :D");
  lcd.print(pHMin);
  lcd.print(" U");
  lcd.print(pHMax);
}

void getDataSerial(){
 
while(Serial.available()>0)
{
  vD[j] = Serial.read();
  j++;
}

String check = "";
for(char *p = strtok(vD, "|"); p != NULL; p = strtok(NULL, "|")){
  check = p;
  check.trim();
  if(check == "send"){sendDataSerial();
  }else{
    int l = 0;
    String tmp[2];
    char *token, *str, *tofree;
    tofree = str = strdup(p);
    while((token = strsep(&str,":"))){
      tmp[l] = token;
      l++;
    }
    
       if(tmp[0] == "phmax"){
        pHMax = tmp[1].toFloat();
        EEPROM_writeAnything(35,pHMax);
      }
      if(tmp[0] == "phmin"){
        pHMin = tmp[1].toFloat();
        EEPROM_writeAnything(45,pHMin);
      }
      if(tmp[0] == "tdsmin"){
        tdsMin = tmp[1].toFloat();
        EEPROM_writeAnything(55,tdsMin);
      }
      if(tmp[0] == "phcal"){
        pHCal = tmp[1].toFloat();
        EEPROM_writeAnything(15,pHCal);
      }
      if(tmp[0] == "tdscal"){
        tdsCal = tmp[1].toFloat();
        EEPROM_writeAnything(25,tdsCal);
      }
      
    if(tmp[0] == "pu") pHMax       = tmp[1].toFloat();
    if(tmp[0] == "pd") pHMin       = tmp[1].toFloat();
    if(tmp[0] == "kp") pHCal       = tmp[1].toFloat();
    if(tmp[0] == "kt") tdsCal      = tmp[1].toFloat();
    if(tmp[0] == "st") tdsMin      = tmp[1].toFloat();
    if(tmp[0] == "rt") relayTds    = tmp[1].toInt();
    if(tmp[0] == "rl") relayLamp   = tmp[1].toInt();

    free(tofree);
    }
  }
    
// reset
memset(vD, 0, sizeof(vD));
j = 0;       
}

void sendDataSerial(){
  String  DataSend = String(temp)+ "#" +
    String(tdsValue)+ "#" +
    String(ldrValue)+ "#" +
    String(Wlevel)+ "#" +
    String(phValue);
  Serial.println(DataSend); 
}

void setLdr(){
 ldrValue = analogRead(ldrAnalog);
 if(ldrValue >= 200) ldrStatus = "DARK";
 else ldrStatus = "BRIGHT";
}

void setTemp() {
  sensors.requestTemperatures(); // Send the command to get temperatures
  temp = sensors.getTempCByIndex(0); // Set temperature in Celsius
}

void setWheight() {
  int Wlevel = readSensor();
}

int readSensor() {
  digitalWrite(sensorPower, HIGH);
  delay(10);
  val = analogRead(sensorPin);
  digitalWrite(sensorPower, LOW);
  return val;
}

void setpH() {
  static float voltage;

  pHArray[pHArrayIndex++] = analogRead(pHAnalog);
  if(pHArrayIndex == pHArrayLength) pHArrayIndex = 0;
  voltage = avgpHValue(pHArray, pHArrayLength) * 5.0 / 1024;
  phValue = 3.5 * voltage + pHCal;
}

void setTds() {
  static unsigned long analogSampleTimepoint = millis();
  // every 40 milliseconds,read the analog value from the ADC
  if(millis() - analogSampleTimepoint > 40U){
     analogSampleTimepoint = millis();
     analogBuffer[analogBufferIndex] = analogRead(tdsAnalog); // read the analog value and store into the buffer
     analogBufferIndex++;
     if(analogBufferIndex == SCOUNT) 
      analogBufferIndex = 0;
   }   

   static unsigned long printTimepoint = millis();
   if(millis() - printTimepoint > 800U){
      printTimepoint = millis();
      for(copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
        analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 1024.0;
      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationCoefficient=1.0 + 0.02 * (temp - 25.0);
      //temperature compensation
      float compensationVolatge = averageVoltage / compensationCoefficient;
      //convert voltage value to tds value
      tdsValue = (
        133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 *
        compensationVolatge * compensationVolatge + 857.39 * compensationVolatge
      ) * 0.5;

      tdsValue = (tdsValue == 0.0) ? tdsValue : tdsValue + tdsCal;
    }
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  int i, j, bTemp;

  for(byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];

  for(j = 0; j < iFilterLen - 1; j++) {
    for(i = 0; i < iFilterLen - j - 1; i++) {
      if(bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }

  if((iFilterLen & 1) > 0) bTemp = bTab[(iFilterLen - 1) / 2];
  else bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;

  return bTemp;
}

double avgpHValue(int* arr, int number){
  double avg;
  long amount = 0;
  int i, max, min;

  if(number <= 0){
    Serial.println(F("Error number for the array to avraging!/n"));
    return 0;
  }

  // less than 5, calculated directly statistics
  if(number < 5){
    for(i = 0;i < number; i++){
      amount += arr[i];
    }
    avg = amount / number;
    return avg;
  }else{
    if(arr[0] < arr[1]){
      min = arr[0];
      max = arr[1];
    }
    else{
      min = arr[1];
      max = arr[0];
    }

    for(i = 2; i < number; i++){
      if(arr[i] < min){
        amount += min;
        min = arr[i];
      }else {
        if(arr[i] > max){
          amount += max;
          max = arr[i];
        }else{
          amount+=arr[i];
        }
      }
    }
    avg = (double)amount / (number - 2);
  }
  return avg;
}

void relayController(){
  // ldr >= 200 DARK and turn on lamp
  if(ldrValue >= 200 || relayLamp == "on"){
    digitalWrite(lampRelay,LOW);
  }
  else {
    digitalWrite(lampRelay,HIGH); 
  }

  if(phValue < pHMin){
      digitalWrite(phupRelay,LOW);
      delay(250);
      digitalWrite(phupRelay,HIGH);
  } else 
      
  if(phValue > pHMax){
      digitalWrite(phdownRelay,LOW);
      delay(250);
      digitalWrite(phdownRelay,HIGH);
  } else 
      
  if((tdsValue < tdsMin && tdsValue != 0.0) || relayTds == "on"){
      digitalWrite(tdsRelay,LOW);
      delay(250);
      digitalWrite(tdsRelay,HIGH);
  } else;
}

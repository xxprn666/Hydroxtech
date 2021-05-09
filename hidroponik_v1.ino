#include <string.h>
#include <stdio.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4);

/* SENSOR PH PIN */
#define pHAnalog A0     // pH meter Analog output to Arduino Analog Input 0
#define pHArrayLength 5 // times of collection
/* SENSOR TDS SETTING */
#define TdsSensorPin A1
#define VREF 5.0      // analog reference voltage(Volt) of the ADC
#define SCOUNT  5     // sum of sample point
/* SENSOR Water level PIN */
#define SIGNAL_PIN A2
/* SENSOR TDS SETTING */
#define ONE_WIRE_BUS 2 // Data wire is plugged into digital pin 2 on the Arduino
OneWire oneWire(ONE_WIRE_BUS);  
DallasTemperature sensors(&oneWire);

/* VAR DATA FROM NODEMCU */
char vD[45]; 
int j =0;
char mystr[20]; 
/* PIN RELAY  */
const int pinRelaytds    = 4;
const int pinRelayphup   = 5;
const int pinRelayphdown = 7;
/* LDR PIN  */
const int ldrpin = A3;
/* SENSOR VAR */
float phValue, tdsValue, Wtemp;
int ldrValue, Wlevel;
String ldrStatus;
/* LCD VALUE */
int lcdOn    = 1; 
int lcdOff   = 0;
/* KALIBRASI VAR */
float pHsetup, pHsetdown;
int tdsSet, kalPh, kalTds;
/* KALIBRASI VALUE */
float phUp   = 7;
float phDown = 5;
float *pUp   = &phUp ;
float *pDown = &phDown;
/* KALIBRASI PH SENSOR */
int phKali    = 0;
int *pKalPh  = &phKali;
/* KALIBRASI TDS SENSOR */
int tdsKali    = 0;
int *pKalTds  = &tdsKali;
/* SENSOR TDS SETTING */
int analogBuffer[SCOUNT];    // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0,copyIndex = 0;
float averageVoltage = 0;
/* SENSOR TDS SETTING */
int pHArray[pHArrayLength];
int pHArrayIndex = 0;

void setup(void)
{
Serial.begin(9600);
sensors.begin();  
lcd.init();
// LCD BEGIN
lcd.backlight();
// WATERLEVEL BEGIN
digitalWrite(SIGNAL_PIN, LOW);
// TDS SENSOR BEGIN
pinMode(TdsSensorPin,INPUT);

// RELAY BEGIN
pinMode (pinRelayphup, OUTPUT);   //pin relay ph up 
digitalWrite(pinRelayphup,HIGH); //HIGH = OFF

pinMode (pinRelayphdown, OUTPUT);   //pin relay ph down
digitalWrite(pinRelayphdown,HIGH); //HIGH = Mati OFF
   
pinMode (pinRelaytds, OUTPUT);   //pin relay nutrisi
digitalWrite(pinRelaytds,HIGH); //HIGH = Mati OFF
}

void loop(void){
tempSensor();
waterlvSensor();
ldrSensor();  
tdsSensor();
phSensor();
sendNodemcu();
lcdDisplay();
relayPump();

delay(1000);
}

void sendNodemcu(){
// REQ NodeMCU
while(Serial.available()>0)
{
  vD[j] = Serial.read();
  j++;
}

String check = "";
for(char *p = strtok(vD, "|"); p != NULL; p = strtok(NULL, "|")){
  check = p;
  check.trim();
  if(check == "send"){kirimdata();
  }else{
    int l = 0;
    String tmp[2];
    char *token, *str, *tofree;
    tofree = str = strdup(p);
    while((token = strsep(&str,":"))){
      tmp[l] = token;
      l++;
    }
    if(tmp[0] == "pu") pHsetup     = tmp[1].toFloat();
    if(tmp[0] == "pd") pHsetdown   = tmp[1].toFloat();
    if(tmp[0] == "kp") kalPh       = tmp[1].toInt();
    if(tmp[0] == "kt") kalTds      = tmp[1].toInt();
    if(tmp[0] == "st") tdsSet      = tmp[1].toInt();
    if(tmp[0] == "pu") {dataUpdate();}
    free(tofree);
    }
  }
    // RESET
    memset(vD, 0, sizeof(vD));
    j = 0;       
}

void dataUpdate(){
// UPDATE VALUE VAR
  *pUp     = pHsetup;
  *pDown   = pHsetdown;
  *pKalPh  = kalPh;
  *pKalTds = kalTds;
}

void kirimdata(void){
// SEND DATA TO NODEMCU DATASERIAL
  String  datakirim = String(Wtemp)+ "#" +
    String(tdsValue)+ "#" +
    String(ldrValue)+ "#" +
    String(Wlevel)+ "#" +
    String(phValue);
  Serial.println(datakirim); 
}

void tempSensor(){
// SUHU SENSOR CODE
    sensors.requestTemperatures();
    Wtemp = sensors.getTempCByIndex(0);
}

void waterlvSensor(){
// WATER LEVEL    
    Wlevel = analogRead(SIGNAL_PIN);
}

void ldrSensor(){
// LDR CAHAYA 
    ldrValue = analogRead(ldrpin);
    if(ldrValue >= 200) ldrStatus = "DARK";
    else ldrStatus = "BRIGHT";
}

void tdsSensor(){
  static unsigned long analogSampleTimepoint = millis();
   if(millis()-analogSampleTimepoint > 40U)     //every 40 milliseconds,read the analog value from the ADC
   {
     analogSampleTimepoint = millis();
     analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
     analogBufferIndex++;
     if(analogBufferIndex == SCOUNT) 
         analogBufferIndex = 0;
   }   
   static unsigned long printTimepoint = millis();
   if(millis()-printTimepoint > 800U)
   {
      printTimepoint = millis();
      for(copyIndex=0;copyIndex<SCOUNT;copyIndex++)
        analogBufferTemp[copyIndex]= analogBuffer[copyIndex];
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 1024.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      float compensationCoefficient=1.0+0.02*(Wtemp-25.0);    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationVolatge=averageVoltage/compensationCoefficient;  //temperature compensation
      tdsValue=(
        100.42*compensationVolatge*compensationVolatge*compensationVolatge -
        255.86*compensationVolatge*compensationVolatge + 857.39*compensationVolatge
      )*0.5; //convert voltage value to tds value

      tdsValue = (tdsValue == 0.0) ? tdsValue : tdsValue + tdsKali;
   }
}

// TDS SENSOR VOLTAGE
int getMedianNum(int bArray[], int iFilterLen) 
{
      int bTab[iFilterLen];
      for (byte i = 0; i<iFilterLen; i++)
      bTab[i] = bArray[i];
      int i, j, bTemp;
      for (j = 0; j < iFilterLen - 1; j++) 
      {
      for (i = 0; i < iFilterLen - j - 1; i++) 
          {
        if (bTab[i] > bTab[i + 1]) 
            {
        bTemp = bTab[i];
            bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
         }
      }
      }
      if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
      else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
      return bTemp;
}

void phSensor() {
  static float voltage;
  pHArray[pHArrayIndex++] = analogRead(pHAnalog);
  if(pHArrayIndex == pHArrayLength) pHArrayIndex = 0;
  voltage = avgpHValue(pHArray, pHArrayLength) * 5.0 / 1024;
  phValue = 3.5 * voltage + phKali;
}

// PH SENSOR VOLTAGE
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

void lcdDisplay(){
// MENAMPILKAN HASIL SENSOR PADA LCD
lcd.clear();
lcd.setCursor(0,0);
lcd.print("PH:");
lcd.print(phValue);
lcd.print(" TDS:");
lcd.print(tdsValue);
lcd.setCursor(0,1);
lcd.print("SH:");
lcd.print(Wtemp);
lcd.print((char)223);
lcd.print("L:");
lcd.print(ldrValue);
lcd.print("W:");
lcd.print(Wlevel);
lcd.setCursor(0,2);
lcd.print("PH SET: D");
lcd.print(phDown);
lcd.print(" U");
lcd.print(phUp);
lcd.setCursor(0,3);
lcd.print("TS:");
lcd.print(tdsSet); 
}

void relayPump(){
  // PH UP PUMP IS ON
  lcd.print(" PM:PU"); 
  if (phValue < phDown){
      lcd.print(lcdOn);
      digitalWrite(pinRelayphup, LOW);
      delay(500);  
      digitalWrite(pinRelayphup, HIGH);      
  }else lcd.print(lcdOff); 
  
  // PH DOWN PUMP IS ON
  lcd.print(" PD");
  if (phValue > phUp){
      lcd.print(lcdOn);
      digitalWrite(pinRelayphdown, LOW);
      delay(500);  
      digitalWrite(pinRelayphdown, HIGH);      
  }else lcd.print(lcdOff); 
  
  // TDS PUMP IS ON 
  lcd.print(" N");
  if (tdsValue < tdsSet){
      lcd.print(lcdOn);
      digitalWrite(pinRelaytds, LOW);
      delay(500);  
      digitalWrite(pinRelaytds, HIGH);      
  }else lcd.print(lcdOff);        
}    

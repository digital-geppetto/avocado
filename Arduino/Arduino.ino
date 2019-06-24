//
// The Firmware for the Basic Arduino, which gets downloaded and installed when creatinga teleonome using www.digitalgeppetto.com/CreateTeleonome.sh
//
// This firmware controls 3 rgb leds and 2 buttons and an RTC
// led 1 is the identity led and changes according to wheter the teleonome is in host or organism
// led 2 is the pulse led, it changes to red during the pulse
// led 3 is the weather led, it goes from deep blue, light blue , yellow, green, red and orange
// the ranges for this colors are given by the variables below.  this led blinks if the forecast includes rain

// Button 1 is the restart button.  When the user presses this button, the hypthalamus receives the reboot command, which is   
//  and executes it at the next async cycle

// Button 2 is the shutdown button.  When the user presses this button, the hypthalamus receives the reboot command, which is   
//  and executes it at the next async cycle

// the RTC is primarly used to generate TOTP
//
//Build 1 9/7/18 6:26
//
//
// Libraries
//
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "totp.h"
#include <swRTC.h>

#define USE_INTERNAL_RTC

//
//End Of Libraries
//

String faultData="";
swRTC  rtc;     //RTC Initialization
int timeZoneHours=11;
int SECONDOFFSET=10;
#define LEAP_YEAR(_year) ((_year%4)==0)
static  byte monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};
char code[7];
int SHARED_SECRET_LENGTH=27;
int SHARED_SECRET_START_POSITION=0;
int DIGITAL_GEPPETTO_SECRET_LENGTH=27;
int DIGITAL_GEPPETTO_START_POSITION=30;

int COMMAND_CODE_HISTORY_REFRESH_SECONDS=15;
long commandCodeHistoryLastRefreshSeconds=0;

int currentCommandCodeHistoryPos=0;
int numberOfCommandCodesInHistory=5;
long commandCodeHistory[5]={999999,999999,999999,99999,99999};


//
// User Input Variables
//
// These variables get updated by the user
//
float purpleMaximumTemp=3.0;
float blueMaximumTemp=10.0;
float yellowMaximumTemp=18.0;
float greenMaximumTemp=30.0;
float redMaximumTemp=38.0;


//
// The RGB Leds
#define LED_PIN        7
#define LED_COUNT      3
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
int blinkCounter=0;
boolean blinkOn=false;
boolean blinkStatus=false;
int weatherGreen, weatherRed, weatherBlue;
int blinkMillis=4000;
//
// The push buttons
//
// two sets of variables one each for the reboot button 
// and one for the shutdown button
//
int rebootPin = 5;   
int shutdownPin = 6;
long debounceDelay = 50;  

int rebootButtonValue=0;
int rebootButtonState;             // the current reading from the input pin
int rebootLastButtonState = LOW;   // the previous reading from the input pin
long rebootLastDebounceTime = 0;  // the last time the output pin was toggled

int shutdownButtonValue=0;
int shutdownButtonState;             // the current reading from the input pin
int shutdownLastButtonState = LOW;   // the previous reading from the input pin
long shutdownLastDebounceTime = 0;  // the last time the output pin was toggled





String pulseStartTime="";
 String pulseStopTime="";
boolean inPulse=false;

//
// the virtual micrcntroller
boolean isHost=true;
String currentIpAddress="No IP";
String currentSSID="No SSID";



//
// devices specific functions
//
void saveSecret(String secret, int numberDigits, int periodSeconds ){
  char someCharArray[SHARED_SECRET_LENGTH];
  secret.toCharArray(someCharArray,SHARED_SECRET_LENGTH);
  
   for(int i=SHARED_SECRET_START_POSITION;i<SHARED_SECRET_LENGTH ;i++){
    EEPROM.write(i, someCharArray[i]);
   }

   EEPROM.write(SHARED_SECRET_LENGTH, numberDigits);
   EEPROM.write(SHARED_SECRET_LENGTH+1, periodSeconds);
}

void readSecret(char *secretCode){
  
  if ( EEPROM.read ( SHARED_SECRET_START_POSITION ) != 0xff ){
    for (int i = SHARED_SECRET_START_POSITION; i < SHARED_SECRET_LENGTH; ++i ){
        secretCode [ i ] = EEPROM.read ( i );
    }
  }
}


void saveDigitalGeppettoSecret(String secret, int numberDigits, int periodSeconds ){
  char someCharArray[DIGITAL_GEPPETTO_SECRET_LENGTH];
  secret.toCharArray(someCharArray,DIGITAL_GEPPETTO_SECRET_LENGTH);
  
   for(int i=DIGITAL_GEPPETTO_START_POSITION;i<DIGITAL_GEPPETTO_SECRET_LENGTH ;i++){
    EEPROM.write(i, someCharArray[i]);
   }
  int pos = DIGITAL_GEPPETTO_START_POSITION + DIGITAL_GEPPETTO_SECRET_LENGTH;
   EEPROM.write(pos, numberDigits);
   EEPROM.write(pos+1, periodSeconds);
}


void readDigitalGeppettoSecret(char *digitalGeppettoSecretCode){
  
  if ( EEPROM.read ( DIGITAL_GEPPETTO_START_POSITION ) != 0xff ){
    for (int i = DIGITAL_GEPPETTO_START_POSITION; i < SHARED_SECRET_LENGTH; ++i ){
        digitalGeppettoSecretCode [ i ] = EEPROM.read ( i );
    }
  }
}

long dateAsSeconds(int year, int month, int date, int hour, int minute, int second){

 
// note year argument is full four digit year (or digits since 2000), i.e.1975, (year 8 is 2008
  
   int i;
   long seconds;

   if(year < 69) 
      year+= 2000;
    // seconds from 1970 till 1 jan 00:00:00 this year
    seconds= (year-1970)*(60*60*24L*365);
  
    // add extra days for leap years
    for (i=1970; i<year; i++) {
        if (LEAP_YEAR(i)) {
            seconds+= 60*60*24L;
        }
    }
    // add days for this year
    for (i=0; i<month; i++) {
      if (i==1 && LEAP_YEAR(year)) { 
        seconds+= 60*60*24L*29;
      } else {
        seconds+= 60*60*24L*monthDays[i];
      }
    }
    seconds+= (date-1)*3600*24L;
    seconds+= hour*3600L;
    seconds+= minute*60L;
    seconds+= second;
    seconds -=  timeZoneHours*3600L;
    return seconds; 
}



long generateCode(){

   int seconds = rtc.getSeconds()+SECONDOFFSET;  
  int month = rtc.getMonth()-1;
  long timestamp = dateAsSeconds(rtc.getYear(), month, rtc.getDay(), rtc.getHours(), rtc.getMinutes(), seconds);
  
  char secretCode[SHARED_SECRET_LENGTH];
  readSecret(secretCode);
  TOTP totp = TOTP(secretCode);
 // TOTP totp = TOTP("JV4UYZLHN5CG633S");
 
 long code=totp. gen_code  (timestamp ) ;
 //long code=totp. gen_code  (rtc.year, month, rtc.day, rtc.hour, rtc.minute, rtc.second, timeZoneHours*60);
 //long code=totp. gen_code  (
 // now check to see if this code is already in the history
 boolean found=false;
 for(int i=0;i<numberOfCommandCodesInHistory;i++){
    if(commandCodeHistory[i]==code){
        found=true;
    }     
 }

 if(!found){
   //
   // 
   if(currentCommandCodeHistoryPos<numberOfCommandCodesInHistory){
      commandCodeHistory[currentCommandCodeHistoryPos]=code;
      currentCommandCodeHistoryPos++;
    }else{
      for(int i=0;i<numberOfCommandCodesInHistory-1;i++){
        commandCodeHistory[i]=commandCodeHistory[i+1];
      }
      commandCodeHistory[numberOfCommandCodesInHistory-1]=code;
    }
 }
  
  return code;
}

long generateDigitalGeppettoCode(){
  
  int seconds = rtc.getSeconds()+SECONDOFFSET;  
  int month = rtc.getMonth()-1;
  long timestamp = dateAsSeconds(rtc.getYear(), month, rtc.getDay(), rtc.getHours(), rtc.getMinutes(), seconds);
  char dgSecretCode[DIGITAL_GEPPETTO_SECRET_LENGTH];
  readDigitalGeppettoSecret(dgSecretCode);
  TOTP totp = TOTP(dgSecretCode);
 long code=totp. gen_code  (timestamp ) ; 
  return code;
}

boolean checkCode(long userCode){
  boolean codeOk=false;
  long code = generateCode();
 // Serial.print("code=");
  //Serial.print(code);
  
  if(userCode == code){
    codeOk=true;
  }else{
    for(int i=0;i<numberOfCommandCodesInHistory;i++){
   //    Serial.print("codehistory");
   //   Serial.print(commandCodeHistory[i]);
      
    if(commandCodeHistory[i] == userCode){
      codeOk=true;
    }
  }
  }
  
  return codeOk;
}
void testRTCMode(){
  
//*************************Time********************************
  Serial.print("   Year = ");//year
  Serial.print(rtc.getYear());
  Serial.print("   Month = ");//month
  Serial.print(rtc.getMonth());
  Serial.print("   Day = ");//day
  Serial.print(rtc.getDay());
  Serial.print("   Hour = ");//hour
  Serial.print(rtc.getHours());
  Serial.print("   Minute = ");//minute
  Serial.print(rtc.getMinutes());
  Serial.print("   Second = ");//second
  int seconds = rtc.getSeconds()+SECONDOFFSET;
  Serial.print(seconds);
  

//
int month = rtc.getMonth()-1;

  long timestamp = dateAsSeconds(rtc.getYear(), month, rtc.getDay(), rtc.getHours(), rtc.getMinutes(), seconds);
  Serial.print("date as seconds=");
  Serial.print(timestamp);
  long code = generateCode();
  Serial.print(" timestamp=");
    Serial.print(timestamp);
    Serial.print(" Code=");
    Serial.print(code);
  
    for(int i=0;i<numberOfCommandCodesInHistory;i++){
       Serial.print("codehistory=");
      Serial.print(commandCodeHistory[i]);
     
    }
    Serial.println(" ");
    Serial.flush();
}


String getElapsedTimeHoursMinutesSecondsString(long elapsedTime) {       
      //String seconds = String(elapsedTime % 60);  
      long seconds = elapsedTime/1000;
      int minutes = (seconds % 3600) / 60;
      String minP ="";
      if(minutes<10)minP="0";
      
     
      int hours = seconds / 3600;
      String hoursS = "";
      if(hours<10)hoursS="0";
      
     
      String time =  hoursS + hours + ":" + minP + minutes;// + ":" + seconds;  
      return time;  
  } 
  
String getValue(String data, char separator, int index)
{
 int found = 0;
  int strIndex[] = {
0, -1  };
  int maxIndex = data.length()-1;
  for(int i=0; i<=maxIndex && found<=index; i++){
  if(data.charAt(i)==separator || i==maxIndex){
  found++;
  strIndex[0] = strIndex[1]+1;
  strIndex[1] = (i == maxIndex) ? i+1 : i;
  }
 }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void clearLEDs()
{
  for (int i=0; i<LED_COUNT; i++)
  {
    leds.setPixelColor(i, 0);
  }
}

void displayCommandConfirmation(){

   for(int i=0;i<4;i++){
   leds.setPixelColor(1, 0xFF, 0x00, 0x00);
   leds.show();
   delay(1000);
   leds.setPixelColor(1, 0x00, 0xFF, 0x00);
   leds.show();
   delay(1000);
   leds.setPixelColor(1, 0x00, 0x00, 0xFF);
   leds.show();
   delay(1000);
   leds.setPixelColor(0, 0x00, 0x00, 0x00);
   leds.setPixelColor(1, 0x00, 0x00, 0x00);
   leds.setPixelColor(2, 0x00, 0x00, 0x00);
   leds.show();
   }
    
}
float stringToFloat(String s){
    char buffer[10];
    s.toCharArray(buffer, 10);
    float f = atof(buffer);
  return f;
}

void setup() {
   Serial.begin(9600);
  pinMode(rebootPin, INPUT);
  pinMode(shutdownPin, INPUT);
    leds.begin();  // Call this to start up the LED strip.
  clearLEDs();   // This function, defined below, turns all LEDs off...
  leds.show();
  
}

void loop() {


 int seconds = rtc.getSeconds()+SECONDOFFSET;
 int month = rtc.getMonth()-1;
  long now = dateAsSeconds(rtc.getYear(), month, rtc.getDay(), rtc.getHours(), rtc.getMinutes(), seconds);
  if( (now - commandCodeHistoryLastRefreshSeconds)>COMMAND_CODE_HISTORY_REFRESH_SECONDS){
      commandCodeHistoryLastRefreshSeconds=now;
      long currentCode = generateCode();
  }
  
    String toReturn="";
//    toReturn.concat(batteryVoltageStr) ;
//    toReturn.concat("#") ;
//
//    toReturn.concat(currentValueStr) ;
//    toReturn.concat("#") ;
//    toReturn.concat(lockVoltageValueStr) ;

  if(blinkStatus){
          // if(trueblinkOn){
            //leds.setPixelColor(2,weatherGreen, weatherRed, weatherBlue);
            //leds.show();
            int gradGreen=weatherGreen;
            int gradRed = weatherRed;
            int gradBlue=weatherBlue;
              while(gradGreen<255 || gradRed<255 || gradBlue<255){
                if(gradGreen<255)gradGreen++;
                if(gradRed<255)gradRed++;
                if(gradBlue<255)gradBlue++;
                leds.setPixelColor(2,gradGreen, gradRed, gradBlue);
                leds.show();
                delay(10);
              }
              while(gradGreen>weatherGreen || gradRed>weatherRed || gradBlue>weatherBlue){
                if(gradGreen>weatherGreen)gradGreen--;
                if(gradRed>weatherRed)gradRed--;
                if(gradBlue>weatherBlue)gradBlue--;
                leds.setPixelColor(2,gradGreen, gradRed, gradBlue);
                leds.show();
                delay(10);
              }
              
              leds.setPixelColor(2,weatherGreen, weatherRed, weatherBlue);
              leds.show();
              delay(3500);
         // }else{
         //   leds.setPixelColor(2,weatherGreen, weatherRed, weatherBlue);
         //   leds.show();
        //  }
  }else{
      //
      // is not blinking 
       leds.setPixelColor(2,weatherGreen, weatherRed, weatherBlue);
       leds.show();
 }  
      
      if(blinkCounter>=blinkMillis){
        blinkCounter=0;
        blinkOn=!blinkOn;
      }else{
        blinkCounter++;
      }
      
    //
  // the commands
  //
  if( Serial.available() != 0) {
 
  
 // lcd.clear();
  String command = Serial.readString();
 // lcd.setCursor(0, 0);
//  lcd.print(command);
  

  if(command.startsWith("PulseStart")){
    inPulse=true;
     pulseStartTime = getValue(command, '#', 1);
    Serial.println("Ok-PulseStart");
    Serial.flush();
    leds.setPixelColor(1, 0x00, 0xFF, 0x00);
   leds.show();
 //   lcd.clear();
  }else if(command.startsWith("RebootHypothalamus")){
     pulseStopTime = getValue(command, '#', 1);
    inPulse=false;
    Serial.println("RebootHypothalamusOK");
    Serial.flush(); 
    for(int i=0;i<4;i++){
      leds.setPixelColor(0, 0x00, 0x00, 0xFF);
      leds.setPixelColor(1, 0x00, 0x00, 0xFF);
      leds.setPixelColor(2, 0x00, 0x00, 0xFF);
      leds.show();
      delay(250);
      leds.setPixelColor(0, 0x00, 0x00, 0x00);
      leds.setPixelColor(1, 0x00, 0x00, 0x00);
      leds.setPixelColor(2, 0x00, 0x00, 0x00);
      leds.show();
      delay(250);
    }
     
    
  //  lcd.clear();
  }else if(command.startsWith("ShutdownHypothalamus")){
     pulseStopTime = getValue(command, '#', 1);
    inPulse=false;
    Serial.println("ShutdownHypothalamusOK");
    Serial.flush(); 
    for(int i=0;i<4;i++){
      leds.setPixelColor(0, 0xFF, 0x00, 0x00);
      leds.setPixelColor(1, 0xFF, 0x00, 0x00);
      leds.setPixelColor(2, 0xFF, 0x00, 0x00);
      leds.show();
      delay(250);
      leds.setPixelColor(0, 0x00, 0x00, 0x00);
      leds.setPixelColor(1, 0x00, 0x00, 0x00);
      leds.setPixelColor(2, 0x00, 0x00, 0x00);
      leds.show();
      delay(250);
    }
     
    
  //  lcd.clear();
  }else if(command.startsWith("PulseFinished")){
     pulseStopTime = getValue(command, '#', 1);
    inPulse=false;
     leds.setPixelColor(1, 0x00, 0x00, 0x00);
    Serial.println("Ok-PulseFinished");
     leds.show();
    Serial.flush(); 
  //  lcd.clear();
  }else if(command.startsWith("GetTime"){
    String displayTime =  "";
  Serial.print(rtc.getDay());
  Serial.print("/");
  Serial.print(rtc.getMonth());
  Serial.print("/");
  int year = rtc.getYear()-2000;
  Serial.print(year);
  Serial.print(" ");
  Serial.print(rtc.getHours());
  Serial.print(":");
  Serial.print(rtc.getMinutes());
  Serial.print(":");
  Serial.println(rtc.getSeconds());
  Serial.println("Ok-Get Time");
   Serial.flush();
  }else if(command.startsWith("SetTime")){
    //SetTime#2019#6#24#13#50#45
    int year = getValue(command, '#', 1).toInt();
    int month = getValue(command, '#', 2).toInt(); // January=1
    int day = getValue(command, '#', 3).toInt();
    int hour = getValue(command, '#', 4).toInt();
    int minute = getValue(command, '#', 5).toInt();
    int second = getValue(command, '#', 6).toInt();
    
    rtc.stopRTC(); //stop the RTC
    rtc.setTime(hour,minute,second); //set the time here
    rtc.setDate(day,month,year); //set the date here
    rtc.startRTC(); //start the RTC
    
    Serial.println("Ok-Set Time");
    Serial.flush();
    
  }else if(command.startsWith("testrtc")){
    testRTCMode();
  }else if(command.startsWith("GetSecret")){
     char secretCode[SHARED_SECRET_LENGTH];
    readSecret(secretCode);
    Serial.print("Secret=");
    Serial.println(secretCode);
    Serial.flush();
    
  }else if(command.startsWith("VerifyUserCode")){
    String codeInString = getValue(command, '#', 1);
     long userCode = codeInString.toInt();
     boolean validCode = checkCode( userCode);
     String result="Invalid Code";
     if(validCode)result="Valid Code";
     Serial.println(result);
    Serial.flush();
  }else if(command.startsWith("GetCommandCode")){
  
    long code =generateCode();
    //
    // patch a bug in the totp library
    // if the first digit is a zero, it
    // returns a 5 digit number
    if(code<100000){
      Serial.print("0");
      Serial.println(code);
    }else{
      Serial.println(code);
    }
    
    Serial.flush();
//  }else if(command.startsWith("GetSecret")){
//    char secretCode[SHARED_SECRET_LENGTH];
//    readSecret(secretCode);
//     Serial.println(secretCode);
//    Serial.flush();
//   
//   isHost=true;
  }else if(command.startsWith("GetDigitalGeppettoCommandCode")){
  
    long code =generateDigitalGeppettoCode();
    //
    // patch a bug in the totp library
    // if the first digit is a zero, it
    // returns a 5 digit number
    if(code<100000){
      Serial.print("0");
      Serial.println(code);
    }else{
      Serial.println(code);
    }
    
    Serial.flush();
// 
  } else if(command.startsWith("SetSecret")){    

      String secret = getValue(command, '#', 1);
      int numberDigits = getValue(command, '#', 2).toInt();
      int periodSeconds = getValue(command, '#', 3).toInt();
     saveSecret(secret, numberDigits, periodSeconds);

      Serial.println("Ok-SetSecret");
      Serial.flush();
    
  }else if(command.startsWith("UpdateParameters")){
   //
   // format is 
   //  UpdateParameters#purpleMaximumTemp#blueMaximumTemp#yellowMaximumTemp#greenwMaximumTemp#redMaximumTemp#blinkMillis
  //
      purpleMaximumTemp=  getValue(command, '#', 1).toInt();
      blueMaximumTemp = getValue(command, '#', 2).toInt();
      yellowMaximumTemp = getValue(command, '#', 3).toInt();
      greenMaximumTemp = getValue(command, '#', 4).toInt();
      redMaximumTemp = getValue(command, '#', 5).toInt();
      blinkMillis = getValue(command, '#', 6).toInt();
   Serial.println("Ok-UpdateParams");
      Serial.flush();
  }else if(command.startsWith("SetWeather")){
    //
    //dark blue SetWeather#0#0#255#1#2000
    // light blue SetWeather#82#202#250#1#2000
   // purple  SetWeather#89#59#255#1#2000
   // orange  SetWeather#248#74#31#1#2000
   // red  SetWeather#255#0#0#1#2000
    float forecastTemperature = stringToFloat(getValue(command, '#', 1));//.toInt(); 
    float forecastRainMillis = stringToFloat(getValue(command, '#', 2)); 


      if(forecastTemperature<purpleMaximumTemp){
            weatherRed = 0; 
          weatherGreen = 0; 
           weatherBlue = 255; 
      }else if(forecastTemperature>purpleMaximumTemp && forecastTemperature<blueMaximumTemp){
        weatherRed = 82; 
          weatherGreen = 202; 
           weatherBlue = 250; 
      } else if(forecastTemperature>blueMaximumTemp &&forecastTemperature<yellowMaximumTemp){
         weatherRed = 255; 
          weatherGreen = 255; 
           weatherBlue = 0; 
      } else if(forecastTemperature>yellowMaximumTemp && forecastTemperature<greenMaximumTemp){
        weatherRed = 0; 
          weatherGreen = 255; 
           weatherBlue = 0; 
      } else if(forecastTemperature>greenMaximumTemp  &&forecastTemperature<redMaximumTemp){
        weatherRed = 255; 
          weatherGreen = 0; 
           weatherBlue = 0; 
      }  else if(forecastTemperature>redMaximumTemp){
        weatherRed = 248; 
          weatherGreen = 74; 
           weatherBlue = 31; 
      } 
   if(forecastRainMillis>0){
      blinkStatus=true;
   }else{
      blinkStatus=false;
   }
   blinkCounter=0;
  Serial.print("Ok-SetWeather ");
  Serial.println(blinkStatus);
    Serial.flush();

  }else if(command.startsWith("IPAddr")){
//    notInPulse=false;
    currentIpAddress = getValue(command, '#', 1); 
    Serial.println("Ok-IPAddr");
    Serial.flush();
 //   delay(delayTime);
  //  notInPulse=true;

  }else if(command.startsWith("SSID")){
    //notInPulse=false;
    currentSSID = getValue(command, '#', 1); 
    Serial.println("Ok-currentSSID");
    Serial.flush();
  //  delay(delayTime);
  //  notInPulse=true;

  }else if(command.startsWith("HostMode")){
   Serial.println("Ok-HostMode");
    Serial.flush();
  //  delay(delayTime);
  leds.setPixelColor(0, 0xFF, 0x00, 0x00);
  leds.show();
   isHost=true;
  }else if(command.startsWith("NetworkMode")){
   Serial.println("Ok-NetworkMode");
    Serial.flush();
   // delay(delayTime);
   leds.setPixelColor(0, 0x00, 0x00, 0xFF);
   leds.show();
   isHost=false;
  }else if(command.startsWith("GetSensorData")){
    Serial.println(toReturn);
    Serial.flush();
   
  }else if(command=="Ping"){
  
    Serial.println("ok-Ping");
    Serial.flush();
  }else if (command.startsWith("AsyncData") ){
    if(faultData == ""){
      Serial.print("AsyncCycleUpdate#");
      Serial.println(toReturn);
      Serial.flush();
    }
  }else{
    Serial.print("Failure-Bad Command");
    Serial.println(command);
    Serial.flush();
  }
 }else{
  // while waiting for data request check
    // to see if the user clicked on the reboot or
    // the shutdown button
    //
    // first check the reboot button
    if(!inPulse){
     //
     // check and debounce the reboot button
     //
      rebootButtonValue = digitalRead(rebootPin);  // read input value     
      if (rebootButtonValue != rebootLastButtonState) {
        // reset the debouncing timer
        rebootLastDebounceTime = millis();
        //Serial.println("p2");
      }
     
      if ((millis() - rebootLastDebounceTime) > debounceDelay) {
       // Serial.println("p3=");
        //Serial.println((millis() - rebootLastDebounceTime));
          // whatever the reading is at, it's been there for longer
          // than the debounce delay, so take it as the actual current state:
          // if the button state has changed:
          if (rebootButtonValue != rebootButtonState) {
              rebootButtonState = rebootButtonValue;
              if (rebootButtonState != HIGH) {
                Serial.println("$Reboot");
                Serial.flush();
                displayCommandConfirmation();
                            
              }
          }
        }
      rebootLastButtonState = rebootButtonValue;
     //
     // check and debounce the shutdown button
     //
      shutdownButtonValue = digitalRead(shutdownPin);  // read input value
      if (shutdownButtonValue != shutdownLastButtonState) {
        // reset the debouncing timer
        shutdownLastDebounceTime = millis();
      }
     // Serial.println("p2");
      if ((millis() - shutdownLastDebounceTime) > debounceDelay) {
          // whatever the reading is at, it's been there for longer
          // than the debounce delay, so take it as the actual current state:
          // if the button state has changed:
          if (shutdownButtonValue != shutdownButtonState) {
              shutdownButtonState = shutdownButtonValue;
              if (shutdownButtonState != HIGH) {
                Serial.println("$Shutdown");
                Serial.flush();
                displayCommandConfirmation();
              }
          }
        }
        shutdownLastButtonState = shutdownButtonValue;   
 }
}
}

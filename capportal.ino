#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "FS.h"
#include "SPIFFS.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <BH1750FVI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <NTPClient.h>
#include <WiFiUdp.h>



#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1013.25)
#define echoPin 14 // Echo Pin (OUTPUT pin in RB URF02)
#define trigPin 15// Trigger Pin (INPUT pin in RB URF02)
#define  SENSOR      A6                    // Определяем номер аналогового входа, к которому подключен датчик влажности почвы.                                        
#define  MIN        1603                  // Определяем минимальное показание датчика (в воздухе),
#define  MAX        832                    // определяем максимальное показание датчика (в воде),



IPAddress apIP(172, 217, 28, 1);
DNSServer dnsServer;
WebServer webServer(80);
//SimpleSmtpClient client; //создаем экземпляр класса SimpleSmtpClient (клиента SMTP)
BH1750FVI LightSensor(BH1750FVI::k_DevModeContLowRes);
Adafruit_BME280 bme; // I2C
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


String NAME = "Foxy";
String SPECY = "Decabrist";
int NOMINAL_LIGHT = 4000;
String TIME_WATER = "1h5";
String DATEOFBIRTH = "23.12.2019";
int WATER_CAPACITY = 0;
int SOIL_MOISTURE = 0;

bool ntp_flag = false;
int last_check = 0;
const uint8_t GPIOPIN[4] = {16,7,4,13};  
String  etatGpio[4] = {"OFF","OFF","OFF","OFF"};
const byte DNS_PORT = 53;
int maximumRange = 50; // Maximum range needed
int minimumRange = 0; // Minimum range needed
long duration, distance; // Duration used to calculate distance



bool CONFIG = false;
bool WIFI_ON = false;
bool flag = true;
int wifi_rst_button = 5;
unsigned long watering_last_time;

//параметры точки доступа
const char *ssid = "ESP-32(captive)";
const char *password = "88888888";

void setup(){

Serial.begin(115200);
Serial.println("Starting up...");
  Wire.begin();
 LightSensor.begin(); 

 if (! bme.begin(&Wire)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        //while (1);
    }

 pinMode(trigPin, OUTPUT);
 pinMode(echoPin, INPUT);
  Serial.println("Starting up...");

for ( int x = 0 ; x < 5 ; x++ ) { 
    pinMode(GPIOPIN[x],OUTPUT);
  }  

pinMode(wifi_rst_button, INPUT);
 digitalWrite(wifi_rst_button, HIGH);
 

  SPIFFS.begin();

    
  WIFI_ON = true;
  // SPIFFS.remove("/ip_config.txt"); 
  
  File myFile = SPIFFS.open("/ip_config.txt", FILE_READ);

  if (!myFile) {
  Serial.println("File doesn't exist yet. Creating it");
  }
  Serial.print("File content: \"");
  
  while(myFile.available()) {
      Serial.write(myFile.read());
  }
  
  Serial.println("\"");
  Serial.print("File size: ");
  Serial.println(myFile.size());
  
  if  (myFile.size() == 0){
      myFile.close();
      Serial.println("Configuring access point...");
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      WiFi.softAP(ssid, password);
      Serial.println("Good");
  }else{
    WiFi.mode(WIFI_STA);
      CONFIG = true;
  }
  
  dnsServer.start(DNS_PORT, "*", apIP);
  
  
  webServer.onNotFound([]() {
    File file = SPIFFS.open("/cap_portal.html", "r");
    
    Serial.println("otprav cap");
    webServer.streamFile(file,"text/html");
    file.close();
   // webServer.send(200, "text/html", responseHTML);
  });
   
   webServer.on ( "/", handleRoot );
   webServer.on ( "/index", index1 );
   webServer.on ( "/flower.html", flower1 );
   webServer.on ( "/sensors.html", sensor1 );
   webServer.on ( "/schedule.html", schedule1 );
   webServer.on ( "/developers.html", developers1 );
   webServer.on ( "/sensor", data_sensor );
   webServer.on ( "/flower", data_flower );
   webServer.on ( "/flower1", watering_now );
   webServer.on ( "/flower2", scheduling );
   
   
   webServer.begin();

}






//***********Write to ROM**********//
void write_SPIFFS(String x){
  File f = SPIFFS.open("/ip_config.txt", FILE_APPEND);
  x += ";";
  Serial.println(x);
  f.print(x);
  f.close();
  Serial.println("finishing writing...");
  delay(3000);
}

  
//****************************EEPROM Read****************************//
String read_string(int l){
  File f = SPIFFS.open("/ip_config.txt", "r");
  String line;
  int counter = 0;
  Serial.println("starting reading");
  delay(2000);
  while(f.available()) {
      line = f.readStringUntil(';');
      //Serial.println(line);
      if (l == counter){
      // Serial.println("zapisal" + line);
        f.close();
        return line;
      }
      counter++;
    }
  f.close();
  return "";
}
void ROMwrite(String s, String p){
 write_SPIFFS(s);
 write_SPIFFS(p);
 CONFIG = true;  
}

const char HTTP_PAGE_WiFi[] PROGMEM = "<p>{s}<br>{p}</p>";
const char HTTP_PAGE_GOHOME[] PROGMEM = "<H2><a href=\"/\">go home</a></H2><br>";

void reconnectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
        String string_Ssid="";
        String string_Password="";
        string_Ssid= read_string(0); 
        string_Password= read_string(1);        
        Serial.println("ssid: "+ string_Ssid);
        Serial.println("Password: "+string_Password);
               
  delay(400);
  WiFi.begin(string_Ssid.c_str(),string_Password.c_str());
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {   
      delay(500);
      Serial.print(".");
      if(counter == 20){
          String response = "<script>alert(\"Password not connected\")</script";
          webServer.send(200,"text/html",response);
          ESP.restart();
        }
        counter++;
  }
 
  //dnsServer.stop();
 // webServer.on ( "/index", index1 );
  //webServer.stop();
  delay(350);
  
  Serial.print("Connected to:\t");
  Serial.println(WiFi.localIP());
  
   WIFI_ON =false;
  flag = false;

  
   /*if (client.sendMail(smtpServer,port,user,pass,mailfrom,mailto,subject,message)) {// если сеанс закончен
      if (client.getStatus()) Serial.println(F("Message sent!"));
      else Serial.println(F("Message not sent!"));
    }*/
  
}

void handleSubmitForm() {

      String response=FPSTR(HTTP_PAGE_WiFi);
      Serial.println(webServer.arg("ssid"));
      Serial.println(webServer.arg("passkey"));
      response.replace("{s}",webServer.arg("ssid"));
      response.replace("{p}",String(webServer.arg("passkey")));
      response+=FPSTR(HTTP_PAGE_GOHOME);
      webServer.send(200, "text/html", response);
      ROMwrite(String(webServer.arg("ssid")),String(webServer.arg("passkey")));  
      reconnectWiFi();
  }
  
void handleRoot(){ 

if(webServer.hasArg("ssid") && webServer.hasArg("passkey")){

       if(webServer.arg("configure") != ""){

           File fileToWrite = SPIFFS.open("/ip_set.txt", FILE_WRITE);

           if(!fileToWrite){
              Serial.println("Error opening SPIFFS");
              return;
           }  

        }
       handleSubmitForm();
  }
}
void data_sensor(){
  const size_t capacity = JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  doc["light_cur"] = LightSensor.GetLightIntensity();
  doc["temp"] = bme.readTemperature();
  doc["hum"] = bme.readHumidity();
  doc["height"] = getWaterCapacity();

  char output_json[700];
  serializeJson(doc, output_json);   
  Serial.println(output_json); 
  
  webServer.sendHeader("Access-Control-Allow-Origin","*");
  webServer.send(200, "application/json", output_json);
   Serial.println("dai dannyh dlya sensor");
}
void data_flower(){
  const size_t capacity = JSON_OBJECT_SIZE(11);
  DynamicJsonDocument doc(capacity);
  doc["light_nom"] = NOMINAL_LIGHT;
  doc["soil"] = getSoilMoisture();
  doc["light_cur"] = LightSensor.GetLightIntensity();
  doc["temp"] = bme.readTemperature();
  doc["hum"] = bme.readHumidity();
  doc["name"] = NAME;
  doc["specy"] = SPECY;
  doc["height"] = getWaterCapacity();
  TIME_WATER = "121212";
  doc["dateofbirth"] = DATEOFBIRTH;
  doc["time_water"] = TIME_WATER;
  char output_json[700];
  serializeJson(doc, output_json);   
  Serial.println(output_json); 
  //tgSendEx("Flower",output_json);
  webServer.sendHeader("Access-Control-Allow-Origin","*");
  webServer.send(200, "application/json", output_json);
  Serial.println("dai dannyh dlya flower");
}
void index1(){
   // SPIFFS.begin();
    File file = SPIFFS.open("/index.html", "r");
    Serial.println("otprav index");
    webServer.streamFile(file,"text/html");
    file.close();
}
void flower1(){
   // SPIFFS.begin();
    File file = SPIFFS.open("/flower.html", "r");
    webServer.streamFile(file,"text/html");

    file.close();
 
    Serial.println("otprav flower");
}
void sensor1(){
   // SPIFFS.begin();
    File file = SPIFFS.open("/sensors.html", "r");

    webServer.streamFile(file,"text/html");
 
    file.close();

    Serial.println("otprav sensor");
}
void write_date_to_file_change(String Date){
   Serial.println("zashel");
  Date.replace(".", ", ");
  int tmp_min = Date.substring(Date.lastIndexOf(", ")).toInt();
  String date_without_min = Date.substring(0, Date.lastIndexOf(", "));

//  String tmp_test = "2019, 11, 30, 10, 0";// пока не придумал как сделать 10 и 16 строку, не знаю как правильно прибавить час.
  
 /* if(!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
  }*/
  
  File file_read = SPIFFS.open("/schedule.html");
 
  if(!file_read){
      Serial.println("Failed to open file for reading");
      return;
  }

  File file_write = SPIFFS.open("/tmp.txt", FILE_WRITE);
 
  if(!file_write){
      Serial.println("There was an error opening the file for writing");
      return;
  }
  Serial.println("1");
  bool flag_first_str = true;
  
//  Serial.println("File Content:");
  String tmp_str = "";
  while(file_read.available()){
    
    tmp_str = file_read.read();
     Serial.println(tmp_str);
   // if (tmp_str.startsWith("startDate: new Date(")){
    /*  if (flag_first_str){
//        file_write.print("startDate: new Date(" + startDate1 +"),");
//        file_read.read();
//        file_write.print("endDate: new Date(" + tmp_test +")");
        flag_first_str = false; Serial.println("2");
      }
      else{
         Serial.println("3");
         file_write.print("startDate: new Date(" + Date +"),");
         file_read.read();
        file_write.print("endDate: new Date(" + date_without_min + String(tmp_min) +")");
      }
    }
    else{
      file_write.print(tmp_str);
    }*/
     file_write.print(tmp_str);
  }
  
   Serial.println("4");
  file_write.close();
  file_read.close();

  SPIFFS.remove("/schedule.html");
  SPIFFS.rename("/tmp.txt", "/schedule.html");
   Serial.println("5");
}

void schedule1(){
    //SPIFFS.begin();

    
    File file = SPIFFS.open("/schedule.html", "r");

    
    
    webServer.streamFile(file,"text/html");

    file.close();

    
    Serial.println("otprav schedule");
}
void developers1(){
    //SPIFFS.begin();
    File file = SPIFFS.open("/developers.html", "r");
 
    webServer.streamFile(file,"text/html");
  
    file.close();
  
    Serial.println("otprav developers");
}
void watering_now(){

 
 digitalWrite(GPIOPIN[0], HIGH); //пин 0 на помпу не забыть
  etatGpio[0] = "On";
  delay(3000); // тут надо посчитать сколько за секунду льется воды и сделать например параметры умеренный/обильны полив
  digitalWrite(GPIOPIN[0], LOW); //пин 0 на помпу не забыть
  etatGpio[0] = "Off";
  Serial.println("Flower was watered");
  watering_last_time = timeClient.getEpochTime();  
   File file = SPIFFS.open("/flower.html", "r");
    webServer.streamFile(file,"text/html");
    file.close();
    Serial.println("otprav flower");
}
void scheduling(){
  
   // if(webServer.hasArg("text")){
    
  String date_next_watering = webServer.arg("plain");
  if (date_next_watering == ""){
     Serial.println("null"); 
    
  }
  Serial.println(date_next_watering); 
   Serial.println("scheduling button"); 
  write_date_to_file_change(date_next_watering);
  // File file = SPIFFS.open("/flower.html", "r");s
   // webServer.streamFile(file,"text/html");
    //file.close();
    Serial.println("otprav scheduler");
    
   // }
}
int getSoilMoisture(){

  uint32_t sensor; 
  sensor = analogRead(SENSOR);             
  Serial.println(sensor);
  SOIL_MOISTURE = (MIN - sensor) / ((MIN - MAX) / 100);   
  Serial.println(sensor);                  
  Serial.println(SOIL_MOISTURE);   
  delay(500); 
                               
return SOIL_MOISTURE;
}
//в процентах
int getWaterCapacity(){
 // 100% - 10cm
 // free - 12 cm
 digitalWrite(trigPin, LOW); 
 delayMicroseconds(2); 
 digitalWrite(trigPin, HIGH);
 delayMicroseconds(10); 
 digitalWrite(trigPin, LOW);
 duration = pulseIn(echoPin, HIGH);
 Serial.println(duration); // Custom text
 distance = duration/58.2; //Calculate the distance (in cm) based on the speed of sound.

 Serial.print("The Water Level is");
 Serial.print(distance); // Custom text
 Serial.println("cm");
 WATER_CAPACITY  = (distance - 2.5) * (100/13.5); 

return (100 - WATER_CAPACITY);
  
}










void loop() {

    if (CONFIG && WIFI_ON){
      reconnectWiFi();
    }
    
    if (digitalRead(wifi_rst_button) == LOW){
     delay(2);
     WiFi.disconnect();
     SPIFFS.begin();
     flag = true;
     WIFI_ON = true;
     SPIFFS.remove("/ip_config.txt");
     CONFIG = false;
     ESP.restart();
    }
  //Serial.println("loop");
    if (flag){
    dnsServer.processNextRequest();
    }

    if (!ntp_flag and CONFIG){
    ntp_flag = true;

    // Initialize a NTPClient to get time
    timeClient.begin();
    timeClient.setTimeOffset(10800); 
  }

  /*if (ntp_flag){

       while(!timeClient.update()) {
     timeClient.forceUpdate();
    }


   if (timeClient.getEpochTime() == watering_last_time + 60*60*24*period){
      water_flower();
    }


    if (millis() - last_check > 10000){
      //checkValues();
      last_check = millis();
    }
  }  */
  
    webServer.handleClient();
}

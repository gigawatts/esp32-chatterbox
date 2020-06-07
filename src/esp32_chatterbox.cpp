/*  ESP32 ChatterBox
 *  Running on a Sparkfun ESP32 Thing
 *  Source available at: https://github.com/gigawatts/esp32-chatterbox 
 *  Forked from: https://github.com/fenwick67/esp32-chatterbox
 *  See README.md for more info
*/

#include <Arduino.h>
#include <WiFi.h>
//#include <DNSServer.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <Update.h>
#include "HTMLPAGE.h"

#define FORMAT_SPIFFS_IF_FAILED true
#define RECORD_SEP "\x1E"

IPAddress apIP(10, 1, 1, 1);
IPAddress gateway = apIP;
IPAddress subnet(255, 255, 255, 0);

//const byte DNS_PORT = 53;
//DNSServer dnsServer;
WebServer server(80);

const char* filename = "/posts.txt";

// comment out the below line if you don't want to blink a LED when a new message arrives
#define MESG_NOTIFY

#ifdef MESG_NOTIFY
#define LED 13
unsigned long previousMillis = 0;  // will store last time LED was updated
const long interval = 200;  // interval at which to blink (milliseconds)
const int maxBlinkCount = 20;  // number of times to toggle the LED
int blinkCount = 0;  // keeps track of how many times we have blinked
int ledState = LOW;  // ledState used to set the LED

bool newMessage = false;
#endif


// Functions -------------------------------------------
void sendPage(){
  Serial.println("GET /");
  server.send(200,"text/html",HTMLPAGE);
}

void sendMessages(){
  //Serial.println("GET /posts");
  File file = SPIFFS.open(filename, FILE_READ);
  if(!file){
      Serial.println("- failed to open file for reading");
  }
  server.streamFile(file,"text/plain");
  file.close();
}

//void downloadFile(){ }

void receiveMessage(){
  //Serial.println("POST /post");
  int argCount = server.args();
  if (argCount == 0){
    Serial.println("zero args?");
  }
  
  File file = SPIFFS.open(filename, FILE_APPEND);
  if(!file){
      Serial.println("- failed to open file for writing");
  }
  
  if (argCount == 2){
    //with argument names
    String user = "Anon";
    String mesg = "";

    // parse POST arguments for user and mesg
    for (int i = 0; i < server.args(); i++) {
      if(server.argName(i) == "user"){
        user = server.arg(i);
      } else if(server.argName(i) == "mesg") {
        mesg = server.arg(i);
      }
    }

    String line = user + ": " + mesg;
    line.replace(String(RECORD_SEP),String(""));
    file.print(line);
    file.print(RECORD_SEP);
    #ifdef MESG_NOTIFY
      newMessage = true;
      blinkCount = 0;
    #endif
    Serial.print("New message: ");
    Serial.println(line);
  }
  file.close();
  server.send(200,"text.plain","");
}

void eraseHistory(){
  Serial.print("overwriting file... ");
  File file_write = SPIFFS.open(filename, FILE_WRITE);
  file_write.print("");
  file_write.close();      
  Serial.println("file erased");
}

void clearMessages(){
  eraseHistory();
  String message = "<html>"
  "<head><meta http-equiv='Refresh' content='3; URL=/'></head>"
  "Messages cleared!<br>Redirecting in 3 seconds..."
  "</html>";
  server.send(200, "text/html", message);
}

void browseFiles(){
  // Code borrowed from https://www.esp8266.com/viewtopic.php?p=80943
  Serial.println("GET /browse");
  String message = "<!DOCTYPE HTML><html>";
  message += "<head><title>SPIFFS Browser</title><head />";
  message += "<body>";
  // print all the files, use a helper to keep it clean
  message += "<h2>SPIFFS Browser</h2>";
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file)
  {
    message += "<a href=\"";
    message += file.name();
    message += "\">";
    message += file.name();
    message += "</a>";
    message += "    ";
    message += file.size();
    message += "<br>\r\n";
    file = root.openNextFile();
  }
  message += "</body></html>";
  server.send(200, "text/html", message);
}

void updateForm(){
  String message = "<!DOCTYPE html><html lang='en'>"
  "<head>"
  "<meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
  "</head>"
  "<body>"
  "<form method='POST' action='' enctype='multipart/form-data'>"
  "Firmware:<br><input type='file' accept='.bin,.bin.gz' name='firmware'>"
  "<input type='submit' value='Update Firmware'>"
  "</form>"
  "<form method='POST' action='' enctype='multipart/form-data'>"
  "FileSystem:<br><input type='file' accept='.bin,.bin.gz' name='filesystem'>"
  "<input type='submit' value='Update FileSystem'>"
  "</form></body></html>";
  server.send(200, "text/html", message);
}

// End of Functions -------------------------------------------

void setup() {
  Serial.begin(115200);

  #ifdef MESG_NOTIFY
    pinMode(LED, OUTPUT);
  #endif

  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // initialize file (not totally sure this is necessary but YOLO)
  File file = SPIFFS.open(filename, FILE_READ);
  if(!file){
      file.close();
      File file_write = SPIFFS.open(filename, FILE_WRITE);
      if(!file_write){
          Serial.println("- failed to create file?!?");
      }
      else{
        file_write.print("");
        file_write.close();
      }
  }else{
    file.close();
  }
  
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ChatterBox");
  delay(2000);  // stops ESP32 crash on new client connect
  WiFi.softAPConfig(apIP, gateway, subnet);

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  //dnsServer.start(DNS_PORT, "*", apIP);

  // init http server
  server.on("/", HTTP_GET, sendPage);
  server.on("/index.html", HTTP_GET, sendPage);
  server.on("/posts", HTTP_GET, sendMessages);
  server.on("/post", HTTP_POST, receiveMessage);
  server.on("/clear", HTTP_GET, clearMessages);
  server.on("/browse", HTTP_GET, browseFiles);
  //server.on(filename, HTTP_GET, downloadFile);

  // handling uploading firmware file
  server.on("/update", HTTP_GET, updateForm);
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();

  Serial.println("Setup complete");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  //dnsServer.processNextRequest();
  server.handleClient();

  // type a "c" in the serial monitor to clean the database
  // type a "r" in the serial monitor to read messages
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();
    if(incomingByte == 99){
      clearMessages();
    
    } else if(incomingByte == 114) {
      Serial.println("reading file...");
      File file = SPIFFS.open(filename, FILE_READ);
      Serial.println("## Message file contents ##");
      char temp;
      while (file.available()) {
        temp = file.read();
        if (temp == '\x1E') 
        {
          Serial.println("");
        } else { 
          Serial.write(temp);
        }
      }
      file.close();
      Serial.println("## End of File ##");
    }
  }

  #ifdef MESG_NOTIFY
  //loop to blink a LED without delay
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {

    // save the last time you blinked the LED
    previousMillis = currentMillis;
    
    // only blink the LED if a new message arrived recently
    if (newMessage == true) {
      // if the LED is off turn it on and vice-versa:
      ledState = not(ledState);
      // set the LED with the ledState of the variable:
      digitalWrite(LED, ledState);
      blinkCount++;
    } 
    
    if (blinkCount >= maxBlinkCount) {
      newMessage = false;
      digitalWrite(LED, LOW);
    }
  }
  #endif

}

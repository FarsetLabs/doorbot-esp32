#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <MFRC522.h> //library responsible for communicating with the module RFID-RC522
#include <SPI.h> //library responsible for communicating of SPI bus


#ifdef ESP8266
  #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
  #define CHIP_ID   ((uint32_t)ESP.getChipId())
#else
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #define CHIP_ID   ((uint32_t)ESP.getEfuseMac())
#endif

#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

#define SS_PIN    21
#define RST_PIN   22
#define SIZE_BUFFER     18
#define MAX_SIZE_BLOCK  16
#define greenPin     2
#define redPin       4
#define relayPin     15

//used in authentication
MFRC522::MIFARE_Key key;
//authentication return status code
MFRC522::StatusCode status;
// Defined pins to module RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); 

char addr_str[21];  // Stores the scanned ID as a str-able


const int WIFI_CONNECT_TIMEOUT_SECONDS = 10;

WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);
HTTPClient http;

String device_id;
String client_id;
String pub_topic;
String sub_topic;
String space_name = SPACE_NAME;

struct HTTPResponse {
   int status_code;
   String response;
};

String get_device_id() {
  uint64_t mac_integer = CHIP_ID;
  uint8_t *mac = (uint8_t*) &mac_integer;
  char mac_chars[13];
  for (int i=0; i<6; i++) {
    sprintf(mac_chars + i * 2, "%" PRIx8, mac[i]);
  }
  return mac_chars;
}

boolean connect_wifi() {
  Serial.print(F("Connecting to wifi "));
  Serial.println(WIFI_SSID);
  WiFi.disconnect() ;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i=0; i<WIFI_CONNECT_TIMEOUT_SECONDS && WiFi.status() != WL_CONNECTED; i++) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("Connected to "));
    Serial.println(WIFI_SSID);
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println(F("Failed to connect to WiFi; current status = "));
    Serial.println(WiFi.status());
    return false;
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
 
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
 
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
 
  Serial.println();
  Serial.println("-----------------------");

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  JsonObject object = doc.as<JsonObject>();
  String new_state = object["state"];
  Serial.println(new_state);
  
}

boolean reconnect_mqtt() {
  if (mqtt_client.connected()) {
    Serial.println(F("Still connected to MQTT broker"));
  }
  Serial.println(F("Reconnecting to MQTT broker"));
  mqtt_client.connect(client_id.c_str());
  if (mqtt_client.connected()) {
    Serial.println(F("Connected to MQTT broker"));
    mqtt_client.subscribe(sub_topic.c_str());
    return true;
  } else {
    Serial.println(F("Failed to connect to MQTT broker"));
    Serial.print(mqtt_client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
    return false;
  }
}

void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

//reads data from card/tag
boolean readCardId()
{
  //prints the technical details of the card/tag
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid)); 
  
  //prepare the key - all keys are set to FFFFFFFFFFFFh
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  
  //buffer for read data
  byte buffer[SIZE_BUFFER] = {0};
 
  //the block to operate
  byte block = 1;
  byte size = SIZE_BUFFER;  //authenticates the block to operate
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)); //line 834 of MFRC522.cpp file
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    digitalWrite(redPin, HIGH);
    delay(1000);
    digitalWrite(redPin, LOW);
    return false;
  }

  //read data from block
  status = mfrc522.MIFARE_Read(block, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Reading failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    digitalWrite(redPin, HIGH);
    delay(1000);
    digitalWrite(redPin, LOW);
    return false;
  }
  else{
    digitalWrite(greenPin, HIGH);
    delay(100);
    digitalWrite(greenPin, LOW);
  }

  array_to_string(mfrc522.uid.uidByte, mfrc522.uid.size, addr_str);
  Serial.println(addr_str);

  return true;
}

void setup() 
{
  Serial.begin(115200);
  SPI.begin(18, 19, 23, 21); //SCK, MISO, MOSI, SS // Init SPI bus
  //Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);

  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(relayPin, OUTPUT);

  digitalWrite(greenPin, LOW);
  digitalWrite(redPin, HIGH);
  digitalWrite(relayPin, HIGH);

  while(!connect_wifi()){
    Serial.println("Couldn't connect to Wifi, sleeping for 5 seconds");
    ESP.deepSleep(5*1e6);
  }
  Serial.print("Connected; Device ID: ");
  Serial.println(get_device_id());

  // Init MFRC522
  mfrc522.PCD_Init(); 
  digitalWrite(greenPin, LOW);
  digitalWrite(redPin, LOW);
  digitalWrite(relayPin, LOW);
  Serial.println("Approach your reader card...");
  Serial.println();

  wifi_client.setInsecure();
}

HTTPResponse make_request(){
  HTTPResponse response;
  // Lets play nexudus!
  //
  http.begin(wifi_client, "https://"+space_name+".spaces.nexudus.com/api/public/checkin"); 
  http.addHeader("Content-Type", "application/json");
  response.status_code = http.POST(
    "{\"AccessCardID\":\""+String(addr_str)+"\"}"
  );
  Serial.println(addr_str);

  if (response.status_code>0) {
    
    Serial.print("HTTP Response code: ");
    Serial.println(response.status_code);
    response.response = http.getString();
    Serial.println(response.response);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(response.status_code);
    Serial.println(http.getString());
  }
  // Free resources
  http.end();

  return response;
}


void loop() {

   //waiting the card approach
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  {
    delay(400);
    digitalWrite(greenPin, HIGH);
    delay(100);
    digitalWrite(greenPin, LOW);
    return;
  }
  // Select a card
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  {
    return;
  }

  // Dump debug info about the card; PICC_HaltA() is automatically called
  //  mfrc522.PICC_DumpToSerial(&(mfrc522.uid));</p><p>  //call menu function and retrieve the desired option
  
  if (readCardId()) {
    HTTPResponse response = make_request();
    // Nexudus helpfully doesn't use HTTP status as actual status, so we hae to break the previous doorbot 'cleverness' (thank goodness)
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response.response);
    if (doc["Status"].as<int>()==200){
      Serial.print("Authorised:");
      Serial.println(doc["Value"]["FullName"].as<String>());
      digitalWrite(greenPin, HIGH);
      digitalWrite(relayPin, HIGH);
      delay(5000);
      digitalWrite(greenPin, LOW);
      digitalWrite(relayPin, LOW);
    } else {
      Serial.println("Unuthorised");
      for (int i = 0; i < 5; i++){
        digitalWrite(redPin, HIGH);
        delay(100);
        digitalWrite(redPin, LOW);
        delay(100);
      }
    }
  }
 
  //instructs the PICC when in the ACTIVE state to go to a "STOP" state
  mfrc522.PICC_HaltA(); 
  // "stop" the encryption of the PCD, it must be called after communication with authentication, otherwise new communications can not be initiated
  mfrc522.PCD_StopCrypto1();  
}

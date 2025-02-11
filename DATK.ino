#include <DHT.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

const char* ssid = "Sinh Viên Năm 8";
const char* password = "sinhvien8nam";

const char* mqtt_server = "demo.thingsboard.io";
 
const char* access_token = "KvfdA9eVszvfXyZe4Mxr";
const int mqtt_port = 1883;

WiFiClient thingboard_Client;
PubSubClient client(thingboard_Client);

// Định nghĩa chân UART cho A7680C
#define SIM_TX 27
#define SIM_RX 26
#define DHTPIN 4
#define DHTTYPE DHT11
#define PHPIN 35
#define BATPIN 32
#define SDA_PIN 14
#define SCL_PIN 13
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);
HardwareSerial simModule(1);

unsigned long timer = millis();
float h,t,ph,ph_round;
int DELAY = 5000;
int bat;
int chuki = 5000;
int interval;
int status = 1;
String smsContent, incomingMessage, senderNumber, messageContent;

void setup_wifi() {
  delay(10);
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}  

void callback_thingsboard(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived from ThingsBoard [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  const char* method = doc["method"];
  if (String(method) == "setValue") {
    bool mode = doc["params"];
    if (mode){
      status = 0;
      Serial.println("SleepMode");
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,20);
      display.print("Dang trong SleepMode !!");
      display.display(); 
    }
    else {
      status = 1;
      Serial.println("Wake up");
    }
  }
  else if((String(method) == "setDelay")){
    interval = doc["params"];
    chuki = DELAY*interval/100;
  }
}

void reconnect(PubSubClient& client, const char* clientID, const char* accessToken = nullptr) {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientID, accessToken, NULL)) {
      Serial.println("connected thingboard");
      client.subscribe("v1/devices/me/rpc/request/+"); 
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("thingboard try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback_thingsboard);
  simModule.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  
  // Khởi động cảm biến DHT11
  dht.begin();

  Serial.println("Khởi động module SIM...");
  if (sendATCommand("AT", 1000, "OK")) {
    Serial.println("Module SIM sẵn sàng.");
  } else {
    Serial.println("Không thể khởi động module SIM.");
  }

  // Bước 2: Kiểm tra SIM có sẵn không
  Serial.println("Kiểm tra SIM...");
  if (sendATCommand("AT+CPIN?", 3000, "READY")) {
    Serial.println("SIM đã sẵn sàng.");
  } else {
    Serial.println("SIM không được chèn hoặc không hợp lệ.");
  }

  // Bước 3: Kiểm tra tín hiệu sóng
  Serial.println("Kiểm tra sóng...");
  if (sendATCommand("AT+CSQ", 3000, "OK")) {
    Serial.println("Tín hiệu sóng OK.");
  } else {
    Serial.println("Không có tín hiệu sóng.");
  }

  // Bước 4: Kiểm tra kết nối mạng
  Serial.println("Kiểm tra kết nối mạng...");
  if (sendATCommand("AT+CREG?", 2000, "+CREG: 0,1")) {
    Serial.println("Đã kết nối vào mạng.");
  } else {
    Serial.println("Chưa kết nối vào mạng.");
  }
  // Khởi động module SIM và thiết lập chế độ SMS
  Serial.println("Thiết lập chế độ SMS...");
  if (sendATCommand("AT+CMGF=1",1000,"OK"))// Thiết lập chế độ SMS
  {
    Serial.println("Success!!!");
  } else {
    Serial.println("Fail");
  }
// Nhận tin nhắn SMS tự động
  Serial.println("Nhận tin nhắn SMS tự động...");
  if (sendATCommand("AT+CNMI=1,2,0,0,0",1000,"OK"))
  {
    Serial.println("Success!!!");
  } else {
    Serial.println("Fail");
  }
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);
}

void loop() {
  if (!client.connected()) {
    reconnect(client, "ESP32Gateway", access_token);
  }
  client.loop();
  if (status){
  if (millis()-timer > chuki){
    h = dht.readHumidity();
    t = dht.readTemperature();
    for(int i=0;i<9;i++){
      ph += (analogRead(PHPIN)/1240.9+0.15)*2.8;
      bat += (int)(analogRead(BATPIN)/38.61);
    }
    ph = ph/10;
    ph_round = roundf(ph*100)/100;
    bat = bat/10;
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      Serial.println("Nhiet do: "+ String(t) +" , Do am: " + String(h)+", PH: "+String(ph_round)+ ", Pin: "+String(bat));
      String payload = "{";
      payload += "\"temperature\":"; payload += String(t); payload += ",";
      payload += "\"humidity\":"; payload += String(h); payload += ",";
      payload += "\"PH\":"; payload += String(ph_round); payload += ",";
      payload += "\"batteryLevel\":"; payload += String(bat); payload += ",";
      payload += "\Interval\":"; payload += String(chuki);
      payload += "}";

      client.publish("v1/devices/me/telemetry", (char*) payload.c_str());
      Serial.print("Publish message: ");
      Serial.println((char*) payload.c_str());
      display.clearDisplay();
    
    // hiển thị nhiệt độ
  
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print("Temperature: ");
      display.setCursor(0,10);
      display.print(t);
      display.print(" ");
      display.setTextSize(1);
      display.cp437(true);
      display.write(167);
      display.setTextSize(1);
      display.print("C");
      
      // hiển thị độ ẩm
      display.setCursor(0, 20);
      display.print("Humidity: ");
      display.setCursor(0, 30);
      display.print(h);
      display.print(" %"); 
      
      display.setCursor(0, 40);
      display.print("PH: ");
      display.setCursor(0, 50);
      display.print(ph_round);

      display.setCursor(60, 40);
      display.print("Pin: ");
      display.setCursor(60, 50);
      display.print(bat);
      display.print(" %"); 

      display.setCursor(60, 40);
      display.print("Pin: ");
      display.setCursor(60, 50);
      display.print(bat);
      display.print(" %"); 

      display.display(); 
    }
    timer = millis();   
  }
  if (simModule.available()) 
  {
    incomingMessage = simModule.readString();
    if (incomingMessage.indexOf("+CMT:") != -1) {
    // Tìm số điện thoại người gửi
    int phoneStart = incomingMessage.indexOf("\"") + 1;
    int phoneEnd = incomingMessage.indexOf("\"", phoneStart);
    senderNumber = incomingMessage.substring(phoneStart, phoneEnd);
    
    // Tìm vị trí bắt đầu của nội dung tin nhắn
    int msgStart = incomingMessage.indexOf("\n", phoneEnd) + 1;
    messageContent = incomingMessage.substring(msgStart);
    
    Serial.println("Số điện thoại người gửi: " + senderNumber);
    Serial.println("Nội dung tin nhắn: " + messageContent);

    //if (messageContent == "info\n"){
      smsContent = "Nhiet do: " + String(t) + "do C, Do am: " + String(h) + "%, PH: "  + String(ph_round) + ", Pin: "+String(bat) +"%";
    //}
    //else{
    //  smsContent = "Nhap đung cu phap";
    //}
    sendSMS(senderNumber, smsContent);
    Serial.println(senderNumber);
    Serial.println(smsContent);
    }
  }
  }
}

bool sendATCommand(String command, unsigned long timeout, String expectedResponse) {
  simModule.println(command);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (simModule.available()) {
      String response = simModule.readString();
      if (response.indexOf(expectedResponse) != -1) {
        return true;
      }
    }
  }
  return false;
}

void sendSMS(String number, String message) {
  simModule.println("AT+CMGS=\"" + number + "\"");
  delay(100);
  simModule.println(message);
  delay(100);
  simModule.write(26); // Ký tự CTRL+Z để gửi tin nhắn
  delay(100);
  Serial.println("Đã gửi");
}
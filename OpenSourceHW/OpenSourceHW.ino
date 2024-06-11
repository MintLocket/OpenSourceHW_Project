#include <ESP8266WiFi.h> //ESP8266 헤더
#include <WiFiUdp.h> //UDP 헤더
#include <NTPClient.h> //NTP 헤더
#include "sha1.h" //SHA1 헤더
#include "TOTP.h" //TOTP 헤더
#include <Keypad.h> //키패드 헤더
#include <LiquidCrystal_I2C.h> //I2C LCD 헤더
#include <Ticker.h> //타이머 인터럽트 헤더

/*===Pakcet==================================================================*/
#define ACK 0xE0
#define NAK 0xF0
#define OP 0x01
#define CL 0x02 //해당 코드에서는 사용되지 않으나 기능 확장용으로 추가
#define AL 0x10

char packet[4] = {0x47, 0x00, 0x00, 0x0A};

/*===WiFi & UDP & NTP========================================================*/
const char* ssid = "YOUR_WIFI"; //WiFi 이름
const char* password = "YOUR_WIFI_PASSWORD"; //WiFi 비밀번호

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long timestamp; 

/*===LCD=====================================================================*/
LiquidCrystal_I2C lcd(0x27,16,2);

/*===Key Pad=================================================================*/
// 키패드의 행, 열의 갯수
const byte rows = 4;
const byte cols = 3;

// 키패드 버튼 위치 설정
char keys[rows][cols] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

//WeMos D1 R1 호환 보드 문제인지 일반 핀 번호로 사용시 wdt 오류 발생하였음, 해당 부분은 보드마다 다를 수 있음
byte rowPins[rows] = {D6, D5, D4, D3}; 
byte colPins[cols] = {D7, D8, D9};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, rows, cols);

char position = 5; 
char wrong = 0;

char input[6];

/*===OTP=====================================================================*/
uint8_t hmacKey [] = {0x4d, 0x79, 0x4c, 0x65, 0x67, 0x6f, 0x44, 0x6f, 0x6f, 0x72};
char hmacKeyLen = sizeof(hmacKey) / sizeof(hmacKey[0]);
TOTP totp = TOTP(hmacKey, hmacKeyLen);

char code[7];

/*===Timer===================================================================*/
Ticker timer;

/*===Function================================================================*/
//NTP 서버에서 시간 가져오는 함수
unsigned long getTime() { 
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

//와이파이 설정 함수
void initWiFi() { 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

//키패드에 입력한 코드와 생성한 OTP 코드를 비교하는 함수
bool cmpOTP()
{
  for(int i = 0 ; i < 6 ; i++){
    if(input[i] != code[i])
      return false;
  }
  return true;
}

//LCD 출력
void printLCD(){
  lcd.setCursor(1,0);
  lcd.print("Input OTP Code");
  lcd.setCursor(5,1);
  lcd.print("******");
}

//키패드 입력 처리 함수
void inputKeypad(char key){ 
  // 키패드에서 입력된 값을 확인하여 맞게 입력된 값일 경우 비교
  if ((key >= '0' && key <= '9') || (key == '*' || key == '#')) {
    if (key == '*') { //* 입력 시 초기화
      lcd.clear();
      printLCD();
      position = 5; 
    } else if(key == '#'){ // # 입력 시 한 칸 지움
      if(position > 5){
        position--;
        lcd.setCursor(position, 1);
        lcd.print("*");
      }
    } 
    else {
        if(position == 5) {
          lcd.clear();
          printLCD();
      }
      lcd.setCursor(position, 1);
      lcd.print(key);
      input[position-5] = key;
      position++; // 다음 자리로 넘어 감
    } 

    if (position >= 11) { // 6자리 전부 입력된 경우
      if (cmpOTP()) { // 비밀번호 일치한 경우
        packet[1] = OP;
        packet[2] = packet[1] ^ 0xFF;
        timer.attach(10, sendPacket);
        position = 5;
        wrong = 0;
        lcd.setCursor(position, 1);
        lcd.print("OPEN!!");
      } 
      else { // 비밀번호 틀린 경우
        position = 5; 
        wrong++;
        lcd.setCursor(position, 1);
        lcd.print("WRONG!");
        if(wrong >= 5) {
          packet[1] = AL;
          packet[2] = packet[1] ^ 0xFF;
          timer.attach(10, sendPacket);
          wrong = 0;
        }
      }
    }
  }
}

//패킷 전송 함수
void sendPacket() {
  for (byte i = 0; i < 4; i++) {
    Serial.write(packet[i]);
  }
}

/*===Main====================================================================*/
void setup() {
  //init WiFi
  initWiFi();
  timeClient.begin();

  //serial init
  Serial.begin(115200);

  //lcd init
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  printLCD();
}

void loop() {
  //OTP 생성
  timestamp = getTime();
  
  char* newCode = totp.getCode(timestamp);
  if (strcmp(code, newCode) != 0) {
    strcpy(code, newCode);
    Serial.println(code);
  }
  
  //키패드 처리
  char key = keypad.getKey();

  if(key){
    inputKeypad(key);
  }
}

/*===Serial Event============================================================*/
void serialEvent() {
  if(Serial.available()){
    byte read_data = Serial.read();
    if(read_data == ACK){
      timer.detach();
      packet[1] = 0x00;
      packet[2] = 0x00;
    }
    else if(read_data == NAK){
      timer.detach();
      timer.attach(10, sendPacket);
    }
  }
}
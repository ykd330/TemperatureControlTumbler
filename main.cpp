/*-----------include-----------*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <FS.h>
#include <LittleFS.h>
// #include "u8g2_font_unifont_t_NanumGothic.h"
//--------------------------------------------------

/*----------전역변수 / 클래스 선언부----------*/
/*-----Display Setting-----*/
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); // I2C 핀 설정

/*-----GPIO 설정부-----*/
enum GPIO_PIN
{
  BATTERY_STATUS_FIN = 0, // 배터리 상태 핀
  CHARGE_STATUS_FIN = 1,  // 충전 상태 핀
  COOLER_PIN = 2,         // 냉각 제어 핀
  HEATER_PIN = 3,         // 가열 제어 핀
  ONE_WIRE_BUS = 4,       // DS18B20 센서 핀
  BUTTON_BOOT = 5,        // 모드 변경 버튼
  BUTTON_UP = 6,          // 설정온도 상승 버튼
  BUTTON_DOWN = 7,        // 설정온도 하강 버튼
  // GPIO 8 :    : SDA  / GPIO 9 :    : SCL
};

/*-----Temperature Sensor Setting-----*/
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int temperatureC = 0;                 // 현재 온도 저장 변수
int RTC_DATA_ATTR userSetTemperature; // 설정 온도 저장 변수

/*-----시스템 관리 / 제어용-----*/
/*---시스템 모드---*/
enum SystemMode
{
  STANBY_MODE = 0,                  // 대기 모드
  ACTIVE_MODE = 1,                  // 활성화 모드
  TEMPERATURE_MAINTANENCE_MODE = 2, // 유지 모드
  TEMPERATURE_SETTING_MODE = 3,
  BOOTING_MODE = 10 // 부팅 모드
};
SystemMode deviceMode = BOOTING_MODE; // 초기 모드 설정
volatile bool DisplaySleeping = false;

/*---전류 방향 제어---*/
enum ControlMode
{
  HEATER_MODE = 0, // 가열 모드
  COOLER_MODE = 1, // 냉각 모드
  STOP_MODE = 2    // 작동 정지
};
ControlMode ActiveFeltier = STOP_MODE; // 온도 설정 모드 초기화
/*---배터리 관리 설정 부---*/
enum BatteryStatus
{
  BATTERY_STATUS_FULL = 100, // 배터리 완충
  BATTERY_STATUS_LOW = 20    // 배터리 부족
};
#define BATTERY_HIGH_VOLTAGE 4.2             // 배터리 전압 (상)
#define BATTERY_LOW_VOLTAGE 3.0              // 배터리 전압 (하)
volatile long BatteryVoltage = 0;            // 배터리 전압 저장 변수
long BatteryPercentage = 0;                  // 배터리 량
volatile bool BatteryChargeStatus = false;   // 배터리 충전 상태 변수
volatile unsigned long BatteryCheckTime = 0; // 배터리 체크 시간 변수

/*-----GPIO 설정 부-----*/
/*---ESP32-C3 SuperMini GPIO 핀 구성---*/
// GPIO 5 : A5 : MISO /   5V   :    : VCC
// GPIO 6 :    : MOSI /  GND   :    : GND
// GPIO 7 :    : SS   /  3.3V  :    : VCC
// GPIO 8 :    : SDA  / GPIO 4 : A4 : SCK
// GPIO 9 :    : SCL  / GPIO 3 : A3 :
// GPIO 10:    :      / GPIO 2 : A2 :
// GPIO 20:    : RX   / GPIO 1 : A1 :
// GPIO 21:    : TX   / GPIO 0 : A0 :
// PWM, 통신 관련 핀은 임의로 설정 가능함
// gpio 4, 5, 6, 7, 8, 9 사용 중

/*-----열전소자 전류 제어용 PWM / 출력 PIN 설정부-----*/
#define PWM_FREQ 5000    // PWM 주파수 설정 (5kHz)
#define PWM_RESOLUTION 8 // PWM 해상도 설정 (8비트)
#define PWM_CHANNEL 0    // PWM 채널 설정 (0번 채널 사용)

// #define PWM_PIN 1 // PWM 핀 설정 (GPIO 1번 사용)
#define COOLER_PIN 1        // 냉각 제어
#define HEATER_PIN 2        // 가열 제어
unsigned int dutyCycle = 0; //

/*-----시스템 한계 온도 설정-----*/
enum SysyemLimitTemperature
{
  SYSYEM_LIMIT_MAX_TEMPERATURE = 80, // 시스템 한계 온도
  SYSTEM_LIMIT_MIN_TEMPERATURE = 5   // 시스템 한계 온도
};

/*-----Interrupt 버튼 triger 선언부-----*/
volatile bool bootButton = false;
volatile bool upButton = false;   // 설정온도 상승 버튼 상태 변수
volatile bool downButton = false; // 설정온도 하강 버튼 상태 변수
volatile bool Trigger = false;    // 버튼 트리거 상태 변수
volatile bool Trigger_YN = false; // 버튼 트리거 상태 변수
volatile int TM_count = 0;
volatile static unsigned long reBootCheck = 0; // 버튼 트리거 시간 변수
volatile static unsigned long upButtonTime = 0; // upButton Trigger
volatile static unsigned long downButtonTime = 0; // downButton Trigger

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTimeUp = 0;    // 마지막 디바운스 시간 UP
volatile unsigned long lastDebounceTimeDown = 0;  // 마지막 디바운스 시간 DOWN
volatile unsigned long lastDebounceTimeBoot = 0;  // 마지막 디바운스 시간 BOOT
volatile const unsigned long debounceDelay = 150; // 디바운싱 지연 시간 (밀리초) - 더블클릭 현상 방지

/*-----Display 절전모드 제어용 변수-----*/
unsigned long displaySleepTime = 0; // display 절전모드 시간 변수
/*----------전역변수 / 클래스 선언부----------*/

/*----------함수 선언부----------*/
/*------Display Print------*/
// 5. Battery관련 내용을 출력할 방식을 고안해야함.

/*-----Starting Display Print-----*/
void startingDisplayPrint()
{
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("제작 : 5조")) / 2, 23, "제작 : 5조");
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("Temperature")) / 2, 39, "Temperature");
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("Control Tumbler")) / 2, 55, "Control Tumbler");
}

/*-----Base DisplayPrint-----*/
void baseDisplayPrint() // 기본 Display 내용 출력 함수
{
  u8g2.drawLine(0, 13, 127, 13);             // 가로선 그리기
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  if (BatteryChargeStatus == false)
  {
    if (BatteryPercentage == BATTERY_STATUS_FULL)
    {
      u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getUTF8Width("100%")), 12); // 배터리 상태 표시
    }
    else if (BatteryPercentage == BATTERY_STATUS_LOW)
    {
      u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getUTF8Width("100%"))) / 2, 12); // 배터리 상태 표시
      u8g2.print("please charge battery");                                            // 배터리 상태 표시
    }
    else
    {
      u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getUTF8Width("99%")), 12); // 배터리 상태 표시
    }
    u8g2.print(BatteryPercentage);
    u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
    u8g2.print("%");
    u8g2.setFont(u8g2_font_unifont_t_korean2); // 배터리 상태 표시
  }
  else
  {
    u8g2.setFont(u8g2_font_unifont_h_symbols);
    u8g2.drawUTF8(u8g2.getDisplayWidth() - u8g2.getUTF8Width("🗲"), 12, "🗲"); // 충전 중 표시
    u8g2.setFont(u8g2_font_unifont_t_korean2);
  }
}

/*-----ModeDisplayPrint-----*/
volatile unsigned int AaCo = 0; // 대기 중 카운트 변수
void StanbyDisplayPrint()       // 대기 모드 Display 관리 함수
{
  u8g2.setFont(u8g2_font_unifont_t_korean2);                                                     // 폰트 설정
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("현재 온도")) / 2, 30, "현재 온도"); // 현재 온도 출력
  u8g2.setFont(u8g2_font_unifont_h_symbols);                                                     // 폰트 설정
  u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getUTF8Width("10℃")) / 2, 50);                   // 현재 온도 출력
  u8g2.print(int(temperatureC));                                                                 // 현재 온도 출력
  u8g2.print("℃");
}

void ActiveDisplayPrint()
{
  u8g2.drawUTF8(0, 30, "온도 조절 중...");
  u8g2.setCursor(2, 47);
  u8g2.print(temperatureC);                               // 현재 온도 출력
  u8g2.setFont(u8g2_font_unifont_h_symbols);              // 폰트 설정
  u8g2.print("℃");                                        // 현재 온도 출력
  u8g2.setCursor(u8g2.getUTF8Width(" 10℃  ---->  "), 47); // 현재 온도 출력
  u8g2.print(userSetTemperature);                         // 설정 온도 출력
  u8g2.print("℃");                                        // 설정 온도 출력
  u8g2.setFont(u8g2_font_unifont_t_korean2);              // 폰트 설정
  // 애니메이션 효과 - 1초마다 Display에 출력되는 글자 변경
  unsigned int DisplayAnimationPrintWitthFixel = u8g2.getUTF8Width("10℃") + 20; // 애니메이션 효과 시작 위치
  switch ((millis() / 1000) % 6)
  {
  case 0:
    u8g2.drawUTF8(DisplayAnimationPrintWitthFixel, 47, "");
    break;

  case 1:
    u8g2.drawUTF8(DisplayAnimationPrintWitthFixel, 47, "-");
    break;

  case 2:
    u8g2.drawUTF8(DisplayAnimationPrintWitthFixel, 47, "--");
    break;

  case 3:
    u8g2.drawUTF8(DisplayAnimationPrintWitthFixel, 47, "---");
    break;

  case 4:
    u8g2.drawUTF8(DisplayAnimationPrintWitthFixel, 47, "----");
    break;

  case 5:
    u8g2.drawUTF8(DisplayAnimationPrintWitthFixel, 47, "---->");
    break;
  }
  if (ActiveFeltier == HEATER_MODE)
    u8g2.drawUTF8(0, 63, "가열 중"); // 가열 중 출력
  // 2668 if 2615

  if (ActiveFeltier == COOLER_MODE)
    u8g2.drawUTF8(0, 63, "냉각 중"); // 냉각 중 출력
  // 2744 or 2746
}

void TMDisplayPrint() // 유지 모드 Display 관리 함수
{
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.drawUTF8(0, 30, "설정온도: ");
  u8g2.setCursor(u8g2.getUTF8Width("설정온도: "), 30); // 설정 온도 출력
  u8g2.print(userSetTemperature);                      // 설정 온도 출력
  u8g2.drawUTF8(0, 50, "온도 유지 중");                // 설정 온도 출력
  u8g2.setFont(u8g2_font_unifont_h_symbols);           // 폰트 설정
  u8g2.print("℃");                                     // 설정 온도 출력
}

void settingTemperatureDisplayPrint() // 온도 설정 Display 관리 함수
{
  // 글자 위치가 이상하게 출력되는 버그 발견 -> 원인 찾을 필요O - 해결
  u8g2.setCursor(0, 0);               // 커서 위치 설정
  u8g2.drawUTF8(0, 16, "설정온도: "); // 설정 온도 출력
  u8g2.setCursor(u8g2.getUTF8Width("설정온도: "), 16);
  u8g2.print(userSetTemperature);                                                                                                                                   // 설정 온도 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("증가:AAA감소:AAA")) / 2, 38, "증가:   감소:");                                                         // 온도 설정 : 전원 버튼 출력
  u8g2.drawUTF8(0, 60, "완료: 전원버튼");                                                                                                                           // 온도 설정 : 전원 버튼 출력
  u8g2.setFont(u8g2_font_unifont_h_symbols);                                                                                                                        // 폰트 설정
  u8g2.print("℃");                                                                                                                                                  // 설정 온도 출력
  u8g2.drawUTF8(((u8g2.getDisplayWidth() - u8g2.getUTF8Width("증가:AAA감소:AAA")) / 2) + u8g2.getUTF8Width("증가:"), 38, "▲");                                      // 설정 온도 출력
  u8g2.drawUTF8(((u8g2.getDisplayWidth() - u8g2.getUTF8Width("증가:AAA감소:▼▼▼")) / 2) + u8g2.getUTF8Width("증가:▼▼▼") + u8g2.getUTF8Width("감소:") + 30, 38, "▼"); // 설정 온도 출력
  u8g2.setFont(u8g2_font_unifont_t_korean2);                                                                                                                        // 폰트 설정
}

void endedSettingTemperatureDisplayPrint() // 온도 설정 Display 관리 함수
{
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도조절을")) / 2, 16, "온도조절을");   // 온도 조절을 시작 합니다. 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("시작합니다!")) / 2, 32, "시작합니다!"); // 온도 조절을 시작 합니다. 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("화상에")) / 2, 48, "화상에");           // 화상에 주의해 주세요! 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("주의하세요!")) / 2, 64, "주의하세요!"); // 화상에 주의해 주세요! 출력
}
/*-----Main Display Print-----*/

/*------Interrupt 함수 정의 부분------*/
void IRAM_ATTR downButtonF() // Down Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeDown > debounceDelay)
  {
    lastDebounceTimeDown = currentTime;
    displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
    downButton = true;           // 설정온도 하강 버튼 상태 변수
  }
}
void IRAM_ATTR upButtonF() // Up Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeUp > debounceDelay)
  {
    lastDebounceTimeUp = currentTime;
    displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
    upButton = true;             // 설정온도 상승 버튼 상태 변수
  }
}
void IRAM_ATTR bootButtonF() // Boot Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeBoot > debounceDelay)
  {
    lastDebounceTimeBoot = currentTime;
    displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
    bootButton = true;
  }
}

/*------가열 / 냉각 모드 변경 함수------*/
void changeControlMode(ControlMode control_device_mode) // 열전소자 제어 함수
{
  if (control_device_mode == HEATER_MODE)
  {
    digitalWrite(HEATER_PIN, HIGH); // 가열 제어 핀 HIGH
    digitalWrite(COOLER_PIN, LOW);  // 냉각 제어 핀 LOW
  }
  else if (control_device_mode == COOLER_MODE)
  {
    digitalWrite(HEATER_PIN, LOW);  // 가열 제어 핀 LOW
    digitalWrite(COOLER_PIN, HIGH); // 냉각 제어 핀 HIGH
  }
  else if (control_device_mode == STOP_MODE)
  {
    digitalWrite(HEATER_PIN, LOW); // 가열 제어 핀 LOW
    digitalWrite(COOLER_PIN, LOW); // 냉각 제어 핀 LOW
    ledcWrite(PWM_CHANNEL, 0);     // 초기 PWM 값 설정
  }
  ActiveFeltier = control_device_mode; // 현재 모드 저장
}

/*-----사용자 설정 온도 파일 저장-----*/
void saveUserSetTemperature(int tempToSave)
{
  File file = LittleFS.open("/UserTemperature.txt", "w"); // 쓰기 모드("w")로 파일 열기. 파일이 없으면 생성, 있으면 덮어씀.
  if (!file)
  {
    return;
  }

  file.println(tempToSave); // 파일에 온도 값 쓰기 (println은 줄바꿈 포함)
  file.close();             // 파일 닫기
}

void loadUserSetTemperature()
{
  if (LittleFS.exists("/UserTemperature.txt"))
  {                                                         // 파일 존재 여부 확인
    File file = LittleFS.open("/UserTemperature.txt", "r"); // 읽기 모드("r")로 파일 열기
    if (!file)
    {
      userSetTemperature = 50; // 기본값 설정
      return;
    }
    if (file.available())
    {                                              // 읽을 내용이 있는지 확인
      String tempStr = file.readStringUntil('\n'); // 한 줄 읽기
      userSetTemperature = tempStr.toInt();        // 문자열을 정수로 변환
    }
    else
    {
      userSetTemperature = 50; // 기본값 설정
    }
    file.close(); // 파일 닫기
  }
  else
  {
    userSetTemperature = 50;                    // 파일이 없으면 기본값 설정
    saveUserSetTemperature(userSetTemperature); // 기본값으로 파일 새로 생성
  }
}

/*-----Button Logic-----*/
void PushedButtonFunction()
{
  // BootButton Logic
  if (bootButton == true && DisplaySleeping == false)
  {
    if ((deviceMode == TEMPERATURE_MAINTANENCE_MODE || deviceMode == TEMPERATURE_SETTING_MODE || deviceMode == ACTIVE_MODE) && Trigger == false)
    {
      Trigger = true;
      if (deviceMode == ACTIVE_MODE || deviceMode == TEMPERATURE_MAINTANENCE_MODE)
      {
        bootButton = false;
      }
    }
    else if (Trigger == true && Trigger_YN == false)
    {
      Trigger_YN = true;
    }
    else
    {
      bootButton = false;
      if (deviceMode != ACTIVE_MODE)
        deviceMode = TEMPERATURE_SETTING_MODE;
    }
  }

  // 절전모드 Button Logic
  if (DisplaySleeping == true)
  {
    if (bootButton == true)
    {
      displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
      bootButton = false;
    }
    if (upButton == true)
    {
      displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
      upButton = false;
    }
    if (downButton == true)
    {
      displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
      downButton = false;
    }
  }
}

void ButtonTriggerEnableFunction()
{
  if (deviceMode == TEMPERATURE_SETTING_MODE)
  {
    // tempSetting모드에서 Trigger가 활성화 되었을 때 BootButton 동작
    u8g2.clearBuffer();
    endedSettingTemperatureDisplayPrint();
    u8g2.sendBuffer();
    saveUserSetTemperature(userSetTemperature); // 설정 온도 저장
    delay(3000);
    if (((temperatureC >= userSetTemperature) ? temperatureC - userSetTemperature : userSetTemperature - temperatureC) > 0.5)
    {
      deviceMode = ACTIVE_MODE;
      Trigger = false;
      bootButton = false;
    }
    else if (((temperatureC >= userSetTemperature) ? temperatureC - userSetTemperature : userSetTemperature - temperatureC) <= 0.5)
    {
      deviceMode = TEMPERATURE_MAINTANENCE_MODE;
      Trigger = false;
      bootButton = false;
    }
  }
  else
  {
    // Trigger가 활성화 되었을 때 BootButton 동작
    // upButton과 downButton이 true일 때 TM_count를 증가 또는 감소시킴
    if (upButton == true)
    {
      TM_count++;
      upButton = false;
    }
    if (downButton == true)
    {
      TM_count--;
      downButton = false;
    }
  }
}
// 설정 온도 증가 / 감소 버튼 함수
void PushButtonTempSetFunction()
{
  if (digitalRead(BUTTON_UP) == HIGH)
  {
    if (upButtonTime == 0)
      upButtonTime = millis();
    if (upButton == true)
    {
      if (userSetTemperature < SYSYEM_LIMIT_MAX_TEMPERATURE)
      {
        userSetTemperature++;
      }
      upButton = false;
    }
    else if (millis() - upButtonTime >= 2000 && upButton == false) // UpButton Toggle - 미구현 
    {
      if (millis() - upButtonTime <= 6000 && millis() - upButtonTime >= 2000 && (millis() - upButtonTime) % 500 == 0)
      {
        if (userSetTemperature < SYSYEM_LIMIT_MAX_TEMPERATURE)
        {
          userSetTemperature++;
        }
      }
      else if (millis() - upButtonTime >= 6000 && (millis() - upButtonTime) % 150 == 0)
      {
        if (userSetTemperature < SYSYEM_LIMIT_MAX_TEMPERATURE)
        {
          userSetTemperature++;
        }
      }
    }
  }
  else {
    upButtonTime = 0;
  }

  if (digitalRead(BUTTON_DOWN) == HIGH)
  {
    if (downButtonTime == 0)
      downButtonTime = millis();
    if (downButton == true)
    {
      if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
      {
        userSetTemperature--;
      }
      downButton = false;
    }
    else if (millis() - downButtonTime >= 2000)// DownButton Toggle - 미구현
    {
      if (millis() - downButtonTime <= 6000 && millis() - downButtonTime >= 2000 && (millis() - downButtonTime) % 500 == 0)
      {
        if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
        {
          userSetTemperature--;
        }
      }
      else if (millis() - downButtonTime >= 6000 && (millis() - downButtonTime) % 150 == 0)
      {
        if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
        {
          userSetTemperature--;
        }
      }
    }
  }
  else {
    downButtonTime = 0;
  }
}
// Trigger 활성화시 작동되는 함수
void TriggerEnebleFunction()
{

  if (deviceMode == ACTIVE_MODE)
  {
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 조절을")) / 2, 30, "온도 조절을");
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("종료하시겠습니까?")) / 2, 46, "종료하시겠습니까?");
  }
  else if (deviceMode == TEMPERATURE_MAINTANENCE_MODE)
  {
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 유지를")) / 2, 30, "온도 유지를");
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("종료하시겠습니까?")) / 2, 46, "종료하시겠습니까?");
  }
  ButtonTriggerEnableFunction();
  if (TM_count < 0)
  {
    // TM_count < 0 방지
    TM_count = 1;
  }
  switch (TM_count % 2)
  {
  case 0:
    u8g2.drawButtonUTF8(40, 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "YES");
    u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
    u8g2.drawUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ "), 63, "NO");
    break;

  case 1:
    u8g2.drawUTF8(25, 63, "YES");
    u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
    u8g2.drawButtonUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ ") + 10, 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "NO");
    break;
  }
}

void TriggerYNFunction()
{
  // Trigger_YN이 true일 때 TM_count를 증가 또는 감소시킴 -> Yes/No 선택
  if ((TM_count % 2) == 0)
  {
    u8g2.clearBuffer();
    if (deviceMode == ACTIVE_MODE)
    {
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 조절을")) / 2, 30, "온도 조절을");
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("종료합니다.")) / 2, 46, "종료합니다.");
    }
    else if (deviceMode == TEMPERATURE_MAINTANENCE_MODE)
    {
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 유지를")) / 2, 30, "온도 유지를");
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("종료합니다.")) / 2, 46, "종료합니다.");
    }
    u8g2.sendBuffer();
    delay(2500);
    deviceMode = STANBY_MODE;
    Trigger = false;
    Trigger_YN = false;
    bootButton = false;
    TM_count = 0;
  }
  else if ((TM_count % 2) == 1)
  {
    Trigger = false;
    Trigger_YN = false;
    bootButton = false;
    TM_count = 0;
  }
}

void FeltierControlFunction(unsigned int temp)
{
  // 펠티어소자 제어 함수
  if (temperatureC + temp < userSetTemperature)
    changeControlMode(HEATER_MODE);
  else if (temperatureC - temp > userSetTemperature)
    changeControlMode(COOLER_MODE);
}
/*----------함수 선언부----------*/

/*----------setup----------*/
void setup()
{
  Wire.begin(); // I2C 초기화
  /*------pinMode INPUT------*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLDOWN);
  pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(BUTTON_BOOT, INPUT_PULLDOWN);
  pinMode(BATTERY_STATUS_FIN, INPUT);         // 배터리 상태 핀 설정
  pinMode(CHARGE_STATUS_FIN, INPUT_PULLDOWN); // 충전 상태 핀 설정

  /*------pinMode OUTPUT------*/
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);

  /*------DS18B20설정부------*/
  sensors.begin();                     // DS18B20 센서 초기화
  sensors.setWaitForConversion(false); // 비동기식으로 온도 측정
  sensors.requestTemperatures();       // 온도 측정 요청

  /*------display설정부------*/
  u8g2.begin();           // display 초기화
  u8g2.enableUTF8Print(); // UTF-8 문자 인코딩 사용
  u8g2.setPowerSave(0);
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.setDrawColor(1);                      // 글자 색상 설정
  u8g2.setFontDirection(0);                  // 글자 방향 설정

  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, upButtonF, RISING);
  attachInterrupt(BUTTON_DOWN, downButtonF, RISING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, RISING); //

  /*------FS 설정부------*/
  LittleFS.begin(false);    // LittleFS 초기화
  loadUserSetTemperature(); // 설정 온도 불러오기

  /*------PWM설정부------*/
  pinMode(COOLER_PIN, OUTPUT); // PWM 핀 설정
  pinMode(HEATER_PIN, OUTPUT);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  ledcAttachPin(COOLER_PIN, PWM_CHANNEL);           // PWM 핀과 채널 연결
  ledcWrite(PWM_CHANNEL, 0);                        // 초기 PWM 값 설정
}
/*----------setup----------*/

/*----------loop----------*/
void loop()
{
  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static unsigned long AM_count = 0;
  /*Sensors error*/
  if (temperatureC == DEVICE_DISCONNECTED_C)
  {
    u8g2.clearBuffer();
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 센서 오류")) / 2, 30, "온도 센서 오류");
    u8g2.sendBuffer();
    delay(1000);
  }

  /*-----온도 측정부-----*/
  if (sensors.isConversionComplete())
  {
    temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures();             // 다음 측정을 위해 온도 요청
  }

  /*-----Battery 상태 관리 함수-----*/
  // 배터리 연결 후 마무리
  // 배터리 전압을 읽어 배터리 상태를 확인
  // 배터리 값은 1.5 ~ 2.1 V -> 3.0 ~ 4.2 V로 변환 -> 0 ~ 100%로 변환 (4.2V = 100%, 3.0V = 0%)
  BatteryVoltage = analogRead(BATTERY_STATUS_FIN) * 2;                                        // 아날로그 핀 0에서 배터리 전압 읽기
  BatteryPercentage = map(BatteryVoltage, BATTERY_LOW_VOLTAGE, BATTERY_HIGH_VOLTAGE, 0, 100); // 배터리 전압을 PWM 값으로 변환
  if (analogRead(CHARGE_STATUS_FIN) >= 2)
  {
    BatteryChargeStatus = true; // 충전 상태
  }
  else
  {
    BatteryChargeStatus = false; // 비충전 상태
  }

  /*-----Boot 버튼 Long Press Check-----*/
  // Boot 버튼을 5초 이상 누르면 Deep Sleep 모드로 전환 -> 오류 발생시 Deep Sleep 모드로 전환 후 재부팅
  if (digitalRead(BUTTON_BOOT) == HIGH)
  {
    if (reBootCheck == 0)
    {
      reBootCheck = millis();
    }
    if (millis() - reBootCheck >= 5000)
    {
      esp_sleep_enable_timer_wakeup(5 * 1000000);                        // 5초 후 Deep Sleep 모드 해제 설정
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO); // RTC Peripherals 전원 차단
      esp_deep_sleep_start();                                            // Boot버튼을 5초 유지하면 ESP32-C3 Deep Sleep 모드로 전환
      reBootCheck = 0;
    }
  }
  else
    reBootCheck = 0; // Boot버튼을 떼면 reBootCheck 초기화

  PushedButtonFunction(); // 버튼 입력 처리 함수

  /*-----Display Low-Energe Mode-----*/
  if (displaySleepTime + 300000 < millis()) // 10초 이상 버튼이 눌리지 않으면 절전모드로 전환
  {
    DisplaySleeping = true;
    u8g2.setPowerSave(1); // 절전모드 설정
  }
  if (displaySleepTime + 300000 > millis()) // 버튼이 눌리면 절전모드 해제
  {
    DisplaySleeping = false;
    u8g2.setPowerSave(0); // 절전모드 해제
  }
  /*Main System control and Display print*/
  u8g2.clearBuffer();
  baseDisplayPrint();
  switch (deviceMode)
  {
  case STANBY_MODE:
    StanbyDisplayPrint();
    u8g2.sendBuffer();
    break;

  case ACTIVE_MODE:
    baseDisplayPrint();
    ActiveDisplayPrint();
    FeltierControlFunction(2);
    if (((temperatureC >= userSetTemperature) ? temperatureC - userSetTemperature : userSetTemperature - temperatureC) < 1)
    {
      if (AM_count == 0)
        AM_count = millis();
      if (millis() - AM_count >= 5000)
      {
        deviceMode = TEMPERATURE_MAINTANENCE_MODE;
        saveUserSetTemperature(userSetTemperature);
        u8g2.clearBuffer();
        u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 유지를")) / 2, 30, "온도 유지를");
        u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("시작합니다.")) / 2, 46, "시작합니다.");
        u8g2.sendBuffer();
        AM_count = 0;
        delay(2000);
        break;
      }
    }
    else
    {
      AM_count = 0;
    }
    dutyCycle = map(temperatureC, temperatureC, userSetTemperature, 0, 255);
    if (Trigger == false && DisplaySleeping == false)
    {
      PushButtonTempSetFunction();
    }

    if (Trigger == true)
    {
      u8g2.clearBuffer();
      TriggerEnebleFunction(); // Trigger 활성화 - Display에 YES/NO 출력
      if (Trigger_YN == true)
      {
        TriggerYNFunction();
      }
    }
    u8g2.sendBuffer();
    break;

  case TEMPERATURE_MAINTANENCE_MODE:
    if (Trigger == true)
    {
      u8g2.clearBuffer();
      TriggerEnebleFunction(); // Trigger 활성화 - Display에 YES/NO 출력

      if (Trigger_YN == true)
      {
        TriggerYNFunction();
      }
    }
    else
    {
      baseDisplayPrint();
      TMDisplayPrint();
      FeltierControlFunction(2);
    }
    dutyCycle = map(temperatureC, temperatureC, userSetTemperature, 0, 255);
    u8g2.sendBuffer();
    break;

  case TEMPERATURE_SETTING_MODE:
    if (Trigger == true)
    {
      ButtonTriggerEnableFunction();
      break;
    }
    u8g2.clearBuffer();
    settingTemperatureDisplayPrint();
    if (Trigger == false && DisplaySleeping == false)
    {
      PushButtonTempSetFunction();
    }
    u8g2.sendBuffer();
    break;

  case BOOTING_MODE:
    u8g2.clearBuffer();
    startingDisplayPrint();
    u8g2.sendBuffer();
    delay(3000);
    deviceMode = STANBY_MODE;
    break;
  }
  delay(100); // 100ms 대기
}
/*----------loop----------*/
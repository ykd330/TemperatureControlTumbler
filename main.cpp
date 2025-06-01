/*-----------include-----------*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <FS.h>
#include <LittleFS.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
//--------------------------------------------------

/*----------전역변수 / 클래스 선언부----------*/
/*-----GPIO 설정부-----*/
enum GPIO_PIN
{

  COOLER_PWM_PIN = 1, // PWM
  HEATER_PWM_PIN = 2, // PWM
  ONE_WIRE_BUS = 3,   // DS18B20 센서 핀
  BUTTON_BOOT = 5,    // 모드 변경 버튼
  BUTTON_UP = 6,      // 설정온도 상승 버튼
  BUTTON_DOWN = 7,    // 설정온도 하강 버튼
  SDA_I2C = 8,        // Hardware에서 설정된 I2C핀
  SCL_I2C = 9,        // Hardware에서 설정된 I2C핀
  COOLING_PAN = 10,
  COOLER_PIN = 20,    // 냉각 제어 핀
  HEATER_PIN = 21     // 가열 제어 핀
};

/*-----Module Setting-----*/
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); // u8g2 객체 선언
SFE_MAX1704X lipo;                                                // MAX1704x객체 선언
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
/*-----Temperature Sensor Setting-----*/
int temperatureC = 0;                 // 현재 온도 저장 변수
int RTC_DATA_ATTR userSetTemperature; // 설정 온도 저장 변수

/*-----시스템 관리 / 제어용-----*/
/*---시스템 모드---*/
enum SystemMode
{                                   // 0b00000 | Device Mode : 000
  STANBY_MODE = 0,                  // 대기 모드
  ACTIVE_MODE = 1,                  // 활성화 모드
  TEMPERATURE_MAINTANENCE_MODE = 2, // 유지 모드
  TEMPERATURE_SETTING_MODE = 3,
  SETTING_CHECK_MODE = 4,
  ENDED_MODE_CHECK = 5,
  BOOTING_MODE = 6, // 부팅 모드
  OVER_HEATING = 7
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
  BATTERY_STATUS_LOW = 20,   // 배터리 부족
  BATTERY_CHARGE = 0,
  BATTERY_DISCHARGE = 1,
  VCELL_REG = 0x02,
  SOC_REG = 0x04,
  MODE_REG = 0x06,
  CONFIG_REG = 0x0C,
};
BatteryStatus BatteryChargeStatus = BATTERY_DISCHARGE; // 배터리 충전 상태 변수
unsigned long BatteryPercentage = 50;                  // 배터리 량
volatile long BatteryVoltage = 0;                      // 배터리 전압 저장 변수
volatile unsigned long BatteryCheckTime = 0;           // 배터리 체크 시간 변수

/*-----열전소자 전류 제어용 PWM / 출력 PIN 설정부-----*/
enum FeltierControlPWM
{
  PWM_FREQ = 5000,
  PWM_RESOLUTION = 8,
  PWM_COOLING_CHANNEL = 0,
  PWM_HEATING_CHANNEL = 1,
  FELTIER_STANBY = 0,
  FELTIER_HEATING = 1,
  FELTIER_COOLING = 2,
};
unsigned int dutyCycle = 0; // PWM 값 설정용

/*-----시스템 한계 온도 설정-----*/
enum SystemSettingTemperature
{
  SYSYEM_LIMIT_MAX_TEMPERATURE = 80, // 시스템 한계 온도
  SYSTEM_LIMIT_MIN_TEMPERATURE = 5,  // 시스템 한계 온도
  MAXTEMPDIFF_PWM = 10,
  MAXPWM = 255,
  MINPWM = 60
};

/*-----Display함수용 변수-----*/
unsigned long displaySleepTime = 0; // display 절전모드 시간 변수
enum checkreturnPixelMode
{
  WIDTH_TEXT = 0,
  ALIGN_CENTER = 1,
  ALIGN_RIGHT = 2,
};

/*-----Interrupt 버튼 Toggle / Toggle Check Time / Trigger 변수 선언부-----*/
volatile bool bootButton = false;
volatile bool upButton = false;          // 설정온도 상승 버튼 상태 변수
volatile bool downButton = false;        // 설정온도 하강 버튼 상태 변수
bool Trigger = false;                    // 버튼 트리거 상태 변수
bool Trigger_YN = false;                 // 버튼 트리거 상태 변수
bool upButtonLowRepeatToggle = false;    // upButton Toggle 상태 변수
bool upButtonHighRepeatToggle = false;   // upButton Toggle 상태 변수
bool downButtonLowRepeatToggle = false;  // downButton Toggle 상태 변수
bool downButtonHighRepeatToggle = false; // downButton Toggle 상태 변수
bool checkToBootButtonTogle = false;     // 부팅 버튼 토글 상태 변수
int YN_Check = 0;                        // 모드 종료 시 Y / N 선택용 변수

// Toggle 작동 시 시간 확인용 변수
static unsigned long reBootCheckTime = 0;      // 버튼 트리거 시간 변수
static unsigned long upButtonCheckTime = 0;    // upButton Trigger
static unsigned long upButtonToggleTime = 0;   // upButton Trigger
static unsigned long downButtonCheckTime = 0;  // downButton Trigger
static unsigned long downButtonToggleTime = 0; // downButton Trigger

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTimeUp = 0;   // 마지막 디바운스 시간 UP
volatile unsigned long lastDebounceTimeDown = 0; // 마지막 디바운스 시간 DOWN
volatile unsigned long lastDebounceTimeBoot = 0; // 마지막 디바운스 시간 BOOT
const unsigned long debounceDelay = 130;         // 디바운싱 지연 시간 (밀리초) - 더블클릭 현상 방지
/*----------전역변수 / 클래스 선언부----------*/

/*------------------------------Display Print------------------------------*/
int returnTextWidthPixel(const char *Text, checkreturnPixelMode Mode = WIDTH_TEXT) // 출력 Text width값 반환용 함수
{
  switch (Mode)
  {
  case WIDTH_TEXT:
    return u8g2.getUTF8Width(Text);

  case ALIGN_CENTER:
    return (u8g2.getDisplayWidth() - u8g2.getUTF8Width(Text)) / 2;

  case ALIGN_RIGHT:
    return (u8g2.getDisplayWidth() - u8g2.getUTF8Width(Text));

  default:
    return -1;
  }
}
/*-----Starting Display Print-----*/
void startingDisplayPrint() // 시작화면
{
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawUTF8(returnTextWidthPixel("Temperature", ALIGN_CENTER), 39, "Temperature");
  u8g2.drawUTF8(returnTextWidthPixel("Control Tumbler", ALIGN_CENTER), 55, "Control Tumbler");

  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.drawUTF8(returnTextWidthPixel("제작 : 5조", ALIGN_CENTER), 23, "제작 : 5조");
}

/*-----Base DisplayPrint-----*/
void baseDisplayPrint() // 기본 Display 내용 출력 함수 - 가로구분선 / 배터리
{
  u8g2.drawLine(0, 13, 127, 13);             // 가로선 그리기
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  /*Battery System Print*/
  // 배터리 시스템 미완으로 Test 불가능 - Display 작동 부분 정상 / 조건문에서 문제 발생
  if (BatteryChargeStatus == BATTERY_DISCHARGE)
  {
    if (BatteryPercentage == BATTERY_STATUS_FULL)
    {
      u8g2.setCursor(returnTextWidthPixel("100%", ALIGN_RIGHT), 12); // 배터리 상태 표시
      u8g2.print(BatteryPercentage);
      u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
      u8g2.print("%");
      u8g2.setFont(u8g2_font_unifont_t_korean2); // 배터리 상태 표시
    }
    else if (BatteryPercentage == BATTERY_STATUS_LOW)
    {
      u8g2.setCursor((returnTextWidthPixel("please charge battery", ALIGN_CENTER)), 12); // 배터리 상태 표시
      u8g2.print("please charge battery");                                               // 배터리 상태 표시
    }
    else if (BatteryPercentage < BATTERY_STATUS_FULL && BatteryPercentage > BATTERY_STATUS_LOW)
    {
      u8g2.setCursor(returnTextWidthPixel("99%", ALIGN_CENTER), 12); // 배터리 상태 표시
      u8g2.print(BatteryPercentage);
      u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
      u8g2.print("%");
      u8g2.setFont(u8g2_font_unifont_t_korean2); // 배터리 상태 표시
    }
  }
  else if (BatteryChargeStatus == BATTERY_DISCHARGE)
  {
    u8g2.setFont(u8g2_font_unifont_h_symbols);
    u8g2.drawUTF8(returnTextWidthPixel("🗲", ALIGN_RIGHT), 12, "🗲"); // 충전 중 표시
    u8g2.setFont(u8g2_font_unifont_t_korean2);
  }
}

/*-----ModeDisplayPrint-----*/
volatile unsigned int AaCo = 0; // 대기 중 카운트 변수
void StanbyDisplayPrint()       // 대기 모드 Display 관리 함수
{
  u8g2.setFont(u8g2_font_unifont_h_symbols);                       // 폰트 설정
  u8g2.setCursor((returnTextWidthPixel("10℃", ALIGN_CENTER)), 50); // 현재 온도 출력
  u8g2.print(int(temperatureC));                                   // 현재 온도 출력
  u8g2.print("℃");

  u8g2.setFont(u8g2_font_unifont_t_korean2);                                         // 폰트 설정
  u8g2.drawUTF8((returnTextWidthPixel("현재 온도", ALIGN_CENTER)), 30, "현재 온도"); // 현재 온도 출력
}

void ActiveDisplayPrint() // Active모드 디스플레이
{
  const char *AM_AnimationCharacter[8] = {
      // 온도 조절 표시 Animation 출력용 변수
      " ",
      "-",
      "--",
      "---",
      "----",
      "-----",
      "----->",
  };
  u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
  u8g2.setCursor(2, 47);
  u8g2.print(temperatureC);
  u8g2.print("℃");                                        // 현재 온도 출력
  u8g2.setCursor(u8g2.getUTF8Width(" 10℃  ---->  "), 47); // 현재 온도 출력
  u8g2.print(userSetTemperature);                         // 설정 온도 출력
  u8g2.print("℃");
  if (ActiveFeltier == COOLER_MODE)
    u8g2.drawGlyph(returnTextWidthPixel("가열 중"), 63, 2668);
  if (ActiveFeltier == HEATER_MODE)
    u8g2.drawGlyph(returnTextWidthPixel("냉각 중"), 63, 2744);
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 한글 폰트
  u8g2.drawUTF8(0, 30, "온도 조절 중...");   // 현재 온도 출력

  // 애니메이션 효과 - 1초마다 Display에 출력되는 글자 변경
  unsigned int DisplayAnimationPrintWidthFixel = u8g2.getUTF8Width("10℃") + 15; // 애니메이션 효과 시작 위치
  u8g2.setCursor(DisplayAnimationPrintWidthFixel, 47);
  u8g2.print(AM_AnimationCharacter[(millis() / 1000) % 7]);

  // feltier작동모드 출력
  if (ActiveFeltier == HEATER_MODE)
    u8g2.drawUTF8(0, 63, "가열 중"); // 가열 중 출력
  // 2668 if 2615
  if (ActiveFeltier == COOLER_MODE)
    u8g2.drawUTF8(0, 63, "냉각 중"); // 냉각 중 출력
  // 2744 or 2746
}
void TMDisplayPrint() // 유지 모드 Display 관리 함수
{
  const char *TMAM_animationCharacter[5] = {
      "",
      ".",
      "..",
      "..."};

  u8g2.setCursor(returnTextWidthPixel("설정온도: "), 30); // 설정 온도 출력

  u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
  u8g2.print(userSetTemperature);
  u8g2.print("℃");

  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.drawUTF8(0, 30, "설정온도: ");        // 설정 온도 출력
  u8g2.drawUTF8(0, 50, "온도 유지 중");      // 설정 온도 출력
  u8g2.drawUTF8(returnTextWidthPixel("온도 유지 중"), 50, TMAM_animationCharacter[(millis() / 1000) % 4]);
}

void settingTemperatureDisplayPrint()                                                                                                               // 온도 설정 Display 관리 함수
{                                                                                                                                                   // 온도 설정 : 전원 버튼 출력
  u8g2.setFont(u8g2_font_unifont_t_korean2);
  u8g2.setCursor(0, 0);               // 커서 위치 설정
  u8g2.drawUTF8(0, 16, "설정온도: "); // 설정 온도 출력
  u8g2.setCursor(u8g2.getUTF8Width("설정온도: "), 16);
  u8g2.print(userSetTemperature);                                                             // 설정 온도 출력
  u8g2.drawUTF8(returnTextWidthPixel("증가:AAA감소:AAA", ALIGN_CENTER), 38, "증가:   감소:"); // 온도 설정 : 전원 버튼 출력
  u8g2.drawUTF8(0, 60, "완료: 전원버튼");     
  
  u8g2.setFont(u8g2_font_unifont_h_symbols);                                                                                                        // 폰트 설정
  u8g2.print("℃");                                                                                                                                  // 설정 온도 출력
  u8g2.drawUTF8(returnTextWidthPixel("증가:AAA감소:AAA", ALIGN_CENTER) + u8g2.getUTF8Width("증가:"), 38, "▲");                                      // 설정 온도 출력
  u8g2.drawUTF8(returnTextWidthPixel("증가:AAA감소:AAA", ALIGN_CENTER) + u8g2.getUTF8Width("증가:▼▼▼") + u8g2.getUTF8Width("감소:") + 30, 38, "▼"); // 설정 온도 출력
  u8g2.setFont(u8g2_font_unifont_t_korean2);
}

void endedSettingTemperatureDisplayPrint() // 온도 설정 Display 관리 함수
{
  u8g2.drawUTF8(returnTextWidthPixel("온도조절을", ALIGN_CENTER), 16, "온도조절을");   // 온도 조절을 시작 합니다. 출력
  u8g2.drawUTF8(returnTextWidthPixel("시작합니다!", ALIGN_CENTER), 32, "시작합니다!"); // 온도 조절을 시작 합니다. 출력
  u8g2.drawUTF8(returnTextWidthPixel("화상에", ALIGN_CENTER), 48, "화상에");           // 화상에 주의해 주세요! 출력
  u8g2.drawUTF8(returnTextWidthPixel("주의하세요!", ALIGN_CENTER), 64, "주의하세요!"); // 화상에 주의해 주세요! 출력
}
/*-----------------------------Main Display Print-----------------------------*/

/*------------------------Interrupt 함수 정의 부분------------------------------*/
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
/*------------------------Interrupt 함수 정의 부분------------------------------*/

/*------------------------사용자 함수 정의 부분------------------------*/

/*------시스템 내부 feltier 제어 결정 함수------*/
void changeFeltierMode(ControlMode control_device_mode) // 열전소자 제어 함수
{
  if (control_device_mode == HEATER_MODE)
  {
    digitalWrite(HEATER_PIN, HIGH);            // 가열 제어 핀 HIGH
    digitalWrite(COOLER_PIN, HIGH);             // 냉각 제어 핀 LOW
    digitalWrite(COOLING_PAN, HIGH);
    ledcWrite(PWM_COOLING_CHANNEL, 0);
    ledcWrite(PWM_HEATING_CHANNEL, dutyCycle); // 가열 PWM
  }
  else if (control_device_mode == COOLER_MODE)
  {
    digitalWrite(HEATER_PIN, HIGH);             // 가열 제어 핀 LOW
    digitalWrite(COOLER_PIN, HIGH);            // 냉각 제어 핀 HIGH
    digitalWrite(COOLING_PAN, HIGH);
    ledcWrite(PWM_HEATING_CHANNEL, 0);
    if(temperatureC < 25)
      ledcWrite(PWM_COOLING_CHANNEL, dutyCycle); // 냉각 PWM
    else
      ledcWrite(PWM_COOLING_CHANNEL, 100); // 냉각 PWM
  }
  else if (control_device_mode == STOP_MODE)
  {
    digitalWrite(HEATER_PIN, LOW); // 가열 제어 핀 LOW
    digitalWrite(COOLER_PIN, LOW); // 냉각 제어 핀 LOW
    digitalWrite(COOLING_PAN, LOW);
    ledcWrite(PWM_HEATING_CHANNEL, 0);
    ledcWrite(PWM_COOLING_CHANNEL, 0);
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

/*-----사용자 설정 온도 파일 불러오기-----*/
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
/*------------------------사용자 함수 정의 부분------------------------*/

/*------------------------Button Logic------------------------*/
void PushedButtonFunction()
{
  // Boot 버튼을 5초 이상 누르면 Deep Sleep 모드로 전환 -> 오류 발생시 Deep Sleep 모드로 전환 후 재부팅
  if (bootButton == true)
    checkToBootButtonTogle = true;                                        // Boot 버튼이 눌리면 checkToBootButtonTogle을 true로 설정
  if (checkToBootButtonTogle == true && digitalRead(BUTTON_BOOT) == HIGH) // boot 버튼 토글 시 로직 - Deep Sleep 모드를 활용한 재부팅
  {
    if (reBootCheckTime == 0)
    {
      reBootCheckTime = millis();
    }
    if (millis() - reBootCheckTime >= 5000)
    {
      checkToBootButtonTogle = false;                                    // Boot 버튼을 5초 이상 누르면 checkToBootButtonTogle을 false로 설정
      bootButton = false;                                                // bootButton을 false로 설정
      esp_sleep_enable_timer_wakeup(3 * 1000000);                        // 3초 후 Deep Sleep 모드 해제 설정
      u8g2.setPowerSave(1);                                              // Display 절전 모드 진입
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO); // RTC Peripherals 전원 차단
      esp_deep_sleep_start();                                            // ESP32-C3 Deep Sleep 모드로 전환
      reBootCheckTime = 0;
    }
  }
  else if (digitalRead(BUTTON_BOOT) == LOW && checkToBootButtonTogle == true) // boot 버튼 작동 시 로직
  {
    reBootCheckTime = 0; // Boot버튼을 떼면 reBootCheckTime 초기화
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
    checkToBootButtonTogle = false; // Boot 버튼을 떼면 checkToBootButtonTogle을 false로 설정
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

// Trigger변수 true일 경우 Button 동작 로직
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
    if (upButton == true)
    {
      YN_Check++;
      upButton = false;
    }
    if (downButton == true)
    {
      YN_Check--;
      downButton = false;
    }
  }
}

// 설정 온도 증가 / 감소 버튼 함수
void PushButtonTempSetFunction()
{
  if (digitalRead(BUTTON_UP) == HIGH)
  {
    if (upButtonToggleTime == 0)
    {
      upButtonToggleTime = millis();
      if (upButton == true)
      {
        if (userSetTemperature < SYSYEM_LIMIT_MAX_TEMPERATURE)
          userSetTemperature++;
        upButton = false;
        upButtonLowRepeatToggle = true;
      }
    }
    if (upButtonLowRepeatToggle && (millis() - upButtonToggleTime >= 1500))
    {
      if (userSetTemperature < SYSYEM_LIMIT_MAX_TEMPERATURE && millis() - upButtonCheckTime >= 300)
      {
        userSetTemperature++;
        upButtonCheckTime = millis();
      }
      if ((millis() - upButtonToggleTime >= 3000))
      {
        upButtonHighRepeatToggle = true;
        upButtonLowRepeatToggle = false;
      }
    }
    else if (upButtonHighRepeatToggle)
      if (millis() - upButtonCheckTime >= 75 && userSetTemperature < SYSYEM_LIMIT_MAX_TEMPERATURE)
      {
        userSetTemperature++;
        upButtonCheckTime = millis();
      }
  }
  else
  {
    upButtonHighRepeatToggle = false;
    upButtonLowRepeatToggle = false;
    upButtonCheckTime = 0;
    upButtonToggleTime = 0;
  }

  if (digitalRead(BUTTON_DOWN) == HIGH)
  {
    if (downButtonToggleTime == 0)
    {
      downButtonToggleTime = millis();
      if (downButton == true)
      {
        if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
          userSetTemperature--;
        downButton = false;
        downButtonLowRepeatToggle = true;
      }
    }
    if (downButtonLowRepeatToggle && (millis() - downButtonToggleTime >= 1500))
    {
      if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE && millis() - downButtonCheckTime >= 300)
      {
        userSetTemperature--;
        downButtonCheckTime = millis();
      }
      if ((millis() - downButtonToggleTime >= 3000))
      {
        downButtonHighRepeatToggle = true;
        downButtonLowRepeatToggle = false;
      }
    }
    else if (downButtonHighRepeatToggle)
      if (millis() - downButtonCheckTime >= 75 && userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
      {
        userSetTemperature--;
        downButtonCheckTime = millis();
      }
  }
  else
  {
    downButtonHighRepeatToggle = false;
    downButtonLowRepeatToggle = false;
    downButtonCheckTime = 0;
    downButtonToggleTime = 0;
  }
}

// Trigger 활성화시 작동되는 함수
void TriggerEnableFunction()
{

  if (deviceMode == ACTIVE_MODE)
  {
    u8g2.drawUTF8(returnTextWidthPixel("온도 조절을", ALIGN_CENTER), 30, "온도 조절을");
    u8g2.drawUTF8(returnTextWidthPixel("종료하시겠습니까?", ALIGN_CENTER), 46, "종료하시겠습니까?");
  }
  else if (deviceMode == TEMPERATURE_MAINTANENCE_MODE)
  {
    u8g2.drawUTF8(returnTextWidthPixel("온도 유지를", ALIGN_CENTER), 30, "온도 유지를");
    u8g2.drawUTF8(returnTextWidthPixel("종료하시겠습니까?", ALIGN_CENTER), 46, "종료하시겠습니까?");
  }
  ButtonTriggerEnableFunction();
  if (YN_Check < 0)
  {
    // YN_Check < 0 방지
    YN_Check = 1;
  }
  switch (YN_Check % 2)
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
  // Trigger_YN이 true일 때 YN_Check를 증가 또는 감소시킴 -> Yes/No 선택
  if ((YN_Check % 2) == 0)
  {
    u8g2.clearBuffer();
    if (deviceMode == ACTIVE_MODE)
    {
      u8g2.drawUTF8(returnTextWidthPixel("온도 조절을", ALIGN_CENTER), 30, "온도 조절을");
      u8g2.drawUTF8(returnTextWidthPixel("종료합니다.", ALIGN_CENTER), 50, "종료합니다.");
    }
    else if (deviceMode == TEMPERATURE_MAINTANENCE_MODE)
    {
      u8g2.drawUTF8(returnTextWidthPixel("온도 유지를", ALIGN_CENTER), 30, "온도 유지를");
      u8g2.drawUTF8(returnTextWidthPixel("종료합니다.", ALIGN_CENTER), 50, "종료합니다.");
    }
    u8g2.sendBuffer();
    delay(2500);
    deviceMode = STANBY_MODE;
    Trigger = false;
    Trigger_YN = false;
    bootButton = false;
    YN_Check = 0;
  }
  else if ((YN_Check % 2) == 1)
  {
    Trigger = false;
    Trigger_YN = false;
    bootButton = false;
    YN_Check = 0;
  }
}

void FeltierControlFunction(unsigned int CalibrateTemperatureValues)
{
  // 펠티어소자 제어 함수
  if (temperatureC + CalibrateTemperatureValues < userSetTemperature)
  {
    if (ActiveFeltier == COOLER_MODE)
    {
      changeFeltierMode(STOP_MODE);
      delay(50);
    }
    changeFeltierMode(HEATER_MODE);
  }
  else if (temperatureC - CalibrateTemperatureValues > userSetTemperature)
  {
    if (ActiveFeltier == HEATER_MODE)
    {
      changeFeltierMode(STOP_MODE);
      delay(50);
    }
    changeFeltierMode(COOLER_MODE);
  }
}
/*------------------------Button Logic------------------------*/

/*----------setup----------*/
void setup()
{
  Wire.begin(SDA_I2C, SCL_I2C); // I2C 초기화
  /*------pinMode INPUT------*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLDOWN);
  pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(BUTTON_BOOT, INPUT_PULLDOWN);
  

  /*------pinMode OUTPUT------*/
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);
  pinMode(COOLING_PAN, OUTPUT);


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

  /*------Battery설정부------*/
  lipo.begin();
  lipo.wake();

  /*------Interrupt설정부------*/
  // Button 작동 방식 - 3Pin / VCC / GND / OUT / 작동시 OUT 단자에서 High 신호 출력
  attachInterrupt(BUTTON_UP, upButtonF, RISING);
  attachInterrupt(BUTTON_DOWN, downButtonF, RISING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, RISING);

  /*------File System 설정부------*/
  LittleFS.begin(false);    // LittleFS 초기화
  loadUserSetTemperature(); // 설정 온도 불러오기

  /*------PWM설정부------*/
  ledcSetup(PWM_HEATING_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  ledcSetup(PWM_COOLING_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  ledcAttachPin(HEATER_PWM_PIN, PWM_HEATING_CHANNEL);  // PWM 핀과 채널 연결
  ledcAttachPin(COOLER_PWM_PIN, PWM_COOLING_CHANNEL);
}
/*----------setup----------*/

/*----------loop----------*/
void loop()
{
  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static unsigned long AM_count = 0;
  /*Sensors error*/
  if (temperatureC == DEVICE_DISCONNECTED_C) // DEVICE_DISCONNECTED_C -127 오류 값 반환
  {
    u8g2.clearBuffer();
    u8g2.drawUTF8(returnTextWidthPixel("온도 센서 오류", ALIGN_CENTER), 30, "온도 센서 오류");
    u8g2.sendBuffer();
  }

  /*-----온도 측정부-----*/
  if (sensors.isConversionComplete())
  {
    temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures();             // 다음 측정을 위해 온도 요청
  }

  /*Main System control and Display print*/

  /*-----Battery 상태 관리 함수-----*/
  unsigned int CheckBattery = lipo.getSOC();
  if (BatteryPercentage < CheckBattery)
  {
    BatteryChargeStatus = BATTERY_CHARGE;
  }
  else if (BatteryPercentage > CheckBattery)
  {
    BatteryChargeStatus = BATTERY_DISCHARGE;
  }
  if (BatteryPercentage != CheckBattery)
  {
    BatteryPercentage = CheckBattery;
  }

  PushedButtonFunction(); // 버튼 입력 처리 함수

  /*-----Display Low-Energe Mode-----*/
  if (displaySleepTime + 300000 < millis()) // 5분 이상 버튼이 눌리지 않으면 절전모드로 전환
  {
    DisplaySleeping = true;
    u8g2.setPowerSave(1); // 절전모드 설정
  }
  if (displaySleepTime + 300000 > millis()) // 버튼이 눌리면 절전모드 해제
  {
    DisplaySleeping = false;
    u8g2.setPowerSave(0); // 절전모드 해제
  }

  /*-----DisplayPrint and Button-----*/
  u8g2.clearBuffer();
  baseDisplayPrint();
  switch (deviceMode)
  {
  case STANBY_MODE:
    StanbyDisplayPrint();
    changeFeltierMode(STOP_MODE);
    u8g2.sendBuffer();
    break;

  case ACTIVE_MODE:
    ActiveDisplayPrint();
    if (abs(userSetTemperature - temperatureC) < 1)
    {
      if (AM_count == 0)
        AM_count = millis();
      if (millis() - AM_count >= 5000)
      {
        deviceMode = TEMPERATURE_MAINTANENCE_MODE;
        saveUserSetTemperature(userSetTemperature);
        u8g2.clearBuffer();
        u8g2.drawUTF8(returnTextWidthPixel("온도 유지를", ALIGN_CENTER), 30, "온도 유지를");
        u8g2.drawUTF8(returnTextWidthPixel("시작합니다.", ALIGN_CENTER), 50, "시작합니다.");
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

    if (abs(userSetTemperature - temperatureC) >= MAXTEMPDIFF_PWM) // PWM 설정
      dutyCycle = MAXPWM;
    else
    {
      dutyCycle = map(abs(userSetTemperature - temperatureC), 0, MAXTEMPDIFF_PWM, MINPWM, MAXPWM);
    }
    FeltierControlFunction(2); // 온도 값 보정치 2

    if (Trigger == false && DisplaySleeping == false)
    {
      PushButtonTempSetFunction();
    }
    if (Trigger == true)
    {
      u8g2.clearBuffer();
      TriggerEnableFunction(); // Trigger 활성화 - Display에 YES/NO 출력
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
      TriggerEnableFunction(); // Trigger 활성화 - Display에 YES/NO 출력

      if (Trigger_YN == true)
      {
        TriggerYNFunction();
      }
    }
    else
    {
      TMDisplayPrint();
      FeltierControlFunction(2); // 온도 값 보정치 2
    }

    if (abs(userSetTemperature - temperatureC) >= MAXTEMPDIFF_PWM)
      dutyCycle = MAXPWM;
    else
    {
      dutyCycle = map(abs(userSetTemperature - temperatureC), 0, MAXTEMPDIFF_PWM, MINPWM, MAXPWM);
    }

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
}
/*----------loop----------*/

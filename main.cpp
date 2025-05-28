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
  ULT_PIN = 1,      // 오류 핀
  EEP_PIN = 2,      // 전류 제어 핀
  ONE_WIRE_BUS = 3, // DS18B20 센서 핀
  BUTTON_BOOT = 5,  // 모드 변경 버튼
  BUTTON_UP = 6,    // 설정온도 상승 버튼
  BUTTON_DOWN = 7,  // 설정온도 하강 버튼
  SDA_I2C = 8,      // Hardware에서 설정된 I2C핀
  SCL_I2C = 9,      // Hardware에서 설정된 I2C핀
  COOLER_PIN = 20,  // 냉각 제어 핀
  HEATER_PIN = 21   // 가열 제어 핀
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
unsigned int DeviceStatus = 0b000000000000; // 0b Error : 0 | Feltier 00 | Display Sleep : 0 | BootButton : 00 | DownButton / Upbutton 00 | Battery Charged State : 0 | Device mode : 000
enum DeviceStatusbit
{                           // 0b00000 | Error : 0 | Display Sleep : 0 | Feltier 00 | BootButton : 00 | DownButton / Upbutton 00 | Battery Charged State : 0 | Device mode : 000
  ERROR_YN = 11,            // 오류 발생 여부
  DISPLAY_SLEEPING = 8,     // Display 절전 모드 여부
  FELTIER_MODE = 9,         // Feltier 작동 모드
  BOOT_BUTTON = 6,          // Boot 버튼 상태
  DOWN_BUTTON = 5,          // Down 버튼 상태
  UP_BUTTON = 4,            // Up 버튼 상태
  BATTERY_CHARGE_STATUS = 3 // 배터리 충전 상태
};
unsigned char ButtonStatus = 0b000000; // Speed : High LOW | UpButton : 00 | DownButton : 00 | Button Trigger in Trigger Trigger | BootButton : 00
enum ButtonStatusbit
{
  BOOT_BUTTON_TOGGLE = 0,      // Boot 버튼 상태
  BOOT_BUTTON_TRIGGER_2 = 1,   // Boot 버튼 트리거 상태
  DOWN_BUTTON_TOGGLE_LOW = 2,  // Down 버튼 상태
  DOWN_BUTTON_TOGGLE_HIGH = 3, // Down 버튼 상태
  UP_BUTTON_TOGGLE_LOW = 4,    // Up 버튼 상태
  UP_BUTTON_TOGGLE_HIGH = 5    // Up 버튼 상태
};
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

/*---배터리 관리 설정 부---*/
enum BatteryStatus
{                            // 0000| charged state : 0 | 000
  BATTERY_STATUS_FULL = 100, // 배터리 완충
  BATTERY_STATUS_LOW = 20,   // 배터리 부족
  BATTERY_CHARGE = 0 << 4,
  BATTERY_DISCHARGE = 1 << 4,
  VCELL_REG = 0x02,
  SOC_REG = 0x04,
  MODE_REG = 0x06,
  CONFIG_REG = 0x0C,
};
unsigned long BatteryPercentage = 50; // 배터리 량

/*-----열전소자 제어 및 PWM 설정부-----*/
enum FeltierControlPWM
{
  PWM_FREQ = 5000,
  PWM_RESOLUTION = 8,
  PWM_COOLING_CHANNEL = 0,
  PWM_HEATING_CHANNEL = 1,
  FELTIER_STANBY = 0b00,
  FELTIER_HEATING = 0b01,
  FELTIER_COOLING = 0b10,
};
unsigned int dutyCycle = 0; // PWM 값 설정용

/*-----시스템 한계 온도 설정-----*/
enum SystemSettingTemperature
{
  SYSTEM_LIMIT_MAX_TEMPERATURE = 80, // 시스템 한계 온도
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
// 0b 000| Trigger_YN : 0 | Triger : 0 | DownButton | 0 | UpButton : 0 | bootButton 0 | 0000
int YN_Check = 0; // 모드 종료 시 Y / N 선택용 변수

// Toggle 작동 시 시간 확인용 변수
static unsigned long reBootCheckTime = 0;      // 버튼 트리거 시간 변수
static unsigned long upButtonCheckTime = 0;    // upButton Trigger
static unsigned long upButtonStatusTime = 0;   // upButton Trigger
static unsigned long downButtonCheckTime = 0;  // downButton Trigger
static unsigned long downButtonStatusTime = 0; // downButton Trigger

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
  if ((DeviceStatus & (1 << BATTERY_CHARGE_STATUS)) == 0)
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
  else if ((DeviceStatus & (1 << BATTERY_CHARGE_STATUS)) == 1)
  {
    u8g2.setFont(u8g2_font_unifont_h_symbols);
    u8g2.drawUTF8(returnTextWidthPixel("🗲", ALIGN_RIGHT), 12, "🗲"); // 충전 중 표시
    u8g2.setFont(u8g2_font_unifont_t_korean2);
  }
}

/*-----ModeDisplayPrint-----*/
void StanbyDisplayPrint() // 대기 모드 Display 관리 함수
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
  if (((DeviceStatus >> FELTIER_MODE) & 0b11) == FELTIER_COOLING) // 냉각 모드일 때
    u8g2.drawGlyph(returnTextWidthPixel("냉각 중"), 63, 2744);
  if (((DeviceStatus >> FELTIER_MODE) & 0b11) == FELTIER_HEATING) // 가열 모드일 때
    u8g2.drawGlyph(returnTextWidthPixel("가열 중"), 63, 2668);
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 한글 폰트
  u8g2.drawUTF8(0, 30, "온도 조절 중...");   // 현재 온도 출력

  // 애니메이션 효과 - 1초마다 Display에 출력되는 글자 변경
  unsigned int DisplayAnimationPrintWidthFixel = u8g2.getUTF8Width("10℃") + 15; // 애니메이션 효과 시작 위치
  u8g2.setCursor(DisplayAnimationPrintWidthFixel, 47);
  u8g2.print(AM_AnimationCharacter[(millis() / 1000) % 7]);

  // feltier작동모드 출력
  if (((DeviceStatus >> FELTIER_MODE) & 0b11) == FELTIER_COOLING) // 가열 모드일 때
    u8g2.drawUTF8(0, 63, "냉각 중");                              // 냉각 중 출력
  // 2744 or 2746
  if (((DeviceStatus >> FELTIER_MODE) & 0b11) == FELTIER_HEATING) // 냉각 모드일 때
    u8g2.drawUTF8(0, 63, "가열 중");                              // 가열 중 출력
  // 2668 if 2615
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
  u8g2.setFont(u8g2_font_unifont_h_symbols);                                                                                                        // 폰트 설정
  u8g2.print("℃");                                                                                                                                  // 설정 온도 출력
  u8g2.drawUTF8(returnTextWidthPixel("증가:AAA감소:AAA", ALIGN_CENTER) + u8g2.getUTF8Width("증가:"), 38, "▲");                                      // 설정 온도 출력
  u8g2.drawUTF8(returnTextWidthPixel("증가:AAA감소:AAA", ALIGN_CENTER) + u8g2.getUTF8Width("증가:▼▼▼") + u8g2.getUTF8Width("감소:") + 30, 38, "▼"); // 설정 온도 출력

  u8g2.setFont(u8g2_font_unifont_t_korean2);
  u8g2.setCursor(0, 0);               // 커서 위치 설정
  u8g2.drawUTF8(0, 16, "설정온도: "); // 설정 온도 출력
  u8g2.setCursor(u8g2.getUTF8Width("설정온도: "), 16);
  u8g2.print(userSetTemperature);                                                             // 설정 온도 출력
  u8g2.drawUTF8(returnTextWidthPixel("증가:AAA감소:AAA", ALIGN_CENTER), 38, "증가:   감소:"); // 온도 설정 : 전원 버튼 출력
  u8g2.drawUTF8(0, 60, "완료: 전원버튼");                                                     // 폰트 설정
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
    displaySleepTime = millis();         // 버튼이 눌리면 절전모드 해제
    DeviceStatus |= true << DOWN_BUTTON; // 설정온도 하강 버튼 상태 변수
  }
}
void IRAM_ATTR upButtonF() // Up Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeUp > debounceDelay)
  {
    lastDebounceTimeUp = currentTime;
    displaySleepTime = millis();       // 버튼이 눌리면 절전모드 해제
    DeviceStatus |= true << UP_BUTTON; // 설정온도 상승 버튼 상태 변수
  }
}
void IRAM_ATTR bootButtonF() // Boot Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeBoot > debounceDelay)
  {
    lastDebounceTimeBoot = currentTime;
    displaySleepTime = millis();         // 버튼이 눌리면 절전모드 해제
    DeviceStatus |= true << BOOT_BUTTON; // Boot 버튼 상태 변수
  }
}
void IRAM_ATTR DRV8866_error()
{
  DeviceStatus = (DeviceStatus & ~0b111) | OVER_HEATING;
}
/*------------------------Interrupt 함수 정의 부분------------------------------*/

/*------------------------사용자 함수 정의 부분------------------------*/

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

/*------------------------Button Logic Functions------------------------*/
void CheckPushedButtonFunction()
{
  if ((DeviceStatus >> UP_BUTTON) & true)
  {
    ButtonStatus |= true << UP_BUTTON_TOGGLE_LOW;
    DeviceStatus &= ~(!false << UP_BUTTON);
  }
  if ((DeviceStatus >> DOWN_BUTTON) & true)
  {
    ButtonStatus |= true << DOWN_BUTTON_TOGGLE_LOW;
    DeviceStatus &= ~(!false << DOWN_BUTTON);
  }

  if (((DeviceStatus >> BOOT_BUTTON) & true) && ((ButtonStatus >> BOOT_BUTTON_TOGGLE) & true))
  {
    ButtonStatus |= true << BOOT_BUTTON_TRIGGER_2;
    DeviceStatus &= ~(!false << BUTTON_BOOT);
  }
  if ((DeviceStatus >> BOOT_BUTTON) & true)
  {
    ButtonStatus |= true << BOOT_BUTTON_TOGGLE;
    DeviceStatus &= ~(!false << BUTTON_BOOT);
  }
}

void ButtonActiveFunction()
{
  if ((DeviceStatus >> DISPLAY_SLEEPING) & true)
  {
    if (ButtonStatus >> BOOT_BUTTON_TOGGLE & true)
    {
      displaySleepTime = millis();              // 버튼이 눌리면 절전모드 해제
      ButtonStatus &= ~(!false << BOOT_BUTTON_TOGGLE); // Boot 버튼 상태 초기화
    }
    if (ButtonStatus >> UP_BUTTON_TOGGLE_LOW & true)
    {
      displaySleepTime = millis();            // 버튼이 눌리면 절전모드 해제
      ButtonStatus &= ~(!false << UP_BUTTON_TOGGLE_LOW); // Up 버튼 상태 초기화
    }
    if (ButtonStatus >> DOWN_BUTTON_TOGGLE_LOW & true)
    {
      displaySleepTime = millis();              // 버튼이 눌리면 절전모드 해제
      ButtonStatus &= ~(!false << DOWN_BUTTON_TOGGLE_LOW); // Down 버튼 상태 초기화
    }
  }
  if ((ButtonStatus >> BOOT_BUTTON_TOGGLE) & true)
  {
    if (digitalRead(BUTTON_BOOT) == HIGH) // boot 버튼 토글 시 로직 - Deep Sleep 모드를 활용한 재부팅
    {
      if (reBootCheckTime == 0)
      {
        reBootCheckTime = millis();
      }
      if (millis() - reBootCheckTime >= 5000)
      {
        ButtonStatus &= ~(!false << BOOT_BUTTON_TOGGLE);                   // Boot 버튼을 5초 이상 누르면 checkToBootButtonTogle을 false로 설정
        DeviceStatus &= ~(!false << BOOT_BUTTON);                          // bootButton을 false로 설정
        esp_sleep_enable_timer_wakeup(3 * 1000000);                        // 3초 후 Deep Sleep 모드 해제 설정
        u8g2.setPowerSave(1);                                              // Display 절전 모드 진입
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO); // RTC Peripherals 전원 차단
        esp_deep_sleep_start();                                            // ESP32-C3 Deep Sleep 모드로 전환
        reBootCheckTime = 0;
      }
    }
    else if (digitalRead(BUTTON_BOOT) == LOW) // boot 버튼 작동 시 로직
    {
      reBootCheckTime = 0; // Boot버튼을 떼면 reBootCheckTime 초기화
      switch (DeviceStatus & 0b111)
      {
      case STANBY_MODE:
        DeviceStatus &= ~(0b111);
        DeviceStatus |= TEMPERATURE_SETTING_MODE;
        ButtonStatus &= ~(!false << BOOT_BUTTON_TOGGLE);
        break;

      case ACTIVE_MODE:
        if ((ButtonStatus >> BOOT_BUTTON_TOGGLE) & true)
        {
          u8g2.clearBuffer();
          u8g2.drawUTF8(returnTextWidthPixel("온도 조절을", ALIGN_CENTER), 30, "온도 조절을");
          u8g2.drawUTF8(returnTextWidthPixel("종료하시겠습니까?", ALIGN_CENTER), 46, "종료하시겠습니까?");
          CheckPushedButtonFunction();
          if (YN_Check < 0)
          {
            // YN_Check < 0 방지
            YN_Check = 1;
          }
          if ((ButtonStatus >> UP_BUTTON_TOGGLE_LOW) & true)
          {
            YN_Check++;
            ButtonStatus &= ~(!false << UP_BUTTON_TOGGLE_LOW); // Up 버튼 상태 초기화
          }
          if ((ButtonStatus >> DOWN_BUTTON_TOGGLE_LOW) & true)
          {
            YN_Check--;
            ButtonStatus &= ~(!false << DOWN_BUTTON_TOGGLE_LOW); // Down 버튼 상태 초기화
          }

          switch (YN_Check % 2)
          {
          case 0:
            u8g2.drawButtonUTF8(40, 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "YES");
            u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
            u8g2.drawUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ "), 63, "NO");
            if ((ButtonStatus >> BOOT_BUTTON_TRIGGER_2) & true)
            {
              DeviceStatus &= ~(0b111);
              DeviceStatus |= STANBY_MODE;
              ButtonStatus &= ~(!false << BOOT_BUTTON_TRIGGER_2);
              ButtonStatus &= ~(!false << BOOT_BUTTON_TOGGLE);
              u8g2.clearBuffer();
              u8g2.drawUTF8(returnTextWidthPixel("온도 조절을", ALIGN_CENTER), 30, "온도 조절을");
              u8g2.drawUTF8(returnTextWidthPixel("종료합니다.", ALIGN_CENTER), 50, "종료합니다.");
            }
            break;

          case 1:
            u8g2.drawUTF8(25, 63, "YES");
            u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
            u8g2.drawButtonUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ ") + 10, 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "NO");
            if ((ButtonStatus >> BOOT_BUTTON_TRIGGER_2) & true)
            {
              ButtonStatus &= ~(!false << BOOT_BUTTON_TRIGGER_2);
              ButtonStatus &= ~(!false << BOOT_BUTTON_TOGGLE);
            }
          }
          u8g2.sendBuffer();
        }
        else if (((ButtonStatus >> BOOT_BUTTON_TOGGLE) & 1) == false)
        {
          if ((ButtonStatus >> UP_BUTTON_TOGGLE_LOW) & true)
          {
            if (upButtonCheckTime == 0) {
              if (userSetTemperature < SYSTEM_LIMIT_MAX_TEMPERATURE)
                  userSetTemperature++;
              upButtonCheckTime = millis();
            }
            if (millis() - upButtonCheckTime > 1500 && (((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true) == false))
            {
              if (upButtonStatusTime == 0)
                upButtonStatusTime = millis();
              if (millis() - upButtonStatusTime > 1000)
              {
                if (userSetTemperature < SYSTEM_LIMIT_MAX_TEMPERATURE)
                  userSetTemperature++;
                upButtonStatusTime = millis();
              }
              if (millis() - upButtonCheckTime > 4000)
                ButtonStatus &= true << UP_BUTTON_TOGGLE_HIGH;
            }
            if ((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true)
            {
              if (millis() - upButtonStatusTime > 300)
              {
                upButtonStatusTime = millis();
              }
            }
            if (!digitalRead(GPIO_PIN::BUTTON_UP))
            {
              ButtonStatus &= ~(!false << UP_BUTTON_TOGGLE_LOW);
              upButtonCheckTime = 0;
              upButtonStatusTime = 0;
              if (((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true) == false)
                if (userSetTemperature < SYSTEM_LIMIT_MAX_TEMPERATURE)
                  userSetTemperature++;
                else
                  ButtonStatus &= ~(!false << UP_BUTTON_TOGGLE_HIGH);
            }
          }

          if (ButtonStatus >> DOWN_BUTTON_TOGGLE_LOW)
          {
            if (downButtonCheckTime == 0)
              upButtonCheckTime = millis();
            if (millis() - downButtonCheckTime > 1500 && (((ButtonStatus >> DOWN_BUTTON_TOGGLE_HIGH) & true) == false))
            {
              if (downButtonCheckTime == 0)
                downButtonCheckTime = millis();
              if (millis() - downButtonStatusTime > 1000)
              {
                if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
                  userSetTemperature--;
                downButtonStatusTime = millis();
              }
              if (millis() - upButtonCheckTime > 4000)
                ButtonStatus &= true << DOWN_BUTTON_TOGGLE_HIGH;
            }
            if ((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true)
            {
              if (millis() - downButtonStatusTime > 300)
              {
                if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
                  userSetTemperature--;
                downButtonStatusTime = millis();
              }
            }
            if (!digitalRead(GPIO_PIN::BUTTON_DOWN))
            {
              ButtonStatus &= ~(!false << DOWN_BUTTON_TOGGLE_LOW);
              downButtonCheckTime = 0;
              downButtonStatusTime = 0;
              if (((ButtonStatus >> DOWN_BUTTON_TOGGLE_HIGH) & true) == false)
                if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
                  userSetTemperature--;
                else
                  ButtonStatus &= ~(!false << DOWN_BUTTON_TOGGLE_HIGH);
            }
          }
          break;

        case TEMPERATURE_MAINTANENCE_MODE:
          if ((ButtonStatus >> BOOT_BUTTON_TOGGLE) & true)
          {
            u8g2.clearBuffer();
            u8g2.drawUTF8(returnTextWidthPixel("온도 유지를", ALIGN_CENTER), 30, "온도 유지를");
            u8g2.drawUTF8(returnTextWidthPixel("종료하시겠습니까?", ALIGN_CENTER), 46, "종료하시겠습니까?");
            CheckPushedButtonFunction();
            if (YN_Check < 0)
            {
              // YN_Check < 0 방지
              YN_Check = 1;
            }
            if ((ButtonStatus >> UP_BUTTON_TOGGLE_LOW) & true)
            {
              YN_Check++;
              ButtonStatus &= ~(!false << UP_BUTTON_TOGGLE_LOW); // Up 버튼 상태 초기화
            }
            if ((ButtonStatus >> DOWN_BUTTON_TOGGLE_LOW) & true)
            {
              YN_Check--;
              ButtonStatus &= ~(!false << DOWN_BUTTON_TOGGLE_LOW); // Down 버튼 상태 초기화
            }

            switch (YN_Check % 2)
            {
            case 0:
              u8g2.drawButtonUTF8(40, 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "YES");
              u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
              u8g2.drawUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ "), 63, "NO");
              if ((ButtonStatus >> BOOT_BUTTON_TRIGGER_2) & true)
              {
                DeviceStatus &= ~(0b111);
                DeviceStatus |= STANBY_MODE;
                ButtonStatus &= ~(!false << BOOT_BUTTON_TRIGGER_2);
                ButtonStatus &= ~(!false << BOOT_BUTTON_TOGGLE);
                u8g2.clearBuffer();
                u8g2.drawUTF8(returnTextWidthPixel("온도 유지를", ALIGN_CENTER), 30, "온도 유지를");
                u8g2.drawUTF8(returnTextWidthPixel("종료합니다.", ALIGN_CENTER), 50, "종료합니다.");
              }
              break;

            case 1:
              u8g2.drawUTF8(25, 63, "YES");
              u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
              u8g2.drawButtonUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ ") + 10, 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "NO");
              if ((ButtonStatus >> BOOT_BUTTON_TRIGGER_2) & true)
              {
                ButtonStatus &= ~(!false << BOOT_BUTTON_TRIGGER_2);
                ButtonStatus &= ~(!false << BOOT_BUTTON_TOGGLE);
              }
            }
            u8g2.sendBuffer();
            break;

          case TEMPERATURE_SETTING_MODE:
            if (ButtonStatus >> UP_BUTTON_TOGGLE_LOW)
            {
              if (upButtonCheckTime == 0)
                upButtonCheckTime = millis();
              if (millis() - upButtonCheckTime > 1500 && (((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true) == false))
              {
                if (upButtonStatusTime == 0)
                  upButtonStatusTime = millis();
                if (millis() - upButtonStatusTime > 1000)
                {
                  if (userSetTemperature < SYSTEM_LIMIT_MAX_TEMPERATURE)
                    userSetTemperature++;
                  upButtonStatusTime = millis();
                }
                if (millis() - upButtonCheckTime > 4000)
                  ButtonStatus &= true << UP_BUTTON_TOGGLE_HIGH;
              }
              if ((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true)
              {
                if (millis() - upButtonStatusTime > 300)
                {
                  if (userSetTemperature < SYSTEM_LIMIT_MAX_TEMPERATURE)
                    userSetTemperature++;
                  upButtonStatusTime = millis();
                }
              }
              if (!digitalRead(GPIO_PIN::BUTTON_UP))
              {
                ButtonStatus &= ~(!false << UP_BUTTON_TOGGLE_LOW);
                upButtonCheckTime = 0;
                upButtonStatusTime = 0;
                if (((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true) == false)
                  if (userSetTemperature < SYSTEM_LIMIT_MAX_TEMPERATURE)
                    userSetTemperature++;
                  else
                    ButtonStatus &= ~(!false << UP_BUTTON_TOGGLE_HIGH);
              }
            }

            if (ButtonStatus >> DOWN_BUTTON_TOGGLE_LOW)
            {
              if (downButtonCheckTime == 0)
                upButtonCheckTime = millis();
              if (millis() - downButtonCheckTime > 1500 && (((ButtonStatus >> DOWN_BUTTON_TOGGLE_HIGH) & true) == false))
              {
                if (downButtonCheckTime == 0)
                  downButtonCheckTime = millis();
                if (millis() - downButtonStatusTime > 1000)
                {
                  if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
                    userSetTemperature--;
                  downButtonStatusTime = millis();
                }
                if (millis() - upButtonCheckTime > 4000)
                  ButtonStatus &= true << DOWN_BUTTON_TOGGLE_HIGH;
              }
              if ((ButtonStatus >> UP_BUTTON_TOGGLE_HIGH) & true)
              {
                if (millis() - downButtonStatusTime > 300)
                {
                  if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
                    userSetTemperature--;
                  downButtonStatusTime = millis();
                }
              }
              if (!digitalRead(GPIO_PIN::BUTTON_DOWN))
              {
                ButtonStatus &= ~(!false << DOWN_BUTTON_TOGGLE_LOW);
                downButtonCheckTime = 0;
                downButtonStatusTime = 0;
                if (((ButtonStatus >> DOWN_BUTTON_TOGGLE_HIGH) & true) == false)
                  if (userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
                    userSetTemperature--;
                  else
                    ButtonStatus &= ~(!false << DOWN_BUTTON_TOGGLE_HIGH);
              }
            }

            if ((ButtonStatus >> BOOT_BUTTON_TOGGLE) & true) {
              u8g2.clearBuffer();
              endedSettingTemperatureDisplayPrint();
              if(abs(userSetTemperature) )

            }
            break;
          }
        }
      }
    }
  }
}
// 설정 온도 증가 / 감소 버튼 함수
/*------------------------Button Logic Functions------------------------*/

/*------------------------DRV8833 Control Functions------------------------*/
void SetupPWM(unsigned int Pin, unsigned int Channel, unsigned int Frequency, unsigned int Resolution)
{
  pinMode(Pin, OUTPUT); // PWM 핀 설정
  ledcSetup(Channel, Frequency, Resolution);
  ledcAttachPin(Pin, Channel);
}

void SetControlFeltier(unsigned int DutyCycle = 0, FeltierControlPWM Mode = FELTIER_STANBY)
{
  switch (Mode)
  {
  case FELTIER_STANBY:
    ledcWrite(PWM_HEATING_CHANNEL, 0);
    ledcWrite(PWM_COOLING_CHANNEL, 0);
    break;

  case FELTIER_HEATING:
    ledcWrite(PWM_HEATING_CHANNEL, DutyCycle);
    ledcWrite(PWM_COOLING_CHANNEL, 0);
    break;

  case FELTIER_COOLING:
    ledcWrite(PWM_HEATING_CHANNEL, 0);
    ledcWrite(PWM_COOLING_CHANNEL, DutyCycle);
    break;
  }
}

/*----------setup----------*/
void setup()
{
  Wire.begin(SDA_I2C, SCL_I2C); // I2C 초기화
  /*------pinMode INPUT------*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLDOWN);
  pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(BUTTON_BOOT, INPUT_PULLDOWN);
  pinMode(ULT_PIN, INPUT_PULLUP);

  /*------pinMode OUTPUT------*/
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);
  pinMode(EEP_PIN, OUTPUT);
  digitalWrite(EEP_PIN, HIGH);

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
  attachInterrupt(ULT_PIN, DRV8866_error, FALLING);

  /*------File System 설정부------*/
  LittleFS.begin(false);    // LittleFS 초기화
  loadUserSetTemperature(); // 설정 온도 불러오기

  /*------PWM설정부------*/
  SetupPWM(COOLER_PIN, PWM_COOLING_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  SetupPWM(HEATER_PIN, PWM_HEATING_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
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
    sensors.begin();                     // DS18B20 센서 초기화
    sensors.setWaitForConversion(false); // 비동기식으로 온도 측정
    sensors.requestTemperatures();       // 온도 측정 요청
    delay(2000);                         // 1초 대기
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
    DeviceStatus &= ~(1 << BATTERY_CHARGE_STATUS); // 배터리 충전 상태 충전 중이 아닐 때
  }
  else if (BatteryPercentage > CheckBattery)
  {
    DeviceStatus |= (1 << BATTERY_CHARGE_STATUS); // 배터리 충전 상태 충전 시
  }
  if (BatteryPercentage != CheckBattery)
  {
    BatteryPercentage = CheckBattery;
  }

  /*-----Display Low-Energe Mode-----*/
  if (displaySleepTime + 300000 < millis()) // 5분 이상 버튼이 눌리지 않으면 절전모드로 전환
  {
    DeviceStatus |= (1 << DISPLAY_SLEEPING); // 절전모드 활성화
    u8g2.setPowerSave(1);                    // 절전모드 설정
  }
  if (displaySleepTime + 300000 > millis()) // 버튼이 눌리면 절전모드 해제
  {
    DeviceStatus &= ~(1 << DISPLAY_SLEEPING); // 절전모드 비활성화
    u8g2.setPowerSave(0);                     // 절전모드 해제
  }

  /*-----DisplayPrint and Button-----*/
  u8g2.clearBuffer();
  baseDisplayPrint();
  switch (DeviceStatus & 0b111)
  {
  case STANBY_MODE:
    StanbyDisplayPrint();
    CheckPushedButtonFunction();
      ButtonActiveFunction();
    u8g2.sendBuffer();
    break;

  case ACTIVE_MODE:
    ActiveDisplayPrint();
    if (abs(userSetTemperature - temperatureC) < 1) // 온도 1도 이내로 유지 시 온도 유지모드 진입
    {
      if (AM_count == 0)
        AM_count = millis();
      if (millis() - AM_count >= 5000)
      {
        DeviceStatus = (DeviceStatus & ~0b111) | TEMPERATURE_MAINTANENCE_MODE; // 현재 모드를 유지 모드로 변경
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
      if (abs(userSetTemperature - temperatureC) >= MAXTEMPDIFF_PWM) // PWM 설정
        dutyCycle = MAXPWM;
      else
        dutyCycle = map(abs(userSetTemperature - temperatureC), 0, MAXTEMPDIFF_PWM, MINPWM, MAXPWM);

      if (userSetTemperature - temperatureC > 2) // 목표 온도가 현재보다 2도 이상 높을 때 (가열 필요)
        SetControlFeltier(dutyCycle, FELTIER_HEATING);
      else if (userSetTemperature - temperatureC < -2) // 목표 온도가 현재보다 2도 이상 낮을 때 (냉각 필요)
        SetControlFeltier(dutyCycle, FELTIER_COOLING);

      CheckPushedButtonFunction();
      ButtonActiveFunction();
    }

  break;

case TEMPERATURE_MAINTANENCE_MODE:

  TMDisplayPrint();
  if (abs(userSetTemperature - temperatureC) >= MAXTEMPDIFF_PWM)
    dutyCycle = MAXPWM;
  else
  {
    dutyCycle = map(abs(userSetTemperature - temperatureC), 0, MAXTEMPDIFF_PWM, MINPWM, MAXPWM);
  }
  if (userSetTemperature - temperatureC > 1)
  { // 현재 온도가 설정 온도보다 deadband 이상 낮으면 가열
    SetControlFeltier(dutyCycle, FELTIER_HEATING);
  }
  else if (userSetTemperature - temperatureC < -1)
  { // 현재 온도가 설정 온도보다 deadband 이상 높으면 냉각
    SetControlFeltier(dutyCycle, FELTIER_COOLING);
  }
  else
  { // 데드밴드 내에서는 대기
    SetControlFeltier(0, FELTIER_STANBY);
  }
  CheckPushedButtonFunction();
  ButtonActiveFunction();
  u8g2.sendBuffer();
  break;

case TEMPERATURE_SETTING_MODE:
  settingTemperatureDisplayPrint();
  CheckPushedButtonFunction();
  ButtonActiveFunction();
  u8g2.sendBuffer();
  break;

case BOOTING_MODE:
  u8g2.clearBuffer();
  startingDisplayPrint();
  u8g2.sendBuffer();
  delay(3000);
  DeviceStatus = (DeviceStatus & ~0b111) | STANBY_MODE; // 부팅 모드 종료 후 대기 모드로 전환
  break;

case OVER_HEATING:
  u8g2.clearBuffer();
  u8g2.drawUTF8(0, 40, "System Error");

  u8g2.sendBuffer();
  break;
}
}
/*----------loop----------*/

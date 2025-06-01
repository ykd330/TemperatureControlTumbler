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
  COOLER_PIN = 20, // 냉각 제어 핀
  HEATER_PIN = 21  // 가열 제어 핀
};
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); // u8g2 객체 선언
SFE_MAX1704X lipo;                                                // MAX1704x객체 선언
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Context *context = new Context(u8g2, sensors, lipo, oneWire); // Context 객체 생성
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

/*-----시스템 한계 온도 설정-----*/
enum SystemSettingTemperature
{
  SYSYEM_LIMIT_MAX_TEMPERATURE = 80, // 시스템 한계 온도
  SYSTEM_LIMIT_MIN_TEMPERATURE = 5,  // 시스템 한계 온도
  MAXTEMPDIFF_PWM = 10,
  MAXPWM = 255,
  MINPWM = 60
};
int RTC_DATA_ATTR userSetTemperature; // 설정 온도 저장 변수
/*-----Display함수용 변수-----*/
enum checkreturnPixelMode
{
  WIDTH_TEXT = 0,
  ALIGN_CENTER = 1,
  ALIGN_RIGHT = 2,
};

class Context;
class StanbyState;
class ActiveState;
class TemperatureMaintanenceState;
class TemperatureSettingState;
class BootingState;

class State
{
public:
  virtual ~State() {} // 가상 소멸자 (상속 관계에서 중요)

  // 상태 진입 시 호출될 함수
  virtual void onEnter(Context &context) = 0;
  // 상태에 머무르는 동안 주기적으로 호출될 함수 (기존 loop의 case 내용)
  virtual void loop(Context &context) = 0;
  // 상태 종료 시 호출될 함수
  virtual void onExit(Context &context) = 0;

  // 각 버튼 입력 처리 함수
  virtual void handleBootButton(Context &context) = 0;
  virtual void handleUpButton(Context &context) = 0;
  virtual void handleDownButton(Context &context) = 0;
};

// --- BootingState 구현 ---
class BootingState : public State
{
private:
  unsigned long entryTime;

public:
  void onEnter(Context &context) override
  {
    context.clearDisplay();
    startingDisplayPrint(); // 기존 전역 함수 사용
    context.sendDisplayBuffer();
    entryTime = millis();
  }

  void loop(Context &context) override
  {
    if (millis() - entryTime > 3000)
    { // 3초 후 Stanby 모드로 전환
      // 다음 상태(StandbyState) 객체를 생성하여 전달해야 함
      // context.setState(new StandbyState()); // StandbyState 정의 후 활성화
    }
  }

  void onExit(Context &context) override
  {
    // 특별히 할 일 없음
  }

  void handleBootButton(Context &context) override { /* 부팅 중 버튼 무시 */ }
  void handleUpButton(Context &context) override { /* 부팅 중 버튼 무시 */ }
  void handleDownButton(Context &context) override { /* 부팅 중 버튼 무시 */ }
};

// --- StandbyState 구현 ---
class StandbyState : public State
{
public:
  void onEnter(Context &context) override
  {
    context.changeFeltier(STOP_MODE); // 펠티어 정지
  }

  void loop(Context &context) override
  {
    context.clearDisplay();
    context.drawBaseDisplay(); // 배터리 등 공통 UI
    StanbyDisplayPrint();      // 기존 대기화면 함수 (Context의 u8g2 객체 사용하도록 수정 필요)
    context.sendDisplayBuffer();
  }

  void handleBootButton(Context &context) override
  {
    // BOOT 버튼 누르면 TEMPERATURE_SETTING_MODE로 전환
    context.setState(new TemperatureSettingState());
  }
};

// --- TemperatureSettingState 구현 ---
// 기존 PushButtonTempSetFunction, settingTemperatureDisplayPrint,
// ButtonTriggerEnableFunction (일부) 로직 포함
class TemperatureSettingState : public State
{
private:
  unsigned long upButtonToggleTime;
  unsigned long upButtonCheckTime;
  bool upButtonLowRepeatToggle;
  bool upButtonHighRepeatToggle;
  unsigned long downButtonToggleTime;
  unsigned long downButtonCheckTime;
  bool downButtonLowRepeatToggle;
  bool downButtonHighRepeatToggle;


public:
  TemperatureSettingState() : upButtonToggleTime(0), upButtonCheckTime(0), upButtonLowRepeatToggle(false), upButtonHighRepeatToggle(false),
                              downButtonToggleTime(0), downButtonCheckTime(0), downButtonLowRepeatToggle(false), downButtonHighRepeatToggle(false)
  {

  }

  void onEnter(Context &context) override
  {
    // 변수 초기화
    upButtonToggleTime = 0;
    upButtonCheckTime = 0;
    upButtonLowRepeatToggle = false;
    upButtonHighRepeatToggle = false;
    unsigned long downButtonToggleTime = 0;
    unsigned long downButtonCheckTime = 0;
    bool downButtonLowRepeatToggle = false;
    bool downButtonHighRepeatToggle = false;
  }

  void loop(Context &context) override
  {
    // 온도 증감 로직 (기존 PushButtonTempSetFunction의 버튼 상태 확인 부분을 제외한 로직)
    // digitalRead(BUTTON_UP) 등은 handleUpButton으로 이동
    // 여기서는 단순히 현재 설정온도와 UI를 표시
    context.clearDisplay();
    context.drawBaseDisplay();
    ::settingTemperatureDisplayPrint();
    context.Display.setFont(u8g2_font_unifont_t_korean2);
    context.Display.drawUTF8(0, 16, "설정온도: ");
    context.Display.print(context.userSetTemperature);
    context.Display.print("℃");
    context.sendDisplayBuffer();

    if(digitalRead(BUTTON_UP))
    {
      // 버튼이 눌렸을 때
      if (millis() - upButtonToggleTime > 1000 && !upButtonHighRepeatToggle) // 0.5초 이상 눌렸을 때
      {
        upButtonCheckTime = millis(); // 버튼 눌림 시간 기록
        if (millis() - upButtonCheckTime > 150){
          context.userSetTemperature++; // 설정 온도 증가 처리
        }
        if(millis() - upButtonToggleTime > 2500) {
          upButtonHighRepeatToggle = true;
        }
      }
      else if(upButtonHighRepeatToggle) {
        // 버튼이 계속 눌려있는 동안
        upButtonCheckTime = millis(); // 버튼 눌림 시간 기록
        if (millis() - upButtonCheckTime > 75) // 1초 이상 눌렸을 때
        {
          context.userSetTemperature++; // 설정 온도 증가 처리
          upButtonCheckTime = millis(); // 시간 갱신
        }
      }
    }
    else if (digitalRead(BUTTON_UP) == LOW)
    {
      upButtonLowRepeatToggle = false; // 버튼이 떼어졌을 때
      upButtonHighRepeatToggle = false; // 버튼이 떼어졌을 때
      upButtonCheckTime = 0; // 버튼 눌림 시간 초기화
      upButtonToggleTime = 0; // 버튼 떼어짐 시간 기록
    }

    if (digitalRead(BUTTON_DOWN))
    {
      // 버튼이 눌렸을 때
      if (millis() - downButtonToggleTime > 1000 && !downButtonHighRepeatToggle) // 0.5초 이상 눌렸을 때
      {
        downButtonCheckTime = millis(); // 버튼 눌림 시간 기록
        if (millis() - downButtonCheckTime > 150){
          context.userSetTemperature--; // 설정 온도 감소 처리
        }
        if(millis() - downButtonToggleTime > 2500) {
          downButtonHighRepeatToggle = true;
        }
      }
      else if(downButtonHighRepeatToggle) {
        // 버튼이 계속 눌려있는 동안
        downButtonCheckTime = millis(); // 버튼 눌림 시간 기록
        if (millis() - downButtonCheckTime > 75) // 1초 이상 눌렸을 때
        {
          context.userSetTemperature--; // 설정 온도 감소 처리
          downButtonCheckTime = millis(); // 시간 갱신
        }
      }
    }
    else if (digitalRead(BUTTON_DOWN) == LOW)
    {
      downButtonLowRepeatToggle = false; // 버튼이 떼어졌을 때
      downButtonHighRepeatToggle = false; // 버튼이 떼어졌을 때
      downButtonCheckTime = 0; // 버튼 눌림 시간 초기화
      downButtonToggleTime = 0; // 버튼 떼어짐 시간 기록
    }
  }

  void onExit(Context &context) override
  {
    context.saveTemperatureSetting(); // 설정 온도 저장
    context.clearDisplay();
    ::endedSettingTemperatureDisplayPrint(); // "온도조절을 완료합니다" 등 표시
    context.sendDisplayBuffer();
    delay(3000); // 3초 대기 후 상태 전환
  }

  void handleBootButton(Context &context) override
  {
    // 설정 완료. ACTIVE 또는 MAINTANENCE 모드로 전환
    unsigned long startTime = millis();

    if (abs(context.currentTemperatureC() - context.userSetTemperature) > 0.5)
    {
      context.setState(new ActiveState()); // 온도 차이가 있으면 ACTIVE 모드로 전환
    }
    else
    {
      context.setState(new TemperatureMaintenanceState());
    }
  }

  void handleUpButton(Context &context) override
  {
    // 기존 PushButtonTempSetFunction의 BUTTON_UP 부분 로직
    // userSetTemperature 증가 로직
    if (context.userSetTemperature < SYSYEM_LIMIT_MAX_TEMPERATURE)
    {
      context.userSetTemperature++;
    }
    upButtonToggleTime = millis(); // 버튼 눌림 시간 기록
    upButtonLowRepeatToggle = true; // 버튼이 눌렸음을 표시
    // 여기서는 단순 1회 증가만 처리하거나, 토글 변수 설정
    // upButtonLowRepeatToggle, upButtonHighRepeatToggle 상태를 여기서 변경하지 않고
    // digitalRead()로 버튼 상태를 직접 확인하는 부분을 이 함수로 옮겨와야 함.
    // 더 나은 방법: 이 함수에서는 "버튼이 눌렸다"는 사실만 처리하고,
    // loop()에서 digitalRead(BUTTON_UP) == HIGH 인 동안의 연속 증가 로직을 수행
  }

  void handleDownButton(Context &context) override
  {
    // userSetTemperature 감소 로직
    if (context.userSetTemperature > SYSTEM_LIMIT_MIN_TEMPERATURE)
    {
      context.userSetTemperature--;
    }
  }
};

class ActiveState : public State
{
public:
  void onEnter(Context &context) override
  {
    context.changeFeltier(context.activeFeltierMode); // 현재 모드에 맞는 펠티어 제어
  }

  void loop(Context &context) override
  {
    context.clearDisplay();
    context.drawBaseDisplay();
    ::ActiveDisplayPrint(); // 활성화 상태 화면 출력 함수
    context.sendDisplayBuffer();

    // 온도 유지 로직 (온도 차이에 따라 펠티어 제어)
    if (abs(context.currentTemperatureC() - context.userSetTemperature) > 0.5)
    {
      if (context.currentTemperatureC() < context.userSetTemperature)
      {
        context.changeFeltier(HEATER_MODE);
      }
      else
      {
        context.changeFeltier(COOLER_MODE);
      }
    }
    else
    {
      context.changeFeltier(STOP_MODE);
    }
  }

  void onExit(Context &context) override
  {
    context.clearDisplay();
    u8g2.drawUTF8(returnTextWidthPixel("온도 조절을", ALIGN_CENTER), 30, "온도 조절을");
    u8g2.drawUTF8(returnTextWidthPixel("종료합니다.", ALIGN_CENTER), 50, "종료합니다.");
    context.sendDisplayBuffer();
    delay(2000); // 2초 대기 후 상태 전환
  }

  void handleBootButton(Context &context) override
  {
    // BOOT 버튼 누르면 TEMPERATURE_SETTING_MODE로 전환
    context.setState(new TemperatureSettingState());
  }

  void handleUpButton(Context &context) override { /* UP 버튼 처리 */ }
  void handleDownButton(Context &context) override { /* DOWN 버튼 처리 */ }
};

class TemperatureMaintenanceState : public State
{
public:
  void onEnter(Context &context) override
  {
    context.changeFeltier(STOP_MODE); // 펠티어 정지
  }

  void loop(Context &context) override
  {
    context.clearDisplay();
    context.drawBaseDisplay();
    ::TMDisplayPrint(); // 유지 모드 화면 출력 함수
    context.sendDisplayBuffer();

    // 온도 유지 로직 (온도 차이에 따라 펠티어 제어)
    if (abs(context.currentTemperatureC() - context.userSetTemperature) > 0.5)
    {
      if (context.currentTemperatureC() < context.userSetTemperature)
      {
        context.changeFeltier(HEATER_MODE);
      }
      else
      {
        context.changeFeltier(COOLER_MODE);
      }
    }
    else
    {
      context.changeFeltier(STOP_MODE);
    }
  }

  void onExit(Context &context) override
  {
    context.clearDisplay();
    u8g2.drawUTF8(returnTextWidthPixel("온도 유지를", ALIGN_CENTER), 30, "온도 유지를");
    u8g2.drawUTF8(returnTextWidthPixel("종료합니다.", ALIGN_CENTER), 50, "종료합니다.");
    context.sendDisplayBuffer();
    delay(2000); // 2초 대기 후 상태 전환
  }

  void handleBootButton(Context &context) override
  {
    // BOOT 버튼 누르면 TEMPERATURE_SETTING_MODE로 전환
    context.setState(new TemperatureSettingState());
  }

  void handleUpButton(Context &context) override { /* UP 버튼 처리 */ }
  void handleDownButton(Context &context) override { /* DOWN 버튼 처리 */ }
};

class Context
{
private:
  State *currentState;

public:
  /*-----Module Setting-----*/
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C &Display; // 참조로 전달받음
  DallasTemperature &Sensors;
  SFE_MAX1704X &batterySensor; // MAX1704x객체 선언
  /*-----Temperature Sensor Setting-----*/
  int temperatureC = 0;                 // 현재 온도 저장 변수
  int RTC_DATA_ATTR userSetTemperature; // 설정 온도 저장 변수
  ControlMode activeFeltierMode;
  unsigned int currentDutyCycle;
  volatile bool bootButtonPressed;
  volatile bool upButtonPressed;
  volatile bool downButtonPressed;
  unsigned long displaySleepTimer;
  bool isDisplaySleeping;

public:
  Context(U8G2_SSD1306_128X64_NONAME_F_HW_I2C &u8g2, DallasTemperature &dallas, SFE_MAX1704X &lipo, OneWire &OneWire) : Display(u8g2), Sensors(dallas),
                                                                                                                        batterySensor(lipo), temperatureC(0), userSetTemperature(50), /* 초기값 */
                                                                                                                        activeFeltierMode(STOP_MODE), currentDutyCycle(0),
                                                                                                                        bootButtonPressed(false), upButtonPressed(false), downButtonPressed(false),
                                                                                                                        displaySleepTimer(0), isDisplaySleeping(false)
  {
  }

  ~Context()
  {
    delete currentState;
  }

  void setState(State *newState)
  {
    if (currentState)
    {
      currentState->onExit(this); // 현재 상태의 Exit 액션 호출
      delete currentState;        // 이전 상태 삭제
    }
    currentState = newState;
    currentState->onEnter(this); // 새로운 상태의 Entry 액션 호출
  }
  
  State *getCurrentState() { return currentState; }
  void changeFeltier(ControlMode mode)
  {

    ::changeFeltierMode(mode); // 전역 함수를 호출하거나,
  }
  void saveTemperatureSetting()
  {
    ::saveUserSetTemperature(userSetTemperature); // 전역 함수 호출
  }

  void loadTemperatureSetting()
  {
    ::loadUserSetTemperature();                    // 전역 함수 호출 후 멤버 변수 userSetTemperature에 반영
    this->userSetTemperature = userSetTemperature; // 전역 변수 값을 가져옴 (주의: RTC_DATA_ATTR)
  }
  // --- 버튼 플래그 처리 ---
  void processButtonInputs()
  {
    if (bootButtonPressed)
    {
      if (currentState && !isDisplaySleeping)
        currentState->handleBootButton(*this);
      displaySleepTimer = millis(); // 절전모드 해제용
      isDisplaySleeping = false;    // 절전모드 해제
      Display.setPowerSave(0);
      bootButtonPressed = false; // 플래그 리셋
    }
    if (upButtonPressed)
    {
      if (currentState && !isDisplaySleeping)
        currentState->handleUpButton(*this);
      displaySleepTimer = millis();
      isDisplaySleeping = false;
      Display.setPowerSave(0);
      upButtonPressed = false;
    }
    if (downButtonPressed)
    {
      if (currentState && !isDisplaySleeping)
        currentState->handleDownButton(*this);
      displaySleepTimer = millis();
      isDisplaySleeping = false;
      Display.setPowerSave(0);
      downButtonPressed = false;
    }
  }

  void update()
  {
    // 온도 측정
    if (Sensors.isConversionComplete())
    {
      temperatureC = Sensors.getTempCByIndex(0);
      Sensors.requestTemperatures();
    }
    // 배터리 상태 관리 (기존 로직)
    // ...

    // 디스플레이 절전 관리
    if (!isDisplaySleeping && (millis() - displaySleepTimer > 300000))
    { // 5분
      isDisplaySleeping = true;
      Display.setPowerSave(1);
    }

    if (currentState)
    {
      currentState->loop(*this); // 현재 상태의 주기적 작업 수행
    }
  }
  int currentTemperatureC()
  {
    if (Sensors.isConversionComplete())
    {
      temperatureC = Sensors.getTempCByIndex(0);
      Sensors.requestTemperatures();
    }
    return temperatureC;
  } // 현재 온도 반환 함수
  
  void clearDisplay() { Display.clearBuffer(); }
  void sendDisplayBuffer() { Display.sendBuffer(); }
  void drawBaseDisplay() { ::baseDisplayPrint(); } // 기존 전역 함수 활용 또는 멤버화
};
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

void settingTemperatureDisplayPrint() // 온도 설정 Display 관리 함수
{                                     // 온도 설정 : 전원 버튼 출력
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
volatile long lastDebounceTimeDown = 0; // 마지막 디바운스 시간 변수
volatile long lastDebounceTimeUp = 0;   // 마지막 디바운스 시간 변수
volatile long lastDebounceTimeBoot = 0; // 마지막 디바운스 시간 변수
volatile unsigned long debounceDelay = 50; // 디바운스 딜레이 시간
/*------------------------Interrupt 함수 정의 부분------------------------------*/
void IRAM_ATTR downButtonF() // Down Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeDown > debounceDelay)
  {
    lastDebounceTimeDown = currentTime;
    context->downButtonPressed = true;           // 설정온도 하강 버튼 상태 변수
  }
}
void IRAM_ATTR upButtonF() // Up Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeUp > debounceDelay)
  {
    lastDebounceTimeUp = currentTime;
    context->upButtonPressed = true;             // 설정온도 상승 버튼 상태 변수
  }
}
void IRAM_ATTR bootButtonF() // Boot Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeBoot > debounceDelay)
  {
    lastDebounceTimeBoot = currentTime;
    context->bootButtonPressed = true;           // 모드 변경 버튼 상태 변수
    context->displaySleepTimer = millis();       // 절전모드 해제용
    context->isDisplaySleeping = false;          // 절전모드 해제
    context->Display.setPowerSave(0);            // 디스플레이 절전 모드 해제
  }
}
/*------------------------Interrupt 함수 정의 부분------------------------------*/

/*------------------------사용자 함수 정의 부분------------------------*/

/*------시스템 내부 feltier 제어 결정 함수------*/
void changeFeltierMode(ControlMode control_device_mode) // 열전소자 제어 함수
{
  if (control_device_mode == HEATER_MODE)
  {
    digitalWrite(HEATER_PIN, HIGH); // 가열 제어 핀 HIGH
    digitalWrite(COOLER_PIN, LOW);  // 냉각 제어 핀 LOW
    digitalWrite(COOLING_PAN, HIGH);
    ledcWrite(PWM_HEATING_CHANNEL, context->currentDutyCycle); // 가열 PWM
    ledcWrite(PWM_COOLING_CHANNEL, 0);
  }
  else if (control_device_mode == COOLER_MODE)
  {
    digitalWrite(HEATER_PIN, LOW);  // 가열 제어 핀 LOW
    digitalWrite(COOLER_PIN, HIGH); // 냉각 제어 핀 HIGH
    digitalWrite(COOLING_PAN, HIGH);
    ledcWrite(PWM_COOLING_CHANNEL, context->currentDutyCycle); // 냉각 PWM
    ledcWrite(PWM_HEATING_CHANNEL, 0);
  }
  else if (control_device_mode == STOP_MODE)
  {
    digitalWrite(HEATER_PIN, LOW); // 가열 제어 핀 LOW
    digitalWrite(COOLER_PIN, LOW); // 냉각 제어 핀 LOW
    digitalWrite(COOLING_PAN, LOW);
    ledcWrite(PWM_HEATING_CHANNEL, 0);
    ledcWrite(PWM_COOLING_CHANNEL, 0);
  }
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

void setup()
{
  // 기존 하드웨어 초기화 (Wire.begin, pinMode 등)
  Wire.begin(SDA_I2C, SCL_I2C);
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  // ... (모든 pinMode 설정)

  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setPowerSave(0);
  // ... (u8g2 설정)

  lipo.begin();
  lipo.wake();

  

  // 설정 온도 불러오기 (Context를 통해)
  context->loadTemperatureSetting(); // 내부적으로 전역 userSetTemperature에 로드 후 멤버 변수로 가져옴

  // 인터럽트 설정 (Context의 플래그를 사용하도록 ISR 수정 필요)
  attachInterrupt(BUTTON_UP, upButtonF, RISING);
  attachInterrupt(BUTTON_DOWN, downButtonF, RISING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, RISING);

  LittleFS.begin(false);
  // PWM 설정 (기존과 동일하게 setup에서 직접)
  ledcSetup(PWM_HEATING_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_COOLING_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(HEATER_PWM_PIN, PWM_HEATING_CHANNEL);
  ledcAttachPin(COOLER_PWM_PIN, PWM_COOLING_CHANNEL);

  // 초기 상태 설정
  context->setState(new BootingState()); // 예시: 부팅 상태부터 시작
}

void loop()
{
  // Context를 통해 버튼 입력 처리
  context->processButtonInputs();

  // Context를 통해 현재 상태의 주기적 작업 및 시스템 업데이트 수행
  context->update();

  // 5초 이상 BOOT 버튼 누를 시 재부팅 로직 (기존 PushedButtonFunction의 일부)
  // 이 로직은 ISR에서 직접 플래그를 세우고, 여기서 타이머를 관리하거나,
  // 별도의 전역 함수로 관리하는 것이 상태 머신과 분리되어 더 깔끔할 수 있음.
  // 또는 모든 상태에서 공통으로 처리해야 하는 전역 이벤트 핸들러처럼 Context에 둘 수도 있음.
  // 여기서는 단순화를 위해 기존 전역 변수(reBootCheckTime 등)를 사용하는 로직을 유지한다고 가정.
  // (하지만 이 부분도 상태 머신 외부에서 독립적으로 관리하는 것이 좋음)
  // 예시:
  static unsigned long reBootCheckTime = 0;
  static bool checkToBootButtonToggleForReboot = false; // 이름 변경

  if (context->bootButtonPressed)
  { // Context의 플래그를 참고할 수 있으나, ISR에서 직접 설정된 전역 플래그를 쓰는게 나을수도
    // 이 부분은 Context의 processButtonInputs에서 bootButtonPressed가 true로 세팅된 후,
    // 여기서 재부팅 체크를 시작하도록 변경
    if (!checkToBootButtonToggleForReboot)
    {
      checkToBootButtonToggleForReboot = true; // 재부팅 체크 시작
      reBootCheckTime = millis();              // 현재 시간 기록
    }
  }

  if (checkToBootButtonToggleForReboot && digitalRead(BUTTON_BOOT) == HIGH)
  {
    if (reBootCheckTime == 0)
      reBootCheckTime = millis();
    if (millis() - reBootCheckTime >= 5000)
    {
      // ... (Deep Sleep 재부팅 로직)
      context->Display.setPowerSave(1);
      esp_sleep_enable_timer_wakeup(3 * 1000000);  
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
      esp_deep_sleep_start();
    }
  }
  else if (digitalRead(BUTTON_BOOT) == LOW && checkToBootButtonToggleForReboot)
  {
    reBootCheckTime = 0;
    checkToBootButtonToggleForReboot = false;
    // 여기서 ::bootButton = false; 와 같이 전역 ISR 플래그를 직접 리셋하면 안됨.
    // ISR 플래그는 Context의 processButtonInputs에서 소비 후 리셋하는 것이 일관적.
    // 이 재부팅 로직은 Context의 버튼 처리와 별개로 동작해야 할 수 있음.
  }
}

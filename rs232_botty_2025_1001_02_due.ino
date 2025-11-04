/*
  아두이노메가/듀 공용 IoT 제어 프로그램 (RS232 JSON I/O)
  - 권장 보드: Arduino Mega 2560 / Arduino Due
  - 라이브러리: ArduinoJson (v6 이상)

  기능 요약
  1) 0.1초 주기로 모든 센서값을 읽어 전역 상태 저장소에 보관하고, 그 저장소만 읽어 JSON 배열로 RS232 출력
  2) RS232로 수신한 JSON 배열의 제어 명령을 파싱하여 각 장비의 출력핀 제어
  3) 장비/프로토콜은 사용자 요청 사양과 1:1 매핑
*/

#include <Arduino.h>
#include <ArduinoJson.h>

// ==============================
// === 설정: 핀 매핑 (수정 지점) ===
// ==============================
// --- Cup(용기디스펜서) 센서 입력핀 (4채널) ---
const uint8_t CUP_STOCK_PINS[4]   = {22, 23, 24, 25};  // 재고 감지 (digital)
const uint8_t CUP_DISP_PINS[4]    = {26, 27, 28, 29};  // 배출 감지 (digital)
// MEGA의 디지털 62~65는 DUE에서 존재하지 않으므로 보드 독립적으로 A8~A11 사용
const uint8_t CUP_REVO_PINS[4]    = {A8, A9, A10, A11};  // 회전 감지 (digital)

// --- Cup 제어 출력핀 (4채널) ---
const uint8_t CUP_CTRL_PINS[4]    = {30, 31, 32, 33};  // start/stopdispense (output)

// --- Ramen(면디스펜서) 센서 입력핀 (4채널) ---
const uint8_t RAMEN_STOCK_PINS[4]    = {34, 35, 36, 37};  // 재고 감지 (digital)
const uint8_t RAMEN_LOADCELL_PINS[4] = {A0, A1, A2, A3};  // 배출 감지(ADC 0~1023)

// --- Powder(스프디스펜서) 제어 출력핀 (4채널) ---
const uint8_t POWDER_CTRL_PINS[4] = {38, 39, 40, 41};  // 모터 제어 (on/off)

// --- Cooker(조리기) 센서/출력핀 (4채널) ---
const uint8_t COOKER_WORK_PINS[4] = {42, 43, 44, 45};  // 조리중 감지 (digital)
const uint8_t COOKER_CTRL_PINS[4] = {46, 47, 48, 49};  // start/stopcook (output)

// --- Outlet(배출구) 센서/출력핀 (4채널) ---
const uint8_t OUTLET_SONAR_PINS[4] = {A4, A5, A6, A7}; // 초음파 ADC(0~1023) -> 1~250로 맵핑
const uint8_t OUTLET_DOOR_SENS_PINS[4]  = {50, 51, 52, 53}; // 도어 열림/닫힘 센서 (digital)
const uint8_t OUTLET_SLIDE_SENS_PINS[4] = {2, 3, 4, 5};     // 슬라이더 위치 센서 (digital)
// 출력: 도어 구동, 슬라이드 구동
const uint8_t OUTLET_DOOR_CTRL_PINS[4]  = {6, 7, 8, 9};     // opendoor/closedoor
const uint8_t OUTLET_SLIDE_CTRL_PINS[4] = {10, 11, 12, 13}; // openslide/closeslide

// --- Entry Door(출입문) 센서 입력핀 (1개 장치, 2개 센서) ---
const uint8_t DOOR_SENSOR1_PIN = 14;  // digital
const uint8_t DOOR_SENSOR2_PIN = 15;  // digital

// ==============================
// === 동작 파라미터/버퍼 크기 ===
// ==============================
const unsigned long PUBLISH_INTERVAL_MS = 100;  // 0.1초
// JSON 버퍼: 상태 21개 오브젝트 여유분 포함
static const size_t JSON_OUT_CAPACITY = 16384;
static const size_t JSON_IN_CAPACITY  = 16384;

// 초음파 값 맵 범위(사양: 1 ~ 250)
const int SONAR_MIN = 1;
const int SONAR_MAX = 250;
// CUP 디스펜서 모터 동작중 센서 체크전 대기 시간
const unsigned long CUP_DISPENSE_WAIT_MS = 1500;

// ==============================
// === 전역 상태 변수 ===
// ==============================
// 용기 모터 동작중 상태
bool cup_dispense_running[4] = {false};
bool cup_dispense_waiting[4] = {false};
unsigned long cupStartMs[4] = {0};

// ==============================
// === 전역 상태 저장소(메모리) ===
// ==============================
int cup_stock[4]      = {0};
int cup_dispense[4]   = {0};

int ramen_stock[4]    = {0};
int ramen_loadcell[4] = {0};

int powder_running[4] = {0}; // 모터 동작 여부(출력 상태 기반)

int cooker_work[4]    = {0};

int outlet_sonar[4]   = {0}; // 1~250로 맵핑된 값 저장
int outlet_door[4]    = {0};
int outlet_slide[4]   = {0};

int door_sensor1      = 0;
int door_sensor2      = 0;

// 직렬 수신 버퍼(명령 수신용)
String rxBuffer;
int bracketDepth = 0;
bool inString = false;

// 수신 프레임 진행 여부(대괄호 열림~닫힘 사이)
volatile bool receivingFrame = false;

// ==============================
// === 유틸 함수들 ===
// ==============================
static inline int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// analogRead(0~1023) -> 1~250 맵핑
int mapAnalogToSonarRange(int raw) {
  long mapped = map(raw, 0, 1023, SONAR_MIN, SONAR_MAX);
  return clampInt((int)mapped, SONAR_MIN, SONAR_MAX);
}

bool isValidControlIndex(int control) {
  return control >= 1 && control <= 4;
}

// ==============================
// === 핀 초기화 ===
// ==============================
void initPins() {
  // Cup
  for (int i = 0; i < 4; i++) {
    pinMode(CUP_STOCK_PINS[i], INPUT_PULLUP);
    pinMode(CUP_DISP_PINS[i], INPUT_PULLUP);
    pinMode(CUP_REVO_PINS[i], INPUT_PULLUP);
    pinMode(CUP_CTRL_PINS[i], OUTPUT);
    digitalWrite(CUP_CTRL_PINS[i], HIGH);
  }

  // Ramen
  for (int i = 0; i < 4; i++) {
    pinMode(RAMEN_STOCK_PINS[i], INPUT_PULLUP);
    // LOADCELL analog -> pinMode 필요X
  }

  // Powder (output only)
  for (int i = 0; i < 4; i++) {
    pinMode(POWDER_CTRL_PINS[i], OUTPUT);
    digitalWrite(POWDER_CTRL_PINS[i], HIGH);
    powder_running[i] = 0;
  }

  // Cooker
  for (int i = 0; i < 4; i++) {
    pinMode(COOKER_WORK_PINS[i], INPUT_PULLUP);
    pinMode(COOKER_CTRL_PINS[i], OUTPUT);
    digitalWrite(COOKER_CTRL_PINS[i], HIGH);
  }

  // Outlet
  for (int i = 0; i < 4; i++) {
    pinMode(OUTLET_DOOR_SENS_PINS[i], INPUT_PULLUP);
    pinMode(OUTLET_SLIDE_SENS_PINS[i], INPUT_PULLUP);

    pinMode(OUTLET_DOOR_CTRL_PINS[i], OUTPUT);
    pinMode(OUTLET_SLIDE_CTRL_PINS[i], OUTPUT);
    digitalWrite(OUTLET_DOOR_CTRL_PINS[i], HIGH);
    digitalWrite(OUTLET_SLIDE_CTRL_PINS[i], HIGH);
  }

  // Entry Door
  pinMode(DOOR_SENSOR1_PIN, INPUT_PULLUP);
  pinMode(DOOR_SENSOR2_PIN, INPUT_PULLUP);
}

// ==============================
// === 센서 스캔 & 상태저장 ===
// ==============================
void readAllSensorsIntoState() {
  // Cup
  for (int i = 0; i < 4; i++) {
    // PULLUP 사용 시 LOW=감지/비감지 논리 반전 가능 -> 실제 센서 논리에 맞게 필요시 ! 연산
    cup_stock[i]    = !digitalRead(CUP_STOCK_PINS[i]) ? 1 : 0;
    cup_dispense[i] = !digitalRead(CUP_DISP_PINS[i])  ? 1 : 0;
  }

  // Ramen
  for (int i = 0; i < 4; i++) {
    ramen_stock[i]    = !digitalRead(RAMEN_STOCK_PINS[i]) ? 1 : 0;
    ramen_loadcell[i] = analogRead(RAMEN_LOADCELL_PINS[i]); // 0~1023 (DUE는 setup에서 10bit로 통일)
  }

  // Powder -> 출력 상태로 판단 (모터 동작중 여부)
  for (int i = 0; i < 4; i++) {
    // 대부분의 보드에서 출력핀 digitalRead는 마지막 출력 상태를 반환
    powder_running[i] = !digitalRead(POWDER_CTRL_PINS[i]) ? 1 : 0;
  }

  // Cooker
  for (int i = 0; i < 4; i++) {
    cooker_work[i] = !digitalRead(COOKER_WORK_PINS[i]) ? 1 : 0;
  }

  // Outlet
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(OUTLET_SONAR_PINS[i]); // 0~1023
    outlet_sonar[i] = mapAnalogToSonarRange(raw);
    outlet_door[i]  = !digitalRead(OUTLET_DOOR_SENS_PINS[i])  ? 1 : 0;
    outlet_slide[i] = !digitalRead(OUTLET_SLIDE_SENS_PINS[i]) ? 1 : 0;
  }

  // Entry Door
  door_sensor1 = !digitalRead(DOOR_SENSOR1_PIN) ? 1 : 0;
  door_sensor2 = !digitalRead(DOOR_SENSOR2_PIN) ? 1 : 0;
}

// ==============================
// === 상태 JSON 생성 & 출력 ===
// ==============================
void publishStateJson() {
  StaticJsonDocument<JSON_OUT_CAPACITY> doc;
  JsonArray arr = doc.to<JsonArray>();

  // 2. Cup x4
  for (int i = 0; i < 4; i++) {
    JsonObject o = arr.createNestedObject();
    o["device"]   = "cup";
    o["control"]  = i + 1;
    o["stock"]    = cup_stock[i];
    o["dispense"] = cup_dispense[i];
  }

  // 3. Ramen x4
  for (int i = 0; i < 4; i++) {
    JsonObject o = arr.createNestedObject();
    o["device"]   = "ramen";
    o["control"]  = i + 1;
    o["stock"]    = ramen_stock[i];
    o["loadcell"] = ramen_loadcell[i];
  }

  // 4. Powder x4 (모터 동작중 여부)
  for (int i = 0; i < 4; i++) {
    JsonObject o = arr.createNestedObject();
    o["device"]   = "powder";
    o["control"]  = i + 1;
    o["dispense"] = powder_running[i];
  }

  // 5. Cooker x4 (조리중 여부)
  for (int i = 0; i < 4; i++) {
    JsonObject o = arr.createNestedObject();
    o["device"]   = "cooker";
    o["control"]  = i + 1;
    o["work"]     = cooker_work[i];
  }

  // 6. Outlet x4
  for (int i = 0; i < 4; i++) {
    JsonObject o = arr.createNestedObject();
    o["device"]  = "outlet";
    o["control"] = i + 1;
    o["sonar"]   = outlet_sonar[i];
    o["door"]    = outlet_door[i];
    o["slide"]   = outlet_slide[i];
  }

  // 7. Entry Door x1
  {
    JsonObject o = arr.createNestedObject();
    o["device"]  = "door";
    o["sensor1"] = door_sensor1;
    o["sensor2"] = door_sensor2;
  }

  // 직렬 출력
  serializeJson(arr, Serial);
  Serial.println();
}

// ==============================
// === 명령 처리 ===
// ==============================
void handleCupCommand(const char* func, int control) {
  if (!isValidControlIndex(control)) return;
  int idx = control - 1;
  if (strcmp(func, "startdispense") == 0) {
    cup_dispense_running[idx] = true;
    cup_dispense_waiting[idx] = true;
    cupStartMs[idx] = millis();
    digitalWrite(CUP_CTRL_PINS[idx], LOW);
  } else if (strcmp(func, "stopdispense") == 0) {
    cup_dispense_running[idx] = false;
    digitalWrite(CUP_CTRL_PINS[idx], HIGH);
  }
}

void handlePowderCommand(const char* func, int control) {
  if (!isValidControlIndex(control)) return;
  int idx = control - 1;
  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(POWDER_CTRL_PINS[idx], LOW);
    powder_running[idx] = 1;
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(POWDER_CTRL_PINS[idx], HIGH);
    powder_running[idx] = 0;
  }
}

void handleCookerCommand(const char* func, int control) {
  if (!isValidControlIndex(control)) return;
  int idx = control - 1;
  if (strcmp(func, "startcook") == 0) {
    digitalWrite(COOKER_CTRL_PINS[idx], LOW);
  } else if (strcmp(func, "stopcook") == 0) {
    digitalWrite(COOKER_CTRL_PINS[idx], HIGH);
  }
}

void handleOutletCommand(const char* func, int control) {
  if (!isValidControlIndex(control)) return;
  int idx = control - 1;

  if (strcmp(func, "opendoor") == 0) {
    digitalWrite(OUTLET_DOOR_CTRL_PINS[idx], LOW);
  } else if (strcmp(func, "closedoor") == 0) {
    digitalWrite(OUTLET_DOOR_CTRL_PINS[idx], HIGH);
  } else if (strcmp(func, "openslide") == 0) {
    digitalWrite(OUTLET_SLIDE_CTRL_PINS[idx], LOW);
  } else if (strcmp(func, "closeslide") == 0) {
    digitalWrite(OUTLET_SLIDE_CTRL_PINS[idx], HIGH);
  } else if (strcmp(func, "stopoutlet") == 0) {
    digitalWrite(OUTLET_DOOR_CTRL_PINS[idx], HIGH);
    digitalWrite(OUTLET_SLIDE_CTRL_PINS[idx], HIGH);
  }
}

// 들어온 JSON 배열을 순회하며 각 오브젝트 처리
void processIncomingJsonArray(JsonArray arr) {
  for (JsonObject obj : arr) {
    const char* device = obj["device"] | "";
    const char* func   = obj["function"] | "";
    int control        = obj["control"]  | 0;

    if (strcmp(device, "cup") == 0) {
      handleCupCommand(func, control);
    } else if (strcmp(device, "powder") == 0) {
      handlePowderCommand(func, control);
    } else if (strcmp(device, "cooker") == 0) {
      handleCookerCommand(func, control);
    } else if (strcmp(device, "outlet") == 0) {
      handleOutletCommand(func, control);
    }
    // ramen/door는 입력 전용 -> 제어 명령 없음
  }
}

// ==============================
// === 직렬 수신(JSON framing) ===
// ==============================
// JSON 배열(대괄호로 시작~끝)을 안전하게 누적/파싱
void serialReceiveTask() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    rxBuffer += c;

    // 문자열 내부 여부 처리(따옴표)
    if (c == '"' && (rxBuffer.length() == 1 || rxBuffer[rxBuffer.length() - 2] != '\\')) {
      inString = !inString;
    }
    if (inString) continue;

    if (c == '[') {
      if (bracketDepth == 0) receivingFrame = true; // 프레임 시작
      bracketDepth++;
    } else if (c == ']') {
      bracketDepth--;
      if (bracketDepth <= 0) {
        // 하나의 JSON 배열 완성
        receivingFrame = false;                    // 프레임 종료
        StaticJsonDocument<JSON_IN_CAPACITY> inDoc;
        DeserializationError err = deserializeJson(inDoc, rxBuffer);

        rxBuffer = "";
        bracketDepth = 0;

        if (!err && inDoc.is<JsonArray>()) {
          JsonArray cmdArr = inDoc.as<JsonArray>();
          processIncomingJsonArray(cmdArr);

          // 에코 (대괄호 제거)
          String output;
          serializeJson(cmdArr, output);
          output.replace("[",""); output.replace("]","");
          Serial.print("###"); Serial.print(output); Serial.println("###");
        } else {
          Serial.println("### Error ###");
          while (Serial.available() > 0) Serial.read();  // 잔여 플러시
        }
      }
    }

    // 안전장치: 버퍼 과대 방지
    if (rxBuffer.length() > 16384) {
      rxBuffer = "";
      bracketDepth = 0;
      inString = false;
      receivingFrame = false;
    }
  }
}

void cupDispenseSensorCheck(void) {
  for (int idx = 0; idx < 4; idx++) {
    if (cup_dispense_running[idx] && !digitalRead(CUP_REVO_PINS[idx])) {
      if (cup_dispense_waiting[idx]) {
        if (millis() - cupStartMs[idx] >= CUP_DISPENSE_WAIT_MS) {
          cup_dispense_waiting[idx] = false;
        }
      } else {
        cup_dispense_running[idx] = false;
        digitalWrite(CUP_CTRL_PINS[idx], HIGH);
      }
    }
  }
}

// ==============================
// === 아두이노 표준 콜백 ===
// ==============================
unsigned long lastPublishMs = 0;

void setup() {
  Serial.begin(115200);   // RS232/USB 직렬

  // DUE(ARM)에서 ADC 해상도를 10비트(0~1023)로 통일
  #ifdef ARDUINO_ARCH_SAM
    analogReadResolution(10);
  #endif

  rxBuffer.reserve(4096);   // ★ String 재할당/파편화 방지(성능 안정화)
  initPins();
  lastPublishMs = millis();
}

void loop() {
  // 1) 수신 처리 (명령 파싱)
  serialReceiveTask();

  // 2) 컵 디스펜서 센서 체크
  cupDispenseSensorCheck();

  // 3) 주기적 상태 스캔 & 송신
  unsigned long now = millis();
  // ★ 수신 프레임 처리 중이거나, RX 버퍼에 데이터가 남아있으면 송신 보류
  if (!receivingFrame && Serial.available() == 0) {
    if (now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
      readAllSensorsIntoState(); // 모든 센서를 상태저장소에 갱신
      publishStateJson();        // 저장소를 바탕으로 JSON 배열 출력
      lastPublishMs = now;
    }
  }
}

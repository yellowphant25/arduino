#include "initialize.h"

// === 1. CUP 핀 매핑 === 
const int cupMotorPins[] = {4, 8, 12, 24};
const int cupRotateSensorPins[] = {5, 9, 13, 25};
const int cupEjectSensorPins[] = {6, 10, 22, 26};
const int cupStockSensorPins[] = {7, 11, 23, 27};
const int cupMotorCurrentPins[] = {A0, A1, A2, A3};
const int cupInductionCurrentPins[] = {A6, A7, A8, A9}; 

// === 2. RAMEN 핀 매핑 ===
const int ramenLiftFwdPins[] = {4, 13, 30, 39};
const int ramenLiftRevPins[] = {5, 22, 31, 40};
const int ramenEjectFwdPins[] = {6, 23, 32, 41};
const int ramenEjectRevPins[] = {7, 24, 33, 42};
const int ramenEjectTopPins[] = {8, 25, 34, 43};
const int ramenEjectBotPins[] = {9, 26, 35, 44};
const int ramenLiftTopPins[] = {10, 27, 36, 45};
const int ramenLiftBotPins[] = {11, 28, 37, 46};
const int ramenNoodleSensorPins[] = {12, 29, 38, 47};
const int ramenEncoderPins[] = {2, 3, 18, 19}; // INT0~3
const int ramenLiftCurrentPins[] = {A0, A2, A4, A6};
const int ramenEjectCurrentPins[] = {A1, A3, A5, A7};

// === 3. POWDER 핀 매핑 ===
const int powderMotorPins[] = {4, 5, 6, 7, 8, 9, 10, 11};
const int powderCurrentPins[] = {A0, A1, A2, A3, A4, A5, A6, A7}; // 1번장비 A0,A1 / 2번 A2,A3...

// === 4. OUTLET 핀 매핑 ===
const int outletMotorFwdPins[] = {4, 8, 12, 24};
const int outletMotorRevPins[] = {5, 9, 13, 25};
const int outletOpenSensorPins[] = {6, 10, 22, 26};
const int outletCloseSensorPins[] = {7, 11, 23, 27};
const int outletMotorCurrentPins[] = {A0, A3, A6, A9};
const int outletLoadcellPins[] = {A1, A4, A7, A10};
const int outletUltrasonicPins[] = {A2, A5, A8, A11};

// === 5. COOKER 핀 매핑 ===
const int cookerInductionPins[] = {32, 33, 34, 35};
const int cookerValvePins[] = {36, 37, 38, 39};
const int cookerCurrentPins[] = {A6, A7, A8, A9};


/**
 * @brief 메인 초기화 함수, API json값에 따라 변경 필요
 */
void initializeArduinoRole(int role, String json) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print(F("JSON 파싱 실패: "));
    Serial.println(error.c_str());
    return;
  }

  const char* deviceType = doc["device"];
  if (strcmp(deviceType, "setting") != 0) {
    Serial.println("ERROR : device에 setting이 매핑되어 있지 않습니다.");
    return;
  }

  // 역할(Role)에 따라 해당 장비의 초기화 함수만 호출
  switch (role) {
    case ROLE_CUP:
      initCupRobot(doc["cup"]);
      break;
    case ROLE_RAMEN:
      initRamenRobot(doc["ramen"]);
      break;
    case ROLE_POWDER:
      initPowderRobot(doc["powder"]);
      break;
    case ROLE_OUTLET:
      initOutletRobot(doc["outlet"]);
      break;
    case ROLE_COOKER:
      initCookerRobot(doc["cooker"]);
      break;
    default:
      Serial.println("오류: 알 수 없는 역할(Role)입니다.");
      break;
  }
}

/**
 * @brief 개별 장비 초기화
 */
void initCupRobot(int count) {
  if (count > 4) count = 4;
  Serial.print("CUP 장비 초기화 ("); Serial.print(count); Serial.println("대)");
  
  for (int i = 0; i < count; i++) {
    pinMode(cupMotorPins[i], OUTPUT);
    pinMode(cupRotateSensorPins[i], INPUT);
    pinMode(cupEjectSensorPins[i], INPUT);
    pinMode(cupStockSensorPins[i], INPUT);
  }
}

void initRamenRobot(int count) {
  if (count > 4) count = 4;
  Serial.print("RAMEN 장비 초기화 ("); Serial.print(count); Serial.println("대)");

  for (int i = 0; i < count; i++) {
    pinMode(ramenLiftFwdPins[i], OUTPUT);
    pinMode(ramenLiftRevPins[i], OUTPUT);
    pinMode(ramenEjectFwdPins[i], OUTPUT);
    pinMode(ramenEjectRevPins[i], OUTPUT);
    pinMode(ramenEjectTopPins[i], INPUT);
    pinMode(ramenEjectBotPins[i], INPUT);
    pinMode(ramenLiftTopPins[i], INPUT);
    pinMode(ramenLiftBotPins[i], INPUT);
    pinMode(ramenNoodleSensorPins[i], INPUT);
    pinMode(ramenEncoderPins[i], INPUT); // attachInterrupt()는 setup에서 별도 설정
  }
}

void initPowderRobot(int count) {
  if (count > 8) count = 8; // Powder는 최대 8대
  Serial.print("POWDER 장비 초기화 ("); Serial.print(count); Serial.println("대)");
  
  for (int i = 0; i < count; i++) {
    pinMode(powderMotorPins[i], OUTPUT);
  }
}

void initOutletRobot(int count) {
  if (count > 4) count = 4;
  Serial.print("OUTLET 장비 초기화 ("); Serial.print(count); Serial.println("대)");
  
  for (int i = 0; i < count; i++) {
    pinMode(outletMotorFwdPins[i], OUTPUT);
    pinMode(outletMotorRevPins[i], OUTPUT);
    pinMode(outletOpenSensorPins[i], INPUT);
    pinMode(outletCloseSensorPins[i], INPUT);
  }
}

void initCookerRobot(int count) {
  if (count > 4) count = 4;
  Serial.print("COOKER 장비 초기화 ("); Serial.print(count); Serial.println("대)");

  for (int i = 0; i < count; i++) {
    if (i == 0 || i == 1) { 
      pinMode(cookerInductionPins[i], OUTPUT);
      pinMode(cookerValvePins[i], OUTPUT);
    } 
    
    else { 
      pinMode(cookerInductionPins[i], INPUT); // '디지털입력 34번핀'
      pinMode(cookerValvePins[i], INPUT);     // 명세서: '디지털입력 38번핀'
    }
  }
}
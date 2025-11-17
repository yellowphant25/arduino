#include <Arduino.h>
#include <ArduinoJson.h>
#include "Protocol.h"  // 자신의 헤더
#include "config.h"    // 핀맵
#include "state.h"     // 전역 변수(current, state) 사용

enum RamenEjectState {
  EJECT_IDLE,
  EJECTING,
  EJECT_RETURNING
};
RamenEjectState ramenEjectStatus = EJECT_IDLE;

bool isPowderDispensing = false;
unsigned long powderStartTime = 0;
const unsigned long POWDER_DISPENSE_TIME_MS = 1000; 


volatile long cur_encoder1 = 0;
unsigned long start_encoder1 = 0;
unsigned long interval = 1000;

void replyCurrentSetting(const Setting& s) {
  StaticJsonDocument<256> doc;
  doc["device"] = "setting";
  if (s.cup)     doc["cup"]    = s.cup;
  if (s.ramen)   doc["ramen"]  = s.ramen;
  if (s.powder)  doc["powder"] = s.powder;
  if (s.cooker)  doc["cooker"] = s.cooker;
  if (s.outlet)  doc["outlet"] = s.outlet;
  serializeJson(doc, Serial);
  Serial.println();
}

// ===== 핀모드 설정 (Setting 시 호출) =====
void setupCup(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(CUP_MOTOR_OUT[i], OUTPUT);
    pinMode(CUP_ROT_IN[i],    INPUT);
    pinMode(CUP_DISP_IN[i],   INPUT);
    pinMode(CUP_STOCK_IN[i],  INPUT);
  }
}
void setupRamen(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(RAMEN_UP_FWD_OUT[i], OUTPUT);
    pinMode(RAMEN_UP_REV_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_FWD_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_REV_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_TOP_IN[i],  INPUT);
    pinMode(RAMEN_EJ_BTM_IN[i],  INPUT);
    pinMode(RAMEN_UP_TOP_IN[i],  INPUT);
    pinMode(RAMEN_UP_BTM_IN[i],  INPUT);
    pinMode(RAMEN_PRESENT_IN[i], INPUT);
    pinMode(RAMEN_ENC_INT[i],    INPUT);
  }
}
void setupPowder(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(POWDER_MOTOR_OUT[i], OUTPUT);
  }
}
void setupOutlet(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(OUTLET_FWD_OUT[i], OUTPUT);
    pinMode(OUTLET_REV_OUT[i], OUTPUT);
    pinMode(OUTLET_OPEN_IN[i], INPUT);
    pinMode(OUTLET_CLOSE_IN[i], INPUT);
  }
}
void setupCooker(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    if (i < 2) {
      pinMode(COOKER_IND_SIG[i], OUTPUT);
      pinMode(COOKER_WTR_SIG[i], OUTPUT);
    } else {
      pinMode(COOKER_IND_SIG[i], INPUT);
      pinMode(COOKER_WTR_SIG[i], INPUT);
    }
  }
}

// ===== 설정 적용 및 검증 (Setting 시 호출) =====
bool validateRules(const Setting& s, String& why) {
  uint8_t nonzeroCnt = (s.cup?1:0) + (s.ramen?1:0) + (s.powder?1:0) + (s.cooker?1:0) + (s.outlet?1:0);
  if (s.cup     > MAX_CUP)    { why = "cup max=4"; return false; }
  if (s.ramen   > MAX_RAMEN)  { why = "ramen max=4"; return false; }
  if (s.powder  > MAX_POWDER) { why = "powder max=8"; return false; }
  if (s.cooker  > MAX_COOKER) { why = "cooker max=4"; return false; }
  if (s.outlet  > MAX_OUTLET) { why = "outlet max=4"; return false; }
  if (nonzeroCnt == 0) { why = "no device count set"; return false; }
  if (s.cup>0 && s.cooker>0) {
    if (s.ramen==0 && s.powder==0 && s.outlet==0) return true;
    why = "only cup+cooker can be combined";
    return false;
  }
  if (nonzeroCnt == 1) return true;
  why = "invalid combination (only cup+cooker together; others solo)";
  return false;
}

void applySetting(const Setting& s) {
  if (s.cup)    setupCup(s.cup);
  if (s.ramen)  setupRamen(s.ramen);
  if (s.powder) setupPowder(s.powder);
  if (s.outlet) setupOutlet(s.outlet);
  if (s.cooker) setupCooker(s.cooker);
  current = s; // 전역 변수 'current'에 적용
}

void printMappingSummary(const Setting& s) {
  // (디버깅용 매핑 요약 함수)
  // ...
}

// =======================================================
// ===               ★ 제어 함수 (API 2.x)            ===
// =======================================================

bool handleCupCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cup) { Serial.println("invalid cup control num"); return false; }
  uint8_t idx = control - 1; 

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], HIGH);
    Serial.println("cup startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], LOW);
    Serial.println("cup stopdispense");
  } else { Serial.println("unknown cup function"); }
  return true;
}

bool handleRamenCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.ramen) { Serial.println("invalid ramen control num"); return false; }
  uint8_t idx = control - 1;
  Serial.println("start handle ramen");

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], HIGH); // ON
    Serial.println("ramen startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], LOW); // OFF
    Serial.println("ramen stopdispense");
  } else { Serial.println("unknown ramen function"); }
  return true;
}

bool handlePowderCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.powder) { Serial.println("invalid powder control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], HIGH); // ON
    Serial.println("powder startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], LOW); // OFF
    Serial.println("powder stopdispense");
  } else { Serial.println("unknown powder function"); }
  return true;
}

bool handleCookerCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cooker) { Serial.println("invalid cooker control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "startcook") == 0) {
    int water = doc["water"]; 
    int timer = doc["timer"]; 
    if (idx < 2) { // 1, 2번 장비만 출력
      digitalWrite(COOKER_WTR_SIG[idx], HIGH); // ON
      digitalWrite(COOKER_IND_SIG[idx], HIGH); // ON
    }
    Serial.println("cooker startcook");
  } else if (strcmp(func, "stopcook") == 0) {
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], LOW); // OFF
      digitalWrite(COOKER_IND_SIG[idx], LOW); // OFF
    }
    Serial.println("cooker stopcook");
  } else { Serial.println("unknown cooker function"); }
  return true;
}

bool handleOutletCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.outlet) { Serial.println("invalid outlet control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "opendoor") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], HIGH); // ON
    digitalWrite(OUTLET_REV_OUT[idx], LOW);  // OFF
    Serial.println("outlet opendoor");
  } else if (strcmp(func, "closedoor") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], LOW);  // OFF
    digitalWrite(OUTLET_REV_OUT[idx], HIGH); // ON
    Serial.println("outlet closedoor");
  } else if (strcmp(func, "stopoutlet") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], LOW); // OFF
    digitalWrite(OUTLET_REV_OUT[idx], LOW); // OFF
    Serial.println("outlet stopoutlet");
  } else { Serial.println("unknown outlet function"); }
  return true;
}

bool handleSettingJson(const JsonDocument& doc) {
  Setting next;
  next.cup     = doc["cup"]    | 0;
  next.ramen   = doc["ramen"]  | 0;
  next.powder  = doc["powder"] | 0;
  next.cooker  = doc["cooker"] | 0;
  next.outlet  = doc["outlet"] | 0;

  String why;
  if (!validateRules(next, why)) {
    Serial.println(why.c_str());
    return false;
  }

  applySetting(next);
  Serial.println("pins configured");
  // printMappingSummary(next); // 디버깅 필요시 주석 해제
  return true;
}

bool parseAndDispatch(const char* json) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) { Serial.println("json parse fail"); return false; }

  const char* dev = doc["device"] | "";

  if (strcmp(dev, "setting") == 0) {
    return handleSettingJson(doc);
  } else if (strcmp(dev, "query") == 0) {
    replyCurrentSetting(current);
    return true;
  }

  else if (strcmp(dev, "cup") == 0) {
    return handleCupCommand(doc);
  } else if (strcmp(dev, "ramen") == 0) {
    return handleRamenCommand(doc);
  } else if (strcmp(dev, "powder") == 0) {
    return handlePowderCommand(doc);
  } else if (strcmp(dev, "cooker") == 0) {
    return handleCookerCommand(doc);
  } else if (strcmp(dev, "outlet") == 0) {
    return handleOutletCommand(doc);
  }
  
  else {
    Serial.println("unsupported device field");
    return false;
  }
}

/**
 * @brief 용기 배출을 시작
 * (명령 수신 시 1회 호출)
 */
void startCupDispense() {
  Serial.println("명령: 용기 배출 시작 (Pin 4 HIGH)");
  digitalWrite(CUP_MOTOR_OUT[0], HIGH);
}

/**
 * @brief 용기 배출 멈춤 조건을 확인
 * (loop()에서 계속 호출)
 */
void checkCupDispense() {
  if (digitalRead(CUP_MOTOR_OUT[0]) == HIGH) {
    if (digitalRead(CUP_ROT_IN[0]) == HIGH) {
      Serial.println("완료: 용기 배출 중지 (Pin 4 LOW)");
      digitalWrite(CUP_MOTOR_OUT[0], LOW);
    }
  }
}

/**
 * @brief (2) 면 상승을 시작
 * (명령 수신 시 1회 호출)
 */
void startRamenRise() {
  Serial.println("명령: 면 상승 시작 (Pin 4 HIGH)");
  
  noInterrupts(); // ISR과의 충돌 방지
  start_encoder1 = cur_encoder1;
  interrupts();
  
  Serial.print("시작 엔코더 값: ");
  Serial.println(start_encoder1);
  
  digitalWrite(RAMEN_UP_FWD_OUT[0], HIGH);
}

/**
 * @brief 면 상승 멈춤 조건 3가지를 확인합니다.
 * (loop()에서 계속 호출)
 */
void checkRamenRise() {
  if (digitalRead(RAMEN_UP_FWD_OUT[0]) == HIGH) {
    bool stopMotor = false;
    
    if (digitalRead(RAMEN_PRESENT_IN[0]) == HIGH) {
      Serial.println("완료: 면 감지됨 (Pin 4 LOW)");
      stopMotor = true;
    }
    
    else if (digitalRead(RAMEN_UP_TOP_IN[0]) == HIGH) {
      Serial.println("완료: 상승 상한 도달 (Pin 4 LOW)");
      stopMotor = true;
    }

    else {
      long current_encoder_safe;
      noInterrupts(); 
      current_encoder_safe = cur_encoder1;
      interrupts();
      
      if (current_encoder_safe - start_encoder1 > interval) {
        Serial.println("완료: 엔코더 인터벌 도달 (Pin 4 LOW)");
        stopMotor = true;
      }
    }

    if (stopMotor) {
      digitalWrite(RAMEN_UP_FWD_OUT[0], LOW);
    }
  }
}

/**
 * @brief (3) 면 하강(초기화)을 시작합니다.
 * (명령 수신 시 1회 호출)
 */
void startRamenInit() {
  Serial.println("명령: 면 하강 시작 (Pin 5 HIGH)");
  digitalWrite(RAMEN_UP_REV_OUT[0], HIGH);
}

/**
 * @brief 면 하강(초기화) 멈춤 조건을 확인합니다.
 * (loop()에서 계속 호출)
 */
void checkRamenInit() {
  // 5번 핀이 켜져있을 때만 멈춤 조건을 검사
  if (digitalRead(RAMEN_UP_REV_OUT[0]) == HIGH) {
    // 11번 핀 (상승 하한)
    if (digitalRead(RAMEN_UP_BTM_IN[0]) == HIGH) {
      Serial.println("완료: 상승 하한 도달 (Pin 5 LOW)");
      digitalWrite(RAMEN_UP_REV_OUT[0], LOW);
    }
  }
}


/**
 * @brief 면 배출을 시작
 * (명령 수신 시 1회 호출)
 */
void startRamenEject() {
  // 이미 동작 중이면 무시
  if (ramenEjectStatus == EJECT_IDLE) {
    Serial.println("명령: 면 배출 시작 (Pin 6 HIGH)");
    ramenEjectStatus = EJECTING;
    digitalWrite(RAMEN_EJ_FWD_OUT[0], HIGH);
  }
}

/**
 * @brief 면 배출 상태 머신을 처리합니다.
 * (loop()에서 계속 호출)
 */
void checkRamenEject() {
  switch (ramenEjectStatus) {
    case EJECTING:
      // 배출 중 (정방향) -> 8번 핀 (배출 상한) 감시
      if (digitalRead(RAMEN_EJ_TOP_IN[0]) == HIGH) {
        Serial.println("상태: 배출 상한 도달. 복귀 시작 (Pin 6 LOW, Pin 7 HIGH)");
        digitalWrite(RAMEN_EJ_FWD_OUT[0], LOW);  // 정방향 정지
        digitalWrite(RAMEN_EJ_REV_OUT[0], HIGH); // 역방향 시작
        ramenEjectStatus = EJECT_RETURNING;
      }
      break;
      
    case EJECT_RETURNING:
      if (digitalRead(RAMEN_EJ_BTM_IN[0]) == HIGH) {
        Serial.println("완료: 배출 하한 도달. 작업 종료 (Pin 7 LOW)");
        digitalWrite(RAMEN_EJ_REV_OUT[0], LOW); // 역방향 정지
        ramenEjectStatus = EJECT_IDLE;
      }
      break;
      
    case EJECT_IDLE:
    default:
      // 할 일 없음
      break;
  }
}

/**
 * @brief (5) 스프 배출을 시작합니다. (타이머 시작)
 * (명령 수신 시 1회 호출)
 */
void startPowderDispense() {
  if (isPowderDispensing == false) {
    Serial.println("명령: 스프 배출 시작 (Pin 4 HIGH, 1초 타이머)");
    isPowderDispensing = true;
    powderStartTime = millis(); // 현재 시간 저장
    digitalWrite(POWDER_MOTOR_OUT[0], HIGH);
  }
}

/**
 * @brief 스프 배출 타이머를 확인합니다.
 * (loop()에서 계속 호출)
 */
void checkPowderDispense() {
  if (isPowderDispensing) {
    // 현재시간 - 시작시간 >= 1000ms (1초)
    if (millis() - powderStartTime >= POWDER_DISPENSE_TIME_MS) {
      Serial.println("완료: 1초 경과. 스프 배출 중지 (Pin 4 LOW)");
      digitalWrite(POWDER_MOTOR_OUT[0], LOW);
      isPowderDispensing = false;
    }
  }
}

/**
 * @brief 배출구 오픈
 * (명령 수신 시 1회 호출)
 */
void startOutletOpen() {
  Serial.println("명령: 배출구 오픈 시작 (Pin 4 HIGH)");
  digitalWrite(OUTLET_FWD_OUT[0], HIGH);
}

/**
 * @brief 배출구 닫기
 * (명령 수신 시 1회 호출)
 */
void startOutletClose() {
  Serial.println("명령: 배출구 닫기 시작 (Pin 5 HIGH)");
  digitalWrite(OUTLET_REV_OUT[0], HIGH);
}

/**
 * @brief 배출구 오픈/닫힘 멈춤 조건을 확인
 * (loop()에서 계속 호출)
 */
void checkOutlet() {
  // 오픈 멈춤 조건 (4번 핀이 켜져있을 때 6번 핀 감시)
  if (digitalRead(OUTLET_FWD_OUT[0]) == HIGH) {
    if (digitalRead(OUTLET_OPEN_IN[0]) == HIGH) {
      Serial.println("완료: 배출구 오픈 완료 (Pin 4 LOW)");
      digitalWrite(OUTLET_FWD_OUT[0], LOW);
    }
  }

  // 닫힘 멈춤 조건 (5번 핀이 켜져있을 때 7번 핀 감시)
  if (digitalRead(OUTLET_REV_OUT[0]) == HIGH) {
    if (digitalRead(OUTLET_CLOSE_IN[0]) == HIGH) {
      Serial.println("완료: 배출구 닫힘 완료 (Pin 5 LOW)");
      digitalWrite(OUTLET_REV_OUT[0], LOW);
    }
  }
}
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Protocol.h"  // 자신의 헤더
#include "config.h" // pin map
#include "state.h"  // 전역 및 상태 정의

// ===== 전역 상태 변수 =====
enum RamenEjectState {
  EJECT_IDLE,
  EJECTING,
  EJECT_RETURNING
};
RamenEjectState ramenEjectStatus = EJECT_IDLE;  // 1번 장비(idx=0) 전용

// [수정] 파우더 관련 변수를 MAX_POWDER 배열로 변경
bool isPowderDispensing[MAX_POWDER] = {false};
unsigned long powderStartTime[MAX_POWDER] = {0};
unsigned long powderDuration[MAX_POWDER] = {0}; // JSON에서 받은 시간을 ms로 저장

volatile long cur_encoder1 = 0;  // 1번 장비(idx=0) 전용
unsigned long start_encoder1 = 0;
unsigned long interval = 1000;  // (엔코더 인터벌 상수)

// =======================================================
// === 1. 설정 (Setup) 및 파싱 (Parse) 함수
// =======================================================

void replyCurrentSetting(const Setting& s) {
  StaticJsonDocument<256> doc;
  doc["device"] = "setting";
  if (s.cup) doc["cup"] = s.cup;
  if (s.ramen) doc["ramen"] = s.ramen;
  if (s.powder) doc["powder"] = s.powder;
  if (s.cooker) doc["cooker"] = s.cooker;
  if (s.outlet) doc["outlet"] = s.outlet;
  serializeJson(doc, Serial);
  Serial.println();
}

// ===== 핀모드 설정 (Setting 시 호출) =====
void setupCup(uint8_t n) {
  pinMode(CUP_MOTOR_OUT[n-1], OUTPUT);
  pinMode(CUP_ROT_IN[n-1], INPUT_PULLUP);  // INPUT_PULLUP으로 통일 (필요시)
  pinMode(CUP_DISP_IN[n-1], INPUT);
  pinMode(CUP_STOCK_IN[n-1], INPUT);
}
void setupRamen(uint8_t n) {
  pinMode(RAMEN_UP_FWD_OUT[n-1], OUTPUT);
  pinMode(RAMEN_UP_REV_OUT[n-1], OUTPUT);
  pinMode(RAMEN_EJ_FWD_OUT[n-1], OUTPUT);
  pinMode(RAMEN_EJ_REV_OUT[n-1], OUTPUT);
  pinMode(RAMEN_EJ_TOP_IN[n-1], INPUT_PULLUP);
  pinMode(RAMEN_EJ_BTM_IN[n-1], INPUT_PULLUP);
  pinMode(RAMEN_UP_TOP_IN[n-1], INPUT_PULLUP);
  pinMode(RAMEN_UP_BTM_IN[n-1], INPUT_PULLUP);
  pinMode(RAMEN_PRESENT_IN[n-1], INPUT_PULLUP);

  // [수정] 모든 RAMEN_ENCORDER 핀 설정 (MAX_RAMEN * 2 = 8개)
  for (uint8_t i = 0; i < MAX_RAMEN * 2; i++) { 
     pinMode(RAMEN_ENCORDER[n-1], INPUT_PULLUP); 
  }
}
void setupPowder(uint8_t n) {
  pinMode(POWDER_MOTOR_OUT[n-1], OUTPUT);
}

void setupOutlet(uint8_t n) {
  pinMode(OUTLET_FWD_OUT[n-1], OUTPUT);
  pinMode(OUTLET_REV_OUT[n-1], OUTPUT);
  pinMode(OUTLET_OPEN_IN[n-1], INPUT_PULLUP);   // PULLUP 설정 확인
  pinMode(OUTLET_CLOSE_IN[n-1], INPUT_PULLUP);  // PULLUP 설정 확인
}
void setupCooker(uint8_t n) {
  pinMode(COOKER_IND_SIG[n-1], OUTPUT);
  pinMode(COOKER_WTR_SIG[n-1], OUTPUT);
}

// ===== 설정 적용 및 검증 (Setting 시 호출) =====
bool validateRules(const Setting& s, String& why) {
  uint8_t nonzeroCnt = (s.cup ? 1 : 0) + (s.ramen ? 1 : 0) + (s.powder ? 1 : 0) + (s.cooker ? 1 : 0) + (s.outlet ? 1 : 0);
  if (s.cup > MAX_CUP) {
    why = "cup max=4";
    return false;
  }
  if (s.ramen > MAX_RAMEN) {
    why = "ramen max=4";
    return false;
  }
  if (s.powder > MAX_POWDER) {
    why = "powder max=8";
    return false;
  }
  if (s.cooker > MAX_COOKER) {
    why = "cooker max=4";
    return false;
  }
  if (s.outlet > MAX_OUTLET) {
    why = "outlet max=4";
    return false;
  }
  if (nonzeroCnt == 0) {
    why = "no device count set";
    return false;
  }
  if (s.cup > 0 && s.cooker > 0) {
    if (s.ramen == 0 && s.powder == 0 && s.outlet == 0) return true;
    why = "only cup+cooker can be combined";
    return false;
  }
  if (nonzeroCnt == 1) return true;
  why = "invalid combination (only cup+cooker together; others solo)";
  return false;
}

void applySetting(const Setting& s) {
  if (s.cup) setupCup(s.cup);
  if (s.ramen) setupRamen(s.ramen);
  if (s.powder) setupPowder(s.powder);
  if (s.outlet) setupOutlet(s.outlet);
  if (s.cooker) setupCooker(s.cooker);
  current = s;  // 전역 변수 'current'에 적용
}

/**
 * @brief [추가] 용기 배출을 시작 (1번 장비)
 */
void startCupDispense() {
  int idx = current.cup - 1;

  Serial.println("명령: 용기 배출 시작 (Pin 4 HIGH)");
  digitalWrite(CUP_MOTOR_OUT[idx], HIGH);
}

/**
 * @brief 용기 배출 멈춤 조건을 확인 (1번 장비)
 */
void checkCupDispense() {
  int idx = current.cup - 1;

  if (digitalRead(CUP_MOTOR_OUT[idx]) == HIGH) {
    // CUP_ROT_IN[0]이 LOW(감지됨)일 때 정지 (INPUT_PULLUP 가정)
    if (digitalRead(CUP_ROT_IN[idx]) == LOW) { 
      Serial.println("완료: 용기 배출 중지 (Pin 4 LOW)");
      digitalWrite(CUP_MOTOR_OUT[idx], LOW);
    }
  }
}

void startRamenInit(int8_t idx) {
  Serial.print("명령: 면 하강 시작");
  digitalWrite(RAMEN_UP_REV_OUT[idx], HIGH);
}

/**
 * @brief 면 상승을 시작 (1번 장비)
 */
void startRamenRise(int8_t idx) {
  Serial.println("명령: 면 상승 시작 (Pin 4 HIGH)");
  noInterrupts();  // ISR과의 충돌 방지
  start_encoder1 = cur_encoder1;
  interrupts();
  Serial.print("시작 엔코더 값: ");
  Serial.println(start_encoder1);
  digitalWrite(RAMEN_UP_FWD_OUT[idx], HIGH);
}

/**
 * @brief 면 상승 멈춤 조건 3가지를 확인 (1번 장비)
 */
void checkRamenRise() {
  int idx = current.ramen - 1;

  if (digitalRead(RAMEN_UP_FWD_OUT[idx]) == HIGH) {
    bool stopMotor = false;

    if (digitalRead(RAMEN_PRESENT_IN[idx]) == LOW) {
      Serial.println("완료: 면 감지됨 (Pin 4 LOW)");
      stopMotor = true;

    } else if (digitalRead(RAMEN_UP_TOP_IN[idx]) == HIGH) {
      Serial.println("완료: 상승 상한 도달 (Pin 4 LOW)");
      stopMotor = true;

    } else {
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
      digitalWrite(RAMEN_UP_FWD_OUT[idx], LOW);
    }
  }
}

/**
 * @brief 면 하강(초기화) 멈춤 조건을 확인 (1번 장비)
 */
void checkRamenInit() {
  int idx = current.ramen - 1;

  if (digitalRead(RAMEN_UP_REV_OUT[idx]) == HIGH) {
    if (digitalRead(RAMEN_UP_BTM_IN[idx]) == HIGH) {  // (HIGH 감지 가정)
      Serial.println("완료: 상승 하한 도달 (Pin 5 LOW)");
      digitalWrite(RAMEN_UP_REV_OUT[idx], LOW);
    }
  }
}

/**
 * @brief 면 배출을 시작 (1번 장비, 상태 머신)
 */
void startRamenEject(int8_t idx) {
  if (ramenEjectStatus == EJECT_IDLE) {
    Serial.println("명령: 면 배출 시작 (Pin 6 HIGH)");
    ramenEjectStatus = EJECTING;
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], HIGH);
  }
}

/**
 * @brief 면 배출 상태 머신을 처리 (1번 장비)
 */
void checkRamenEject() {
  int idx = current.ramen - 1;

  switch (ramenEjectStatus) {
    
    case EJECTING:
      if (digitalRead(RAMEN_EJ_TOP_IN[idx]) == HIGH) { 
        Serial.println("상태: 배출 상한 도달. 복귀 시작");
        digitalWrite(RAMEN_EJ_FWD_OUT[idx], LOW);
        digitalWrite(RAMEN_EJ_REV_OUT[idx], HIGH);
        ramenEjectStatus = EJECT_RETURNING;
      }
      break;

    case EJECT_RETURNING:
      // [요청사항 반영] 면 "상승 하한"이 감지되면 (HIGH)
      if (digitalRead(RAMEN_UP_BTM_IN[idx]) == HIGH) { 
        Serial.println("완료: 상승 하한 감지. 배출 복귀 모터(pin 7) 정지");
        digitalWrite(RAMEN_EJ_REV_OUT[idx], LOW);
        ramenEjectStatus = EJECT_IDLE;
      }
      break;
    default: break;
  }

  // for (uint8_t i = 0; i < current.ramen; i++) {
  //     if (digitalRead(RAMEN_EJ_FWD_OUT[i]) == HIGH && digitalRead(RAMEN_EJ_TOP_IN[i]) == HIGH) {
  //         digitalWrite(RAMEN_EJ_FWD_OUT[i], LOW);
  //     }
  //     if (digitalRead(RAMEN_EJ_REV_OUT[i]) == HIGH && digitalRead(RAMEN_EJ_BTM_IN[i]) == HIGH) {
  //         digitalWrite(RAMEN_EJ_REV_OUT[i], LOW);
  //     }
  // }
}


/**
 * @brief [수정] 스프 배출을 시작 (지정된 장비, 지정된 시간)
 */
void startPowderDispense(uint8_t idx, unsigned long durationMs) {
  if (isPowderDispensing[idx] == false) {
    Serial.print("명령: 스프 배출 시작 (장비: ");
    Serial.print(idx + 1);
    Serial.print(", 시간: ");
    Serial.print(durationMs);
    Serial.println("ms)");
    
    isPowderDispensing[idx] = true;
    powderDuration[idx] = durationMs;
    powderStartTime[idx] = millis();
    digitalWrite(POWDER_MOTOR_OUT[idx], HIGH);
  }
}

/**
 * @brief [수정] "모든" 스프 배출 타이머를 확인 (loop에서 계속 호출)
 */
void checkPowderDispense() {
  int idx = current.powder - 1;
    if (isPowderDispensing[idx]) {
      if (millis() - powderStartTime[idx] >= powderDuration[idx]) {
        Serial.print("완료: 시간 경과. 스프 배출 중지 (장비: ");
        Serial.print(idx);
        Serial.println(")");
        digitalWrite(POWDER_MOTOR_OUT[idx], LOW);
        isPowderDispensing[idx] = false;
      }
    }
}

/**
 * @brief [수정] 배출구 오픈 시작 (모든 장비)
 */
void startOutletOpen(int pinIdx) {
  Serial.print("명령: 배출구 오픈 시작 (장비: ");
  Serial.print(pinIdx + 1);
  Serial.println(")");
  digitalWrite(OUTLET_FWD_OUT[pinIdx], HIGH);
}

/**
 * @brief [수정] 배출구 닫기 시작 (모든 장비)
 */
void startOutletClose(int pinIdx) {
  Serial.print("명령: 배출구 닫기 시작 (장비: ");
  Serial.print(pinIdx + 1);
  Serial.println(")");
  digitalWrite(OUTLET_REV_OUT[pinIdx], HIGH);
}

/**
 * @brief [수정] 배출구 오픈/닫힘 멈춤 조건을 "모든 장비"에 대해 확인
 * (loop()에서 계속 호출)
 */
void checkOutlet() {
  for (uint8_t i = 0; i < current.outlet; i++) {
    if (digitalRead(OUTLET_FWD_OUT[i]) == HIGH) {
      if (digitalRead(OUTLET_OPEN_IN[i]) == LOW) { // LOW 감지
        Serial.print("완료: 배출구 오픈 완료 (장비: ");
        Serial.print(i + 1);
        Serial.println(")");
        digitalWrite(OUTLET_FWD_OUT[i], LOW);
      }
    }

    if (digitalRead(OUTLET_REV_OUT[i]) == HIGH) {
      if (digitalRead(OUTLET_CLOSE_IN[i]) == LOW) { // LOW 감지
        Serial.print("완료: 배출구 닫힘 완료 (장비: ");
        Serial.print(i + 1);
        Serial.println(")");
        digitalWrite(OUTLET_REV_OUT[i], LOW);
      }
    }
  }
}


// =======================================================
// === 3. JSON 명령 핸들러 (API 2.x)
// =======================================================

bool handleCupCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cup) {
    Serial.println("invalid cup control num");
    return false;
  }
  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {
    if (idx == 0) {
      startCupDispense();
    } else {
      digitalWrite(CUP_MOTOR_OUT[idx], HIGH);
    }
    Serial.println("cup startdispense");

  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], LOW);
    Serial.println("cup stopdispense");

  } else {
    Serial.println("unknown cup function");
  }
  return true;
}

bool handleRamenCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.ramen) {
    Serial.println("invalid ramen control num");
    return false;
  }
  uint8_t idx = control - 1;
  Serial.println("start handle ramen");

  if (strcmp(func, "startdispense") == 0) {
    startRamenEject(idx);
    Serial.println("ramen startdispense");

  } else if (strcmp(func, "readydispense") == 0) {
    startRamenRise(idx);
    Serial.println("ramen readydispense");

  } else if (strcmp(func, "initdispense") == 0) {
    startRamenInit(idx);
    Serial.println("ramen initdispense");

  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], LOW);
    digitalWrite(RAMEN_EJ_REV_OUT[idx], LOW);
    digitalWrite(RAMEN_UP_FWD_OUT[idx], LOW);
    digitalWrite(RAMEN_UP_REV_OUT[idx], LOW);
    
    ramenEjectStatus = EJECT_IDLE; 

    Serial.println("ramen stopdispense (ALL STOP)");
} else {
    Serial.println("unknown ramen function");
  }
  return true;
}

bool handlePowderCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.powder) {
    Serial.println("invalid powder control num");
    return false;
  }
  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {
    
    int time_val = doc["time"] | 0; 
    
    if (time_val <= 0) {
        Serial.println("Error: 'time' 0 or missing for powder dispense");
        return false; 
    }

    unsigned long durationMs = (unsigned long)time_val * 100;

    Serial.print("powder startdispense (장비: ");
    Serial.print(idx + 5); // 🔴 이 부분은 idx + 1로 수정하는 것이 맞습니다.
    Serial.print(", 시간: ");
    Serial.print(durationMs);
    Serial.println(" ms)");

    startPowderDispense(idx, durationMs);

  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], LOW);
    
    isPowderDispensing[idx] = false; 
    
    Serial.println("powder stopdispense");

  } else {
    Serial.println("unknown powder function");
  }
  return true;
}

bool handleCookerCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cooker) {
    Serial.println("invalid cooker control num");
    return false;
  }
  uint8_t idx = control - 1;

  if (strcmp(func, "startcook") == 0) {
    int water = doc["water"];
    int timer = doc["timer"];
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], HIGH);
      digitalWrite(COOKER_IND_SIG[idx], HIGH);
    }
    Serial.println("cooker startcook");

  } else if (strcmp(func, "stopcook") == 0) {
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], LOW);
      digitalWrite(COOKER_IND_SIG[idx], LOW);
    }
    Serial.println("cooker stopcook");

  } else {
    Serial.println("unknown cooker function");
  }
  return true;
}

bool handleOutletCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.outlet) {
    Serial.println("invalid outlet control num");
    return false;
  }
  uint8_t idx = control - 1;

  if (strcmp(func, "opendoor") == 0) {
    startOutletOpen(idx);
    digitalWrite(OUTLET_REV_OUT[idx], LOW);
    Serial.println("outlet opendoor");

  } else if (strcmp(func, "closedoor") == 0) {
    startOutletClose(idx);
    digitalWrite(OUTLET_FWD_OUT[idx], LOW);
    Serial.println("outlet closedoor");

  } else if (strcmp(func, "stopoutlet") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], LOW);
    digitalWrite(OUTLET_REV_OUT[idx], LOW);
    Serial.println("outlet stopoutlet");

  } else {
    Serial.println("unknown outlet function");
  }
  return true;
}

// =======================================================
// === 4. 메인 파서 (Main Parser)
// =======================================================

bool handleSettingJson(const JsonDocument& doc) {
  Setting next;
  next.cup = doc["cup"] | 0;
  next.ramen = doc["ramen"] | 0;
  next.powder = doc["powder"] | 0;
  next.cooker = doc["cooker"] | 0;
  next.outlet = doc["outlet"] | 0;

  String reason = "";
  if (!validateRules(next, reason)) {
    Serial.println(reason.c_str());
    return false;
  }

  applySetting(next);
  Serial.println("pins configured");
  return true;
}

void checkSensor() {
  
}

bool parseAndDispatch(const char* json) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.println("json parse fail");
    return false;
  }

  const char* dev = doc["device"] | "";

  if (strcmp(dev, "setting") == 0) {
    return handleSettingJson(doc);

  } else if (strcmp(dev, "query") == 0) {
    replyCurrentSetting(current);
    return true;

  } else if (strcmp(dev, "cup") == 0) {
    return handleCupCommand(doc);
  } else if (strcmp(dev, "ramen") == 0) {
    return handleRamenCommand(doc);
  } else if (strcmp(dev, "powder") == 0) {
    return handlePowderCommand(doc);
  } else if (strcmp(dev, "cooker") == 0) {
    return handleCookerCommand(doc);
  } else if (strcmp(dev, "outlet") == 0) {
    return handleOutletCommand(doc);
  } else {
    Serial.println("unsupported device field");
    return false;
  }
}
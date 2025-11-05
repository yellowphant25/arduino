#include <Arduino.h>
#include <ArduinoJson.h>
#include "Protocol.h"  // 자신
#include "config.h"    // 핀맵
#include "state.h"     // 전역 변수(current, state) 사용

// ===== 유틸리티 함수 (응답) =====
void printError(const char* msg) {
  Serial.print(F("{\"ok\":false,\"error\":\""));
  Serial.print(msg);
  Serial.println(F("\"}"));
}

void printOk(const char* what) {
  Serial.print(F("{\"ok\":true,\"msg\":\""));
  Serial.print(what);
  Serial.println(F("\"}"));
}

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

// ===== 핀모드 설정 =====
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

// ===== 설정 적용 및 검증 =====
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
  StaticJsonDocument<1024> doc;
  doc["ok"] = true;
  doc["device"] = "mapping";
  if (s.cup) {
    JsonArray arr = doc.createNestedArray("cup");
    for (uint8_t i=0;i<s.cup;i++){
      JsonObject o = arr.createNestedObject();
      o["id"] = i+1;
      o["motorOut"]= CUP_MOTOR_OUT[i];
      o["rotIn"]   = CUP_ROT_IN[i];
      o["dispIn"]  = CUP_DISP_IN[i];
      o["stockIn"] = CUP_STOCK_IN[i];
      o["currA"]   = CUP_CURR_AIN[i];
    }
  }
  if (s.ramen) {
    JsonArray arr = doc.createNestedArray("ramen");
    for (uint8_t i=0;i<s.ramen;i++){
      JsonObject o = arr.createNestedObject();
      o["id"] = i+1;
      o["upFwd"]   = RAMEN_UP_FWD_OUT[i];
      o["upRev"]   = RAMEN_UP_REV_OUT[i];
      o["ejFwd"]   = RAMEN_EJ_FWD_OUT[i];
      o["ejRev"]   = RAMEN_EJ_REV_OUT[i];
      o["ejTopIn"] = RAMEN_EJ_TOP_IN[i];
      o["ejBtmIn"] = RAMEN_EJ_BTM_IN[i];
      o["upTopIn"] = RAMEN_UP_TOP_IN[i];
      o["upBtmIn"] = RAMEN_UP_BTM_IN[i];
      o["present"] = RAMEN_PRESENT_IN[i];
      o["encInt"]  = RAMEN_ENC_INT[i];
      o["upCurrA"] = RAMEN_UP_CURR_AIN[i];
      o["ejCurrA"] = RAMEN_EJ_CURR_AIN[i];
    }
  }
  if (s.powder) {
    JsonArray arr = doc.createNestedArray("powder");
    for (uint8_t i=0;i<s.powder;i++){
      JsonObject o = arr.createNestedObject();
      o["id"] = i+1;
      o["motorOut"] = POWDER_MOTOR_OUT[i];
      o["currA"]    = POWDER_CURR_AIN[i];
    }
  }
  if (s.outlet) {
    JsonArray arr = doc.createNestedArray("outlet");
    for (uint8_t i=0;i<s.outlet;i++){
      JsonObject o = arr.createNestedObject();
      o["id"] = i+1;
      o["fwdOut"] = OUTLET_FWD_OUT[i];
      o["revOut"] = OUTLET_REV_OUT[i];
      o["openIn"] = OUTLET_OPEN_IN[i];
      o["closeIn"]= OUTLET_CLOSE_IN[i];
      o["currA"]  = OUTLET_CURR_AIN[i];
      o["loadA"]  = OUTLET_LOAD_AIN[i];
      o["usA"]    = OUTLET_USONIC_AIN[i];
    }
  }
  if (s.cooker) {
    JsonArray arr = doc.createNestedArray("cooker");
    for (uint8_t i=0;i<s.cooker;i++){
      JsonObject o = arr.createNestedObject();
      o["id"] = i+1;
      o["indSig"] = COOKER_IND_SIG[i];
      o["wtrSig"] = COOKER_WTR_SIG[i];
      o["currA"]  = COOKER_CURR_AIN[i];
      o["mode"]   = (i<2) ? "OUTPUT" : "INPUT";
    }
  }
  serializeJson(doc, Serial);
  Serial.println();
}

// ===== API 2.x 제어 명령 핸들러 =====
bool handleCupCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cup) { printError("invalid cup control num"); return false; }
  uint8_t idx = control - 1; 

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], HIGH); 
    printOk("cup startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], LOW);
    printOk("cup stopdispense");
  } else { printError("unknown cup function"); }
  return true;
}
bool handleRamenCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.ramen) { printError("invalid ramen control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], HIGH);
    printOk("ramen startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], LOW);
    printOk("ramen stopdispense");
  } else { printError("unknown ramen function"); }
  return true;
}
bool handlePowderCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.powder) { printError("invalid powder control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], HIGH);
    printOk("powder startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], LOW);
    printOk("powder stopdispense");
  } else { printError("unknown powder function"); }
  return true;
}
bool handleCookerCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cooker) { printError("invalid cooker control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "startcook") == 0) {
    int water = doc["water"]; 
    int timer = doc["timer"]; 
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], HIGH); 
      digitalWrite(COOKER_IND_SIG[idx], HIGH); 
    }
    printOk("cooker startcook");
  } else if (strcmp(func, "stopcook") == 0) {
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], LOW);
      digitalWrite(COOKER_IND_SIG[idx], LOW);
    }
    printOk("cooker stopcook");
  } else { printError("unknown cooker function"); }
  return true;
}
bool handleOutletCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.outlet) { printError("invalid outlet control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "opendoor") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], HIGH);
    printOk("outlet opendoor");
  } else if (strcmp(func, "closedoor") == 0) {
    digitalWrite(OUTLET_REV_OUT[idx], HIGH);
    printOk("outlet closedoor");
  } else if (strcmp(func, "stopoutlet") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], LOW);
    digitalWrite(OUTLET_REV_OUT[idx], LOW);
    printOk("outlet stopoutlet");
  } else { printError("unknown outlet function"); }
  return true;
}

// ===== 'setting' 명령 핸들러 =====
bool handleSettingJson(const JsonDocument& doc) {
  Setting next;
  next.cup     = doc["cup"]    | 0;
  next.ramen   = doc["ramen"]  | 0;
  next.powder  = doc["powder"] | 0;
  next.cooker  = doc["cooker"] | 0;
  next.outlet  = doc["outlet"] | 0;

  String why;
  if (!validateRules(next, why)) {
    printError(why.c_str());
    return false;
  }

  applySetting(next);
  printOk("pins configured");
  printMappingSummary(next); // 디버깅 필요시 주석 해제
  return true;
}

// ===== 메인 파서 =====
bool parseAndDispatch(const char* json) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) { printError("json parse fail"); return false; }

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
    printError("unsupported device field");
    return false;
  }
}
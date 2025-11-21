#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include "config.h"

// ============== 1. 현재 설정 상태 (Setting) ==============
struct Setting {
  uint8_t cup     = 0;
  uint8_t ramen   = 0;
  uint8_t powder  = 0;
  uint8_t cooker  = 0;
  uint8_t outlet  = 0;
};

// ============== 2. 현재 센서 상태 (State) ==============
struct State {
  // Cup
  int cup_amp[MAX_CUP] = {0};
  int cup_stock[MAX_CUP] = {0};
  int cup_dispense[MAX_CUP] = {0};
  // Ramen
  int ramen_amp[MAX_RAMEN] = {0};
  int ramen_stock[MAX_RAMEN] = {0};
  int ramen_lift[MAX_RAMEN] = {0};
  int ramen_loadcell[MAX_RAMEN] = {0};
  // Powder
  int powder_amp[MAX_POWDER] = {0};
  int powder_dispense[MAX_POWDER] = {0};
  // Cooker
  int cooker_amp[MAX_COOKER] = {0};
  int cooker_work[MAX_COOKER] = {0};
  // Outlet
  int outlet_amp[MAX_OUTLET] = {0};
  int outlet_door[MAX_OUTLET] = {0};
  int outlet_sonar[MAX_OUTLET] = {0};
  int outlet_loadcell[MAX_OUTLET] = {0};
  // Door
  int door_sensor1 = 0;
  int door_sensor2 = 0;
};

// ===== 전역 변수 선언 (정의는 메인 .ino 파일에) =====
extern Setting current;
extern State state;

// [수정] PPR과 CPR은 const이므로, extern const로 선언합니다.
extern const int PPR;
extern const int CPR; 

// [수정] 모든 변수의 extern 선언을 통일합니다.
extern volatile long encoderCount;
extern volatile int direction;
extern unsigned long lastEncoderReportTime; // 52줄 수정됨
extern long lastCount;

// ===== 엔코더 관련 설정 =====
// [수정] ENCODER_A/B_PIN은 const이므로, extern const로 선언합니다.
extern const int ENCODER_A_PIN;
extern const int ENCODER_B_PIN;

#endif // STATE_H
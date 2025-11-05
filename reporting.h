#ifndef REPORTING_H
#define REPORTING_H

// 0.1초마다 센서 값을 읽어 'state' 전역 변수에 저장
void readAllSensors();

// 'state' 변수의 값을 JSON으로 PC에 전송
void publishStateJson();

#endif // REPORTING_H
#ifndef LOG_PARAMS_H
#define LOG_PARAMS_H

#include "../globals/global_types.h" // Counter 타입을 사용하기 위해 필요

// 전역 변수를 '선언'합니다.
// 실제 메모리 할당은 .c 파일에서 이루어집니다.
extern Counter log_start_cycle;
extern Counter log_end_cycle;

#endif // LOG_PARAMS_H

//올바른 변수 선언 및 정의 방법
//1단계: 헤더 파일 (.h) 에는 extern으로 변수를 선언
//새로운 헤더 파일 (예: log_params.h)을 만들고, 변수를 **선언(declaration)**만 합니다. extern은 "이 변수는 다른 어딘가에 실제로 존재하니, 여기서는 메모리를 할당하지 말고 이름만 알아둬"라는 의미입니다.
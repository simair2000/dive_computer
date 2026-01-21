
#ifndef BUHLMANN_H
#define BUHLMANN_H

#ifndef bool
typedef enum { false, true } bool;
#endif

#define NUM_COMPARTMENTS 16
#define WATER_VAPOR_PRESSURE 0.0627f // 폐 속 수증기압 (bar) - 정밀 계산용

// ZHL-16C 질소 반감기 (단위: 분)
const float N2_Half_Lives[NUM_COMPARTMENTS] = {
    4.0f, 5.0f, 8.0f, 12.5f, 18.5f, 27.0f, 38.3f, 54.3f, 
    77.0f, 109.0f, 146.0f, 187.0f, 239.0f, 305.0f, 390.0f, 635.0f
};

// a 계수
const float A_Coefficients[NUM_COMPARTMENTS] = {
    1.2599f, 1.0000f, 0.8618f, 0.7562f, 0.6667f, 0.5600f, 0.4947f, 0.4500f,
    0.4187f, 0.3798f, 0.3497f, 0.3223f, 0.2850f, 0.2737f, 0.2523f, 0.2327f
};

// b 계수
const float B_Coefficients[NUM_COMPARTMENTS] = {
    0.5050f, 0.5533f, 0.6122f, 0.6626f, 0.7004f, 0.7541f, 0.7957f, 0.8279f, 
    0.8491f, 0.8732f, 0.8910f, 0.9092f, 0.9222f, 0.9319f, 0.9508f, 0.9650f
};

typedef struct {
    float fraction_o2; // 산소 비율
    float fraction_he; // 헬륨 비율
    float fraction_n2; // 질소 비율
    int max_depth;   // 최대 깊이 (MOD)
} GasMix;

// 결과 값을 담을 구조체
typedef struct {
    int stop_depth; // 정지해야 할 수심 (m)
    int stop_time;  // 정지해야 할 시간 (분)
    bool is_deco;   // 감압 모드 여부
} DecoPlan;


void Init_Loadings(float current_loadings[]);
void Update_N2_Loadings(float current_loadings[], float depth_meters, float fraction_o2, float interval_sec);

#endif // BUHLMANN_H
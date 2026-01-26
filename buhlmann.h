
#ifndef BUHLMANN_H
#define BUHLMANN_H

#ifndef bool
typedef enum { false, true } bool;
#endif

#define NUM_COMPARTMENTS 16
#define WATER_VAPOR_PRESSURE 0.0627f // 폐 속 수증기압 (bar) - 정밀 계산용


extern float N2_Half_Lives[];
extern float A_Coefficients[];
extern float B_Coefficients[];

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
float Calculate_NDL(float depth_meters, float current_loadings[], float GF_Hi, float PO2);
bool Is_Depth_Safe(float depth_to_check, float loadings[], float gf_low);
DecoPlan Calculate_Deco_Stop(float current_loadings[], float GF_Low, float GF_High, float PO2);
int Calculate_TTS(float current_depth_m, float current_loadings[], float GF_Low, float GF_High, float fraction_o2);

#endif // BUHLMANN_H
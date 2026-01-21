#include <stdio.h>
#include <time.h>
#include <conio.h>

int main() {
    printf("Main loop started. Press 'q' to exit.\n");
    
    time_t lastTime = time(NULL);
    int running = 1;
    
    while (running) {
        time_t currentTime = time(NULL);
        
        // 1초마다 루프 실행
        if (currentTime - lastTime >= 1) {
            printf("Loop iteration at %ld\n", currentTime);
            lastTime = currentTime;
        }
        
        // 키보드 입력 확인 (non-blocking)
        if (_kbhit()) {
            int key = _getch();
            printf("Key pressed: %c\n", key);
            
            if (key == 'q' || key == 'Q') {
                printf("Exiting loop...\n");
                running = 0;
            }
        }
    }
    
    printf("Program ended.\n");
    return 0;
}
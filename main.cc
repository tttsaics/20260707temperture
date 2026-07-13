#include <iostream>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include "thermal.hpp"

int main() {
    Thermal thermalSensor;
    
#ifdef _WIN32
    _mkdir("output");
#else
    mkdir("output", 0777);
#endif
    int frameCount = 0;

    //使用 while 迴圈持續讀取，直到 data.bin 檔案結束為止
    while (thermalSensor.readFrame()) {
        //每次迴圈皆呼叫 parserFrame 來解析當前的 frame 數值
        thermalSensor.parserFrame();
        
        //以類似 JSONL 的格式輸出 (依照需求掠過 timestamp 欄位)
        std::cout << "{\"ta\": " << thermalSensor.ta 
                  << ", \"max_temp\": " << thermalSensor.maxTemp 
                  << ", \"min_temp\": " << thermalSensor.minTemp << "}" << std::endl;

        //階段二實作：將每一訊框轉換並儲存為灰階熱點圖
        char filename[64];
        std::sprintf(filename, "output/frame_%03d.bmp", frameCount);
        thermalSensor.saveFrame(filename);
        frameCount++;
    }

    return 0;
}
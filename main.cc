#include <iostream>
#ifdef _WIN32
#include <direct.h>   // [更正2] Windows 平台創建資料夾
#else
#include <sys/stat.h>  // [更正2] Linux/macOS 平台創建資料夾
#endif
#include "thermal.hpp"

int main() {
    Thermal thermalSensor;
    
    // [更正2] 建立儲存熱點圖的 output 目錄 (相容舊版 C++)
#ifdef _WIN32
    _mkdir("output");
#else
    mkdir("output", 0777);
#endif
    int frameCount = 0; // [更正2] 用於計算訊框序號以便儲存圖檔

    // [更動] 使用 while 迴圈持續讀取，直到 data.bin 檔案結束為止
    while (thermalSensor.readFrame()) {
        // [更動] 每次迴圈皆呼叫 parserFrame 來解析當前的 frame 數值
        thermalSensor.parserFrame();
        
        // [更動] 以類似 JSONL 的格式輸出 (依照需求掠過 timestamp 欄位)
        std::cout << "{\"ta\": " << thermalSensor.ta 
                  // [更動] 輸出最高溫度 max_temp 欄位
                  << ", \"max_temp\": " << thermalSensor.maxTemp 
                  // [更動] 輸出最低溫度 min_temp 欄位
                  << ", \"min_temp\": " << thermalSensor.minTemp << "}" << std::endl;

        // [更正2] 階段二實作：將每一訊框轉換並儲存為色彩熱點圖
        char filename[64];
        std::sprintf(filename, "output/frame_%03d.bmp", frameCount);
        thermalSensor.saveFrame(filename);
        frameCount++;
    }

    return 0;
}
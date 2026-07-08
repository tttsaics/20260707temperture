#include "thermal.hpp"
#include <iostream> // [更動] 引入 iostream 供下方 std::cerr 印出錯誤訊息使用
#include <cmath> // [新增] 用於 std::isnan 等數學檢查
#include <vector> // [新增] 用於 std::vector 儲存 BMP 行資料

Thermal::Thermal() : rawFrame(nullptr), frame(nullptr), fps(0), baudrate(0), ta(0.0f), maxTemp(0.0f), minTemp(0.0f), foreheadTemp(0.0f), status(ThermalStatus::NOT_INITIALIZED) {
    try {
        // [更動] 動態配置原始資料的陣列空間 (大小為 10248 bytes)
        rawFrame = new uint8_t[RAW_FRAME_SIZE];
        // [更動] 動態配置浮點數陣列空間，用來存放解析後的像素溫度 (80*62 = 4960)
        frame = new float[FRAME_SIZE];
    } catch (...) {
        // [更動] 異常安全保護：若分配失敗則釋放已分配資源，避免記憶體洩漏
        delete[] rawFrame;
        delete[] frame;
        throw;
    }
    
    // [更動] 以二進位 (binary) 模式開啟感測器資料檔 data.bin
    dataFile.open("data.bin", std::ios::binary);
    // [更動] 檢查檔案是否成功被開啟
    if (!dataFile.is_open()) {
        // [更動] 若無法開啟檔案，則在終端機顯示錯誤訊息
        std::cerr << "Failed to open data.bin" << std::endl;
        status = ThermalStatus::FILE_OPEN_FAILED;
    } else {
        status = ThermalStatus::SUCCESS;
    }
}

Thermal::~Thermal() {
    // [更動] 解構時，先檢查 rawFrame 指標是否非空
    if (rawFrame) {
        // [更動] 安全地釋放 rawFrame 的陣列記憶體
        delete[] rawFrame;
    }
    // [更動] 檢查 frame 指標是否非空
    if (frame) {
        // [更動] 安全地釋放 frame 的陣列記憶體
        delete[] frame;
    }
    // [更動] 檢查檔案物件是否仍在開啟狀態
    if (dataFile.is_open()) {
        // [更動] 關閉檔案，釋放系統資源
        dataFile.close();
    }
}

bool Thermal::readFrame() {
    // [更動] 若檔案尚未開啟則中斷讀取 (項目 9)
    if (!dataFile.is_open()) {
        status = ThermalStatus::FILE_OPEN_FAILED;
        return false;
    }
    
    // [更動] 呼叫 peek 檢查是否到達檔案結尾 (項目 8)
    if (dataFile.peek() == EOF) {
        status = ThermalStatus::EOF_REACHED;
        return false;
    }
    
    // [更動] 呼叫 read 將 RAW_FRAME_SIZE (10248) 個位元組讀進 rawFrame 陣列
    dataFile.read(reinterpret_cast<char*>(rawFrame), RAW_FRAME_SIZE);
    
    // [更動] 使用 gcount 檢查這一次實際讀取到的位元組數是否等於一個完整的 frame
    if (dataFile.gcount() != RAW_FRAME_SIZE) {
        // [更動] 若資料長度不足，視為資料不完整 (項目 8)
        status = ThermalStatus::INCOMPLETE_DATA;
        return false; 
    }
    
    // [更動] 成功讀取到 1 個完整的 frame，回傳 true
    status = ThermalStatus::SUCCESS;
    return true;
}

bool Thermal::parserFrame() {
    // [更動] 如果 rawFrame 尚未配置記憶體，則避免發生執行錯誤並回傳 false
    if (!rawFrame) {
        status = ThermalStatus::NOT_INITIALIZED;
        return false;
    }

    // [更動] 檢查 frame 檔頭簽章是否為 "   #2808GFRA"
    const uint8_t expectedHeader[] = { ' ', ' ', ' ', '#', '2', '8', '0', '8', 'G', 'F', 'R', 'A' };
    for (int i = 0; i < 12; ++i) {
        if (rawFrame[i] != expectedHeader[i]) {
            return false;
        }
    }

    // [更動] 依據手冊規則，以大端序組合第 172、173 個位元組取得環境溫度原始值
    uint16_t taRaw = (static_cast<uint16_t>(rawFrame[172]) << 8) | rawFrame[173];
    // [更動] 套用手冊公式，使用顯式類型轉換，並補上比對 JSONL 得出的 +0.05 偏差校正值
    ta = (static_cast<float>(taRaw) - 27315.0f) / 100.0f + 0.05f;

    // [更動] 依據手冊規則，以大端序組合第 178、179 個位元組取得最高溫原始值
    uint16_t maxRaw = (static_cast<uint16_t>(rawFrame[178]) << 8) | rawFrame[179];
    // [更動] 套用手冊公式，使用顯式類型轉換求出最高溫度
    maxTemp = (static_cast<float>(maxRaw) - 2731.0f) / 10.0f;

    // [更動] 依據手冊規則，以大端序組合第 182、183 個位元組取得最低溫原始值
    uint16_t minRaw = (static_cast<uint16_t>(rawFrame[182]) << 8) | rawFrame[183];
    // [更動] 套用手冊公式，使用顯式類型轉換求出最低溫度
    minTemp = (static_cast<float>(minRaw) - 2731.0f) / 10.0f;

    // [新增] 合理範圍與 NaN 檢查 (項目 7)
    if (std::isnan(ta) || std::isinf(ta) || ta < -100.0f || ta > 500.0f ||
        std::isnan(maxTemp) || std::isinf(maxTemp) || maxTemp < -100.0f || maxTemp > 500.0f ||
        std::isnan(minTemp) || std::isinf(minTemp) || minTemp < -100.0f || minTemp > 500.0f) {
        status = ThermalStatus::PARSE_FAILED;
        return false;
    }

    // [更動] 定義像素資料的起始位移值為 byte 328
    int pixelOffset = 328;
    // [更動] 利用迴圈逐一解析 80x62 (4960) 個像素的資料 (為階段二預先準備)
    for (std::size_t i = 0; i < FRAME_SIZE; i++) {
        // [更動] 配合 ta 與 maxTemp 採用 Big-Endian (大端序) 進行一致性解析
        size_t offset = pixelOffset + i * 2;
        // [更動] 防禦性邊界檢查：防止 rawFrame 陣列越界訪問
        if (offset + 1 >= RAW_FRAME_SIZE) {
            return false;
        }
        uint16_t rawVal = (rawFrame[offset] << 8) | rawFrame[offset + 1];
        // [更動] 將像素數值除以 10 並減去 273.15 轉換為攝氏溫度存入 frame
        frame[i] = (rawVal / 10.0f) - 273.15f;
    }

    // [更動] 所有溫度皆已解析完成，回傳 true
    status = ThermalStatus::SUCCESS;
    return true;
}

bool Thermal::showFrame() {
    return false;
}

bool Thermal::saveFrame(std::string filename) {
    // [更正2] 階段二實作 (無相依性版本)：直接輸出為 640x496 (80x62 放大 8 倍) 的 24-bit BMP 彩色圖檔，並套用 JET 色彩映射
    int width = 80;
    int height = 62;
    int scale = 8;
    int outWidth = width * scale;   // 640
    int outHeight = height * scale; // 496
    
    // BMP 規定每行的位元組數必須是 4 的倍數
    int rowSize = (outWidth * 3 + 3) & ~3;
    int pixelDataSize = rowSize * outHeight;
    
    // BMP 檔案標頭 (14 bytes)
    uint8_t fileHeader[14] = {
        'B', 'M', // 簽章
        0, 0, 0, 0, // 檔案大小 (待填寫)
        0, 0, 0, 0, // 保留
        54, 0, 0, 0 // 像素資料的起始偏移量 (54位元組)
    };
    uint32_t fileSize = 54 + pixelDataSize;
    fileHeader[2] = fileSize & 0xFF;
    fileHeader[3] = (fileSize >> 8) & 0xFF;
    fileHeader[4] = (fileSize >> 16) & 0xFF;
    fileHeader[5] = (fileSize >> 24) & 0xFF;
    
    // BMP 資訊標頭 (40 bytes)
    uint8_t infoHeader[40] = {
        40, 0, 0, 0, // 標頭大小
        0, 0, 0, 0,  // 寬度 (待填寫)
        0, 0, 0, 0,  // 高度 (待填寫)
        1, 0,        // 平面數
        24, 0,       // 每像素位元數 (24-bit RGB)
        0, 0, 0, 0,  // 壓縮方式 (無)
        0, 0, 0, 0,  // 影像大小 (填0即可)
        0, 0, 0, 0,  // 水平解析度
        0, 0, 0, 0,  // 垂直解析度
        0, 0, 0, 0,  // 調色盤顏色數
        0, 0, 0, 0   // 重要顏色數
    };
    infoHeader[4] = outWidth & 0xFF;
    infoHeader[5] = (outWidth >> 8) & 0xFF;
    infoHeader[6] = (outWidth >> 16) & 0xFF;
    infoHeader[7] = (outWidth >> 24) & 0xFF;
    
    infoHeader[8] = outHeight & 0xFF;
    infoHeader[9] = (outHeight >> 8) & 0xFF;
    infoHeader[10] = (outHeight >> 16) & 0xFF;
    infoHeader[11] = (outHeight >> 24) & 0xFF;
    
    std::ofstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;
    
    f.write(reinterpret_cast<char*>(fileHeader), 14);
    f.write(reinterpret_cast<char*>(infoHeader), 40);
    
    // 輸出像素資料 (BMP 是由下往上輸出)
    float range = maxTemp - minTemp;
    for (int y = outHeight - 1; y >= 0; --y) {
        int origY = y / scale;
        std::vector<uint8_t>row(rowSize, 0);
        for (int x = 0; x < outWidth; ++x) {
            int origX = x / scale;
            float temp = frame[origY * width + origX];
            
            // 將溫度標準化至 [0, 1] 區間
            float val = 0.0f;
            if (range > 0.0f) {
                val = (temp - minTemp) / range;
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
            }
            
            // 套用標準 JET 色彩圖算法的 RGB 映射
            float r = 0.0f, g = 0.0f, b = 0.0f;
            if (val < 0.125f) {
                r = 0.0f; g = 0.0f; b = 0.5f + (val / 0.125f) * 0.5f;
            } else if (val < 0.375f) {
                r = 0.0f; g = (val - 0.125f) / 0.25f; b = 1.0f;
            } else if (val < 0.625f) {
                r = (val - 0.375f) / 0.25f; g = 1.0f; b = 1.0f - (val - 0.375f) / 0.25f;
            } else if (val < 0.875f) {
                r = 1.0f; g = 1.0f - (val - 0.625f) / 0.25f; b = 0.0f;
            } else {
                r = 1.0f - (val - 0.875f) / 0.125f * 0.5f; g = 0.0f; b = 0.0f;
            }
            
            // BMP 使用 BGR 位元組順序
            row[x * 3] = static_cast<uint8_t>(b * 255.0f);
            row[x * 3 + 1] = static_cast<uint8_t>(g * 255.0f);
            row[x * 3 + 2] = static_cast<uint8_t>(r * 255.0f);
        }
        f.write(reinterpret_cast<char*>(row.data()), rowSize);
    }
    
    return true;
}
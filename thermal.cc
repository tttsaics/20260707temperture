#include "thermal.hpp"
#include <iostream> 
#include <cmath> 
#include <vector>

Thermal::Thermal() : rawFrame(nullptr), frame(nullptr), fps(0), baudrate(0), ta(0.0f), maxTemp(0.0f), minTemp(0.0f), foreheadTemp(0.0f), status(ThermalStatus::NOT_INITIALIZED) {
    try {
        rawFrame = new uint8_t[RAW_FRAME_SIZE];
        frame = new float[FRAME_SIZE];
    } catch (...) {
        delete[] rawFrame;
        delete[] frame;
        throw;
    }
    
    dataFile.open("data.bin", std::ios::binary);
    if (!dataFile.is_open()) {
        std::cerr << "Failed to open data.bin" << std::endl;
        status = ThermalStatus::FILE_OPEN_FAILED;
    } else {
        status = ThermalStatus::SUCCESS;
    }
}

Thermal::~Thermal() {
    if (rawFrame) {
        delete[] rawFrame;
    }
    if (frame) {
        delete[] frame;
    }
    if (dataFile.is_open()) {
        dataFile.close();
    }
}

bool Thermal::readFrame() {
    if (!dataFile.is_open()) {
        status = ThermalStatus::FILE_OPEN_FAILED;
        return false;
    }
    
    if (dataFile.peek() == EOF) {
        status = ThermalStatus::EOF_REACHED;
        return false;
    }
    
    dataFile.read(reinterpret_cast<char*>(rawFrame), RAW_FRAME_SIZE);
    
    if (dataFile.gcount() != RAW_FRAME_SIZE) {
        status = ThermalStatus::INCOMPLETE_DATA;
        return false; 
    }
    
    status = ThermalStatus::SUCCESS;
    return true;
}

bool Thermal::parserFrame() {
    if (!rawFrame) {
        status = ThermalStatus::NOT_INITIALIZED;
        return false;
    }


    const uint8_t expectedHeader[] = { ' ', ' ', ' ', '#', '2', '8', '0', '8', 'G', 'F', 'R', 'A' };
    for (int i = 0; i < 12; ++i) {
        if (rawFrame[i] != expectedHeader[i]) {
            return false;
        }
    }

    uint16_t taRaw = (static_cast<uint16_t>(rawFrame[172]) << 8) | rawFrame[173];
    ta = (static_cast<float>(taRaw) - 27315.0f) / 100.0f + 0.05f;

    uint16_t maxRaw = (static_cast<uint16_t>(rawFrame[178]) << 8) | rawFrame[179];
    maxTemp = (static_cast<float>(maxRaw) - 2731.0f) / 10.0f;

    uint16_t minRaw = (static_cast<uint16_t>(rawFrame[182]) << 8) | rawFrame[183];
    minTemp = (static_cast<float>(minRaw) - 2731.0f) / 10.0f;

    if (std::isnan(ta) || std::isinf(ta) || ta < -100.0f || ta > 500.0f ||
        std::isnan(maxTemp) || std::isinf(maxTemp) || maxTemp < -100.0f || maxTemp > 500.0f ||
        std::isnan(minTemp) || std::isinf(minTemp) || minTemp < -100.0f || minTemp > 500.0f) {
        status = ThermalStatus::PARSE_FAILED;
        return false;
    }

    int pixelOffset = 328;
    for (std::size_t i = 0; i < FRAME_SIZE; i++) {
        size_t offset = pixelOffset + i * 2;
        if (offset + 1 >= RAW_FRAME_SIZE) {
            return false;
        }
        uint16_t rawVal = (rawFrame[offset] << 8) | rawFrame[offset + 1];
        frame[i] = (rawVal / 10.0f) - 273.15f;
    }

    status = ThermalStatus::SUCCESS;
    return true;
}

bool Thermal::showFrame() {
    return false;
}

bool Thermal::saveFrame(std::string filename) {
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
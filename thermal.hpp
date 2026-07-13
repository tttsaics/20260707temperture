#ifndef THERMAL_HPP
#define THERMAL_HPP

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

// [新增] 明確的錯誤狀態機制
enum class ThermalStatus {
    SUCCESS,
    EOF_REACHED,
    INCOMPLETE_DATA,
    FILE_OPEN_FAILED,
    PARSE_FAILED,
    NOT_INITIALIZED,
    NOT_IMPLEMENTED
};

constexpr std::size_t RAW_FRAME_SIZE = 10248;
constexpr std::size_t FRAME_SIZE = 80 * 62; // 解析度為 80 * 62 pixel

class Thermal
{
    // 原始 frame ，包含 metadata
    uint8_t *rawFrame = nullptr;
    // 經過轉換後的各pixel數值
    float * frame = nullptr;
    // Serial連線資訊
    int fps, baudrate;
    std::string portName;
    // 欲開啟的檔案物件 "data.bin"
    std::ifstream dataFile;
    
public:
    // 環境溫度，最高溫度，最低溫度，額溫(棄用)
    float ta, maxTemp, minTemp, foreheadTemp;
    ThermalStatus status; // [新增] 儲存當前狀態
    explicit Thermal();
    ~Thermal();

    // [新增] 避免物件被複製或移動導致動態記憶體重複釋放
    Thermal(const Thermal&) = delete;
    Thermal& operator=(const Thermal&) = delete;
    Thermal(Thermal&&) = delete;
    Thermal& operator=(Thermal&&) = delete;

    // 從 data.bin 檔案讀出一個 frame
    bool readFrame();
    // 解析frame資訊
    bool parserFrame();

    // 呈現 frame，要搭配 opencv
    bool showFrame();
    // 儲存 frame
    bool saveFrame(std::string);
};

#endif

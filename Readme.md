# 熱成像解析程式

- 資料描述
    - data.bin: frame檔案，每 10248 bytes 表一 frame
    - temperature.jsonl: 表每一個 frame 解析後的資料
    - thermal.hpp: Thermal物件原型宣告
    - thermal.cc: Thermal物件定義
    - main.cc: 主程式，Thermal 物件的使用方法
    - heatmap.mp4: 熱成像結果

## 階段一
    - 寫出呈現ta, minTemp, maxTemp的數值

## 階段二
    - 將每一 frame 存下成圖片檔案

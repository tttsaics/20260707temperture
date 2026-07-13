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

    while (thermalSensor.readFrame()) {
        thermalSensor.parserFrame();

        std::cout << "{\"ta\": " << thermalSensor.ta 
                  << ", \"max_temp\": " << thermalSensor.maxTemp 
                  << ", \"min_temp\": " << thermalSensor.minTemp << "}" << std::endl;

        char filename[64];
        std::sprintf(filename, "output/frame_%03d.bmp", frameCount);
        thermalSensor.saveFrame(filename);
        frameCount++;
    }

    return 0;
}
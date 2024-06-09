#pragma once
#include <ofImage.h>
#include <chrono>
#include <filesystem>
#include <iostream>
  
namespace fs = std::filesystem;
  
template <class T>
class ofAutoImage : public ofImage_<T> {
public:
    void loadAuto(std::string name) {
        name = fs::absolute(name).string();        
        setFilename(name);
        ofAddListener(ofEvents().update, this, &ofAutoImage::update);
    }
    
    void setFilename(std::string name) {
        this->name = name;
        ofEventArgs args;
        update(args);
    }
      
    void update(ofEventArgs &args) {
        reload();
    }
    
    void reload(bool force=false) {
        fs::path file(name);
        if(fs::exists(file)) {
            auto ftime = fs::last_write_time(file);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t timestamp = std::chrono::system_clock::to_time_t(sctp);
            if(force || timestamp != lastTimestamp) {
                ofSleepMillis(100);
                if(!ofImage_<T>::load(name)) {
                    std::cout << "[ofAutoImage] Exists, but error loading: " << name << std::endl;
                } else {
                    std::cout << "[ofAutoImage] Timestamp for " << name << ": " << timestamp << std::endl;
                    lastTimestamp = timestamp;
                }
            }
        } else {
            std::cout << "[ofAutoImage] Does not exist: " << name << std::endl;
        }
    }
  
private:
    std::string name = "";
    std::time_t lastTimestamp = 0;
};  

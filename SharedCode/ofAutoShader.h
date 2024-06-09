#pragma once
#include <ofShader.h>
#include <chrono>
#include <filesystem>
#include <iostream>
  
namespace fs = std::filesystem;
  
class ofAutoShader : public ofShader {
public:
    void loadAuto(std::string name) {
        name = fs::absolute(name).string();     
        this->name = name;
        ofEventArgs args;
        update(args);
        ofAddListener(ofEvents().update, this, &ofAutoShader::update);
    }
    
    void update(ofEventArgs &args) {
        bool needsReload = false;
              
        std::string fragName = name + ".frag";
        fs::path fragFile(fragName);
        if (fs::exists(fragFile)) {
            auto ftime = fs::last_write_time(fragFile);
            auto fragTimestamp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t fragTime = std::chrono::system_clock::to_time_t(fragTimestamp);
            if(fragTime != lastFragTimestamp) {
                std::cout << "[ofAutoShader] Timestamp for " << fragName << ": " << fragTime << std::endl;
                needsReload = true;
                lastFragTimestamp = fragTime;
            }
        } else {
            fragName = "";
        }
          
        std::string vertName = name + ".vert";
        fs::path vertFile(vertName);
        if (fs::exists(vertFile)) {
            auto vtime = fs::last_write_time(vertFile);
            auto vertTimestamp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(vtime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t vertTime = std::chrono::system_clock::to_time_t(vertTimestamp);
            if(vertTime != lastVertTimestamp) {
                std::cout << "[ofAutoShader] Timestamp for " << vertName << ": " << vertTime << std::endl;
                needsReload = true;
                lastVertTimestamp = vertTime;
            }
        } else {
            vertName = "";
        }
  
        if (needsReload) {
            ofSleepMillis(100);
            ofShader::load(vertName, fragName);
            ready = checkReady();
        }
    }
    
    bool isReady() {
        return ready;
    }
  
protected:
    bool checkReady() {
        GLuint program = getProgram();
        GLint status;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        GLuint err = glGetError();
        return err == GL_NO_ERROR && status != GL_FALSE;
    }
  
private:
    std::string name;
    std::time_t lastFragTimestamp = 0;
    std::time_t lastVertTimestamp = 0;
    bool ready = false;
};  

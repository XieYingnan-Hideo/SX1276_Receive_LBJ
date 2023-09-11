//
// Created by FLN1021 on 2023/9/5.
//

#ifndef PAGER_RECEIVE_SDLOG_H
#define PAGER_RECEIVE_SDLOG_H

#include <SD.h>
#include <FS.h>
#include <Arduino.h>
#include <ctime>
#include "utilities.h"
#define MAX_LOG_SIZE 5000000

class SD_LOG{
public:
    explicit SD_LOG(fs::FS &fs);
    int begin(const char* path);
    void append(const char* format, ...);
    // todo add csv file.
    File logFile(char op);
    void reopen();
    bool status();

private:
    void getFilename(const char* path);
    void writeHeader();
    String log_path;
    fs::FS* filesys;
    File log;
    char filename[32] = "";
    bool sd_log = false;
    bool have_sd = false;
    bool haveNTP = false;
    bool is_newfile = false;
    bool is_startline = true;
    const char * log_directory{};

    struct tm timein{};
};


#endif //PAGER_RECEIVE_SDLOG_H

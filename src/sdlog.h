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
#include "ESPTelnet.h"
#include "boards.h"
#define MAX_LOG_SIZE 500000
#define MAX_CSV_SIZE 500000

class SD_LOG{
public:
    explicit SD_LOG(fs::FS &fs);
    int begin(const char* path);
    int begincsv(const char* path);
    void getFilenameCSV(const char* path);
    void append(const char* format, ...);
    void appendCSV(const char *format, ...);
    void printTel(int chars, ESPTelnet& tel);
    // todo add csv file.
    File logFile(char op);
    void reopen();
    bool status();

private:
    void getFilename(const char* path);
    void writeHeader();
    void writeHeaderCSV();
    String log_path;
    String csv_path;
    fs::FS* filesys;
    File log;
    File csv;
    int log_count = 0; // Actual file count - 1.
    char filename[32] = "";
    char filename_csv[32] = "";
    bool sd_log = false;
    bool sd_csv = false;
    bool have_sd = false;
    bool haveNTP = false;
    bool is_newfile = false;
    bool is_startline = true;
    bool is_newfile_csv = false;
    bool is_startline_csv = true;
    const char * log_directory{};
    const char * csv_directory{};

    struct tm timein{};
};


#endif //PAGER_RECEIVE_SDLOG_H

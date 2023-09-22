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

#define MAX_LOG_SIZE 500000 // 500000 default
#define MAX_CSV_SIZE 500000

class SD_LOG {
public:
    explicit SD_LOG(fs::FS &fs);

    static int begin(const char *path);

    static int beginCSV(const char *path);

    static void getFilenameCSV(const char *path);

    static void append(const char *format, ...);

    static void appendCSV(const char *format, ...);

    static void printTel(int chars, ESPTelnet &tel);

    static File logFile(char op);

    static void reopen();

    static void disableSizeCheck();

    static void enableSizeCheck();

    static bool status();

private:
    static void getFilename(const char *path);

    static void writeHeader();

    static void writeHeaderCSV();

    static String log_path;
    static String csv_path;
    static fs::FS *filesys;
    static File log;
    static File csv;
    static int log_count; // Actual file count - 1. =0 default
    static char filename[32]; // =""
    static char filename_csv[32];
    static bool sd_log; // = false
    static bool sd_csv;
//    bool have_sd = false;
//    bool haveNTP = false;
    static bool is_newfile; // = false
    static bool is_startline; // = true
    static bool size_checked;
    static bool is_newfile_csv;
    static bool is_startline_csv;
    static const char *log_directory;
    static const char *csv_directory;

    static struct tm timein;
};


#endif //PAGER_RECEIVE_SDLOG_H

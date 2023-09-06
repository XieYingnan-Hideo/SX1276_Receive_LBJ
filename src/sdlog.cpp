//
// Created by FLN1021 on 2023/9/5.
//

#include "sdlog.h"

SD_LOG::SD_LOG(fs::FS &fs) {
    filesys = &fs;
}
void SD_LOG::getFilename(const char* path){
    File cwd = filesys->open(path,FILE_READ,false);
    if (!cwd){
        filesys->mkdir(path);
        cwd = filesys->open(path,FILE_READ,false);
        if (!cwd){
            Serial.println("[SDLOG] Failed to open log directory!");
            Serial.println("[SDLOG] Will not write log to SD card.");
            sd_log = false;
            return;
        }
    }
    if (!cwd.isDirectory()){
        Serial.println("[SDLOG] log directory error!");
        sd_log = false;
        return;
    }
    char last[32];
    int counter = 0;
    while (cwd.openNextFile()){
        counter++;
    }
    sprintf(last,"LOG_%04d.txt",counter-1);
    String last_path = String(String(path) +"/"+ String(last));
    if (!filesys->exists(last_path)){
        sprintf(last,"LOG_%04d.txt",counter-1);
        last_path = String(String(path) +"/"+ String(last));
    }
    File last_log = filesys->open(last_path);

    if(last_log.size() <= MAX_LOG_SIZE && counter > 0) {
        sprintf(filename, "%s", last_log.name());
    }
    else{
        sprintf(filename,"LOG_%04d.txt",counter);
        is_newfile = true;
    }
    Serial.printf("[SDLOG] %d log files, using %s \n",counter,filename);
    sd_log = true;
    return;
}
int SD_LOG::begin(const char* path){
    log_directory = path;
    getFilename(path);
    if (!sd_log)
        return -1;
    log_path = String(String(path) + '/' + filename);
    log = filesys->open(log_path,"a",true);
    if (!log){
        Serial.println("[SDLOG] Failed to open log file!");
        Serial.println("[SDLOG] Will not write log to SD card.");
        sd_log = false;
        return -1;
    }
    writeHeader();
    sd_log = true;
    return 0;
}

void SD_LOG::writeHeader(){
    log.println("-------------------------------------------------");
    if(is_newfile){
        log.printf("ESP32 DEV MODULE LOG FILE %s \n",filename);
    }
    log.printf("BEGIN OF SYSTEM LOG, STARTUP TIME %lu MS.\n",millis());
    if (getLocalTime(&timein,0))
        log.printf("CURRENT TIME %d-%02d-%02d %02d:%02d:%02d\n",
               timein.tm_year+1900,timein.tm_mon+1, timein.tm_mday,timein.tm_hour , timein.tm_min, timein.tm_sec);
    log.println("-------------------------------------------------");
    log.flush();
    return;
}

void SD_LOG::append(const char* format, ...){
    if(!sd_log){
        return;
    }
    if(!filesys->exists(log_path)){
        Serial.println("[SDLOG] Log file unavailable!");
        sd_log = false;
        SD.end();
        return;
    }
    if (log.size()>=MAX_LOG_SIZE){
        log.close();
        begin(log_directory);
    }
    char buffer[256];
    va_list args;
    va_start(args,format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (is_startline){
        if (getLocalTime(&timein,0)){
            log.printf("%d-%02d-%02d %02d:%02d:%02d > ",timein.tm_year+1900,timein.tm_mon+1,
                       timein.tm_mday,timein.tm_hour , timein.tm_min, timein.tm_sec);
        } else {
            log.printf("[%6lu.%03lu] > ", millis()/1000,millis()%1000);
        }
        is_startline = false;
    }
    if (NULL != strchr(format,'\n')) /* detect end of line in stream */
        is_startline = true;
    log.print(buffer);
    log.flush();
    return;
}


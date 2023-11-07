//
// Created by FLN1021 on 2023/10/31.
//

#ifndef PAGER_RECEIVE_COREDUMP_H
#define PAGER_RECEIVE_COREDUMP_H

#include "networks.h"
#include "esp_core_dump.h"
#include "sdlog.h"

extern bool have_cd;

void readCoreDump();

#endif //PAGER_RECEIVE_COREDUMP_H

//
// Created by zj on 7/31/2024.
//

#pragma once

#include "tools.h"
#include "constants.h"

struct CDB_Input {
    Wire<ROB_SIZE_LOG> rob_id; // 0 means invalid
    Wire<32>           value;
};

struct CDB_Output {
    Register<ROB_SIZE_LOG> rob_id; // 0 means invalid
    Register<32>           value;
};
struct Commit_Info {
    Wire<ROB_SIZE_LOG> rob_id; // 0 means disabled
};

#pragma once
#include <string>

typedef struct settings_t {
    std::string pdb_path;
} settings_t;

settings_t nc_settings();
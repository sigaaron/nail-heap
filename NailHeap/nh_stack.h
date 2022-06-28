#pragma once
#include <string>

int         nh_stack_init( const char* pdb_file_path_ );
std::string nh_stack_trace_ps( long pid_ );
std::string nh_stack_trace_thx( long pid_, long tid_ );

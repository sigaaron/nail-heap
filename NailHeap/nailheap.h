#ifndef NAIL_HEAP_H
#define NAIL_HEAP_H

#include <windows.h>

#include "detours/include/detours.h"
#include "detours/include/detver.h"
#include "detours/include/syelog.h"
#include "nh_tracer.h"

#pragma comment( lib, "detours.lib" )

#define HOOK_FX( remark_, old_, new_ )                                                      \
    do {                                                                                    \
        long r = ::DetourAttach( &( PVOID& )old_, new_ );                                   \
        nh_trace( "np-hook: [%s] rep: [%x] -> [%x] , ret = %d\n", remark_, old_, new_, r ); \
    } while ( 0 )

#define UNHOOK_FX( remark_, old_, new_ )                                                      \
    do {                                                                                      \
        long r = ::DetourDetach( &( PVOID& )old_, new_ );                                     \
        nh_trace( "np-unhook: [%s] rep: [%x] -> [%x] , ret = %d\n", remark_, old_, new_, r ); \
    } while ( 0 )

void nh_initialize();
void nh_uninitialize();

//--sdk apis hook.
void np_sdk_start();
void np_sdk_end();

#endif

#include "nailheap.h"

//! VirtualAlloc
static auto o_virtual_alloc = VirtualAlloc;
LPVOID
WINAPI
r_VirtualAlloc(
    _In_opt_ LPVOID lpAddress,
    _In_ SIZE_T     dwSize,
    _In_ DWORD      flAllocationType,
    _In_ DWORD      flProtect ) {

    nh_trace( "v_a=> dwsize = %u, alloctype= %u\n", dwSize, flAllocationType );

    char mod_name[ MAX_PATH ] = { 0 };
    // GetModuleHandleA( NULL )
    ::GetModuleFileNameA( NULL, mod_name, sizeof( mod_name ) );

    nh_trace( "module name = %s\n", mod_name );

    return o_virtual_alloc( lpAddress,
                            dwSize,
                            flAllocationType,
                            flProtect );
}

//! CreateFileMapping
auto o_create_filemapping_a = CreateFileMappingA;
HANDLE
WINAPI
r_CreateFileMappingA(
    _In_ HANDLE                    hFile,
    _In_opt_ LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    _In_ DWORD                     flProtect,
    _In_ DWORD                     dwMaximumSizeHigh,
    _In_ DWORD                     dwMaximumSizeLow,
    _In_opt_ LPCSTR                lpName ) {

    nh_trace( "f_m_ansi => szhi = %u, szlw= %u, name=%s\n", dwMaximumSizeHigh, dwMaximumSizeLow, lpName );

    return o_create_filemapping_a( hFile,
                                   lpFileMappingAttributes,
                                   flProtect,
                                   dwMaximumSizeHigh,
                                   dwMaximumSizeLow,
                                   lpName );
}

auto o_create_filemapping_w = CreateFileMappingW;
HANDLE
WINAPI
r_CreateFileMappingW(
    _In_ HANDLE                    hFile,
    _In_opt_ LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    _In_ DWORD                     flProtect,
    _In_ DWORD                     dwMaximumSizeHigh,
    _In_ DWORD                     dwMaximumSizeLow,
    _In_opt_ LPCWSTR               lpName ) {

    nh_trace( "f_m_w => szhi = %u, szlw= %u, name=%s\n", dwMaximumSizeHigh, dwMaximumSizeLow, lpName );
    return o_create_filemapping_w( hFile,
                                   lpFileMappingAttributes,
                                   flProtect,
                                   dwMaximumSizeHigh,
                                   dwMaximumSizeLow,
                                   lpName );
}

//! SetProcessWorkingSetSize
auto o_ps_ws = SetProcessWorkingSetSize;
BOOL
    WINAPI
    r_SetProcessWorkingSetSize(
        _In_ HANDLE hProcess,
        _In_ SIZE_T dwMinimumWorkingSetSize,
        _In_ SIZE_T dwMaximumWorkingSetSize ) {

    nh_trace( "busybody[ %u ] is shrink process [%u],min=[ %u ], max= [%u]\n",
              ::GetCurrentProcessId(),
              ::GetProcessId( hProcess ),
              dwMinimumWorkingSetSize,
              dwMaximumWorkingSetSize );

    return o_ps_ws( hProcess,
                    dwMinimumWorkingSetSize,
                    dwMaximumWorkingSetSize );
}

//------------gather.
void np_sdk_start() {
    ::DetourTransactionBegin();
    ::DetourUpdateThread( GetCurrentThread() );

    // HOOK_FX( "va", o_virtual_alloc, r_VirtualAlloc );
    HOOK_FX( "CFM-A", o_create_filemapping_a, r_CreateFileMappingA );
    HOOK_FX( "CFM-W", o_create_filemapping_w, r_CreateFileMappingW );
    HOOK_FX( "SetProcessWorkingSetSize", o_ps_ws, r_SetProcessWorkingSetSize );

    long ret = ::DetourTransactionCommit();
    nh_trace( "hva_commit[1] res =  %d\n", ret );
}

void np_sdk_end() {
    ::DetourTransactionBegin();
    ::DetourUpdateThread( GetCurrentThread() );

    // UNHOOK_FX( "va", o_virtual_alloc, r_VirtualAlloc );
    UNHOOK_FX( "CFM-A", o_create_filemapping_a, r_CreateFileMappingA );
    UNHOOK_FX( "CFM-W", o_create_filemapping_w, r_CreateFileMappingW );
    UNHOOK_FX( "SetProcessWorkingSetSize", o_ps_ws, r_SetProcessWorkingSetSize );

    long ret = ::DetourTransactionCommit();
    nh_trace( "hva_commit[2] res =  %d\n", ret );
}
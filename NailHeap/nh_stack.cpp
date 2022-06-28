#include <Windows.h>
#include <conio.h>
#include <dbghelp.h>
#include <map>
#include <string>
#include <tlhelp32.h>

#include "nh_stack.h"

#include "nh_tracer.h"

#pragma comment( lib, "dbghelp.lib" )

static char                            s_pdb_path[ MAX_PATH ];
static std::map<DWORD64, std::wstring> s_symbol_map;

HANDLE ps_initialize( DWORD       nProcessId,
                      const char* pszPdbFilePath ) {

    HANDLE hProcess = OpenProcess( PROCESS_ALL_ACCESS,
                                   FALSE,
                                   nProcessId );

    if ( hProcess == NULL ) {
        nh_trace( "OpenProcess() Error code 0x%08x!\n", GetLastError() );
        return NULL;
    }

    DWORD symOptions = SymGetOptions();
    symOptions |= SYMOPT_LOAD_LINES;
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
    symOptions = SymSetOptions( symOptions );
    if ( !SymInitialize( hProcess, pszPdbFilePath, TRUE ) ) {
        CloseHandle( hProcess );
        nh_trace( "SymInitialize() Error code 0x%08x!\n", GetLastError() );
        return NULL;
    }

    return hProcess;
}

BOOL load_symbol_files( HANDLE hProcess,
                        DWORD  nProcessId ) {

    HANDLE hSnap = INVALID_HANDLE_VALUE;
    hSnap        = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, nProcessId );

    if ( hSnap == INVALID_HANDLE_VALUE ) {
        nh_trace( "CreateToolhelp32Snapshot() Error code 0x%08x!\n", GetLastError() );
        return FALSE;
    }

    MODULEENTRY32 me;
    memset( &me, 0, sizeof( me ) );
    me.dwSize   = sizeof( me );
    int  nCount = 0;
    BOOL bRes   = Module32First( hSnap, &me );

    while ( bRes ) {
        SymLoadModule64( hProcess,
                         0,
                         ( PCSTR )me.szExePath,
                         ( PCSTR )me.szModule,
                         ( DWORD64 )me.modBaseAddr,
                         me.modBaseSize );

        std::wstring name                         = me.szModule;
        s_symbol_map[ ( DWORD64 )me.modBaseAddr ] = name;
        bRes                                      = Module32Next( hSnap, &me );
        nCount++;
    }

    CloseHandle( hSnap );
    return nCount > 0;
}

BOOL peek_thread_stack( HANDLE       hProcess,
                        DWORD        nThreadId,
                        std::string& out_string_ ) {

    out_string_.clear();

    DWORD nProcessId = GetProcessId( hProcess );

    const DWORD STACKWALK_MAX_NAMELEN = 1024;
    HANDLE      hSnap                 = CreateToolhelp32Snapshot( TH32CS_SNAPALL, nProcessId );
    if ( hSnap == INVALID_HANDLE_VALUE ) {
        nh_trace( "CreateToolhelp32Snapshot() Error code 0x%08x!\n", GetLastError() );
        return FALSE;
    }

    HANDLE hThread = OpenThread( THREAD_ALL_ACCESS,
                                 FALSE,
                                 nThreadId );
    if ( hThread == NULL ) {
        CloseHandle( hSnap );
        nh_trace( "OpenThread() Error code 0x%08x!\n", GetLastError() );
        return FALSE;
    }

    SuspendThread( hThread );

    CONTEXT context;
    memset( &context, 0, sizeof( CONTEXT ) );
    context.ContextFlags = CONTEXT_FULL;
    if ( !GetThreadContext( hThread, &context ) ) {
        ResumeThread( hThread );
        CloseHandle( hSnap );
        nh_trace( "GetThreadContext() Error code 0x%08x!\n", GetLastError() );
        return FALSE;
    }

    STACKFRAME64 stackframe;
    memset( &stackframe, 0, sizeof( stackframe ) );
    DWORD imageType;

    imageType                   = IMAGE_FILE_MACHINE_I386;
    stackframe.AddrPC.Offset    = context.Eip;
    stackframe.AddrPC.Mode      = AddrModeFlat;
    stackframe.AddrFrame.Offset = context.Ebp;
    stackframe.AddrFrame.Mode   = AddrModeFlat;
    stackframe.AddrStack.Offset = context.Esp;
    stackframe.AddrStack.Mode   = AddrModeFlat;

    char               sysbuff[ sizeof( IMAGEHLP_SYMBOL64 ) + STACKWALK_MAX_NAMELEN ];
    IMAGEHLP_SYMBOL64* pSym = ( IMAGEHLP_SYMBOL64* )sysbuff;
    memset( pSym, 0x00, sizeof( IMAGEHLP_SYMBOL64 ) + STACKWALK_MAX_NAMELEN );
    pSym->SizeOfStruct  = sizeof( IMAGEHLP_SYMBOL64 );
    pSym->MaxNameLength = STACKWALK_MAX_NAMELEN;

    IMAGEHLP_LINE64 line;
    memset( &line, 0x00, sizeof( line ) );
    line.SizeOfStruct = sizeof( line );

    char szOneCallFunctionInfo[ STACKWALK_MAX_NAMELEN ];
    nh_trace( "\n\nThreadId %u:\n", nThreadId );

    while ( true ) {

        if ( !StackWalk64( imageType,
                           hProcess,
                           hThread,
                           &stackframe,
                           &context,
                           NULL,
                           SymFunctionTableAccess64,
                           SymGetModuleBase64,
                           NULL ) ) {

            nh_trace( "StackWalk64() Error code 0x%08x!\n", GetLastError() );
            return FALSE;
        }

        if ( stackframe.AddrPC.Offset == stackframe.AddrReturn.Offset ) {
            break;
        }

        if ( stackframe.AddrPC.Offset != 0 ) {
            memset( szOneCallFunctionInfo, 0x00, sizeof( char ) * STACKWALK_MAX_NAMELEN );

            DWORD64 moduleBase = SymGetModuleBase64( hProcess,
                                                     stackframe.AddrPC.Offset );

            auto module_itr = s_symbol_map.find( moduleBase );

            if ( module_itr != s_symbol_map.end() ) {
                const char* name = module_itr->second.c_str();

                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, name );
            }
            else {
                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, "unknow module" );
            }

            strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, ": " );

            DWORD dwDisplacementFromLine;
            if ( SymGetLineFromAddr64( hProcess,
                                       stackframe.AddrPC.Offset,
                                       &dwDisplacementFromLine,
                                       &line ) ) {
                char name[ 50 ] = { 0 };
                _itoa_s( line.LineNumber, name, 50, 10 );
                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, line.FileName );
                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, "(" );
                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, name );
                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, ")" );
            }
            else {
                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, "unknow file" );
            }
            strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, ": " );

            DWORD64 dwDisplacementFromFunc;
            if ( SymGetSymFromAddr64( hProcess,
                                      stackframe.AddrPC.Offset,
                                      &dwDisplacementFromFunc,
                                      pSym ) ) {
                if ( pSym->Name[ 0 ] == '?' ) {

                    char name[ STACKWALK_MAX_NAMELEN ] = { 0 };
                    UnDecorateSymbolName( pSym->Name, name, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY );
                    strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, name );
                }
                else {
                    strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, pSym->Name );
                }
            }
            else {
                strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, "unknow func" );
            }
            strcat_s( szOneCallFunctionInfo, STACKWALK_MAX_NAMELEN, "\n" );

            out_string_ = szOneCallFunctionInfo;
        }

        if ( stackframe.AddrReturn.Offset == 0 )
            break;
    }

    ResumeThread( hThread );
    CloseHandle( hThread );
    CloseHandle( hSnap );

    return TRUE;
}

BOOL peek_threads_stack( HANDLE       hProcess,
                         std::string& output_string_ ) {

    DWORD nProcessId = GetProcessId( hProcess );

    HANDLE hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPALL,
                                             nProcessId );
    if ( hSnap == INVALID_HANDLE_VALUE ) {
        nh_trace( "CreateToolhelp32Snapshot() Error code 0x%08x!\n", GetLastError() );
        return FALSE;
    }

    std::string thread_stack_string;

    THREADENTRY32 te;
    memset( &te, 0, sizeof( te ) );
    te.dwSize = sizeof( te );
    BOOL bRes = Thread32First( hSnap, &te );
    while ( bRes ) {
        if ( te.th32OwnerProcessID == nProcessId ) {
            if ( peek_thread_stack( hProcess,
                                    nProcessId,
                                    te.th32ThreadID,
                                    thread_stack_string ) ) {
                output_string_ += "\n" + thread_stack_string + "\n";
            }
            else {
                output_string_ += "\n-------------------error thread stack tracing-------------------------\n";
            }
        }
        bRes = Thread32Next( hSnap, &te );
    }

    CloseHandle( hSnap );

    return TRUE;
}

int nh_stack_init( const char* pdb_file_path_ ) {

    memset( s_pdb_path, 0x00, sizeof( s_pdb_path ) );
    strcpy_s( s_pdb_path, pdb_file_path_ );

    return 0;
}

std::string nh_stack_trace_ps( long pid_ ) {
    auto        ps_handle = ps_initialize( pid_, s_pdb_path );
    std::string r         = "";
    peek_threads_stack( ps_handle, r );

    return r;
}

std::string nh_stack_trace_thx( long pid_, long tid_ ) {
    auto ps_handle = ps_initialize( pid_, s_pdb_path );

    std::string r = "";
    peek_thread_stack( ps_handle, tid_, r );

    return r;
}
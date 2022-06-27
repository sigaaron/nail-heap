#include <Windows.h>
#include <iostream>
#include <stdio.h>

int main() {

    HMODULE h = ::LoadLibraryA( "NailHeap.dll" );

    printf( "load nail heap=0x%x,%d\n", ( unsigned )h, GetLastError() );

    auto v = VirtualAlloc( NULL, 100 * 1024 * 1024, MEM_COMMIT, PAGE_NOACCESS );

    HANDLE hFile = CreateFile( L"./mmfile",
                               GENERIC_WRITE | GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               CREATE_ALWAYS,
                               FILE_FLAG_SEQUENTIAL_SCAN,
                               NULL );

    HANDLE hFileMapping = CreateFileMapping( hFile, NULL, PAGE_READWRITE, 0, 100 * 1024 * 1024, NULL );
    CloseHandle( hFile );

    PBYTE pbFile = ( PBYTE )MapViewOfFile( hFileMapping,
                                           FILE_MAP_ALL_ACCESS,
                                           0,
                                           0,
                                           50 * 1024 * 1024 );

    getchar();
    return 0;
}

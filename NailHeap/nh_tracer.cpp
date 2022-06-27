
#include <Windows.h>
#include <stdio.h>

#include "nh_tracer.h"

#define PRINT( m_ ) ::OutputDebugStringA( m_ );

void nh_trace( const char* format_, ... ) {
    va_list ap;
    va_start( ap, format_ );

    char buf[ 512 ] = { 0 };

    vsprintf_s( buf, sizeof( buf ), format_, ap );

    PRINT( buf );
}

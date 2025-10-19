// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define ISOLATION_AWARE_ENABLED 1

#ifndef WINVER
#define WINVER 0x601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x601
#endif
#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x601
#endif

#include <winsdkver.h>

#include "helpers/foobar2000+atl.h"

#define SCI_NAMESPACE

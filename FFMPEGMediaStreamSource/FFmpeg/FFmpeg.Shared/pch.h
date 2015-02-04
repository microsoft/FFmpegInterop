#pragma once

#include <collection.h>
#include <ppltasks.h>

// Disable debug string output on non-debug build
#if !_DEBUG
#ifdef OutputDebugString
#undef OutputDebugString
#endif
#define OutputDebugString(x)
#endif
#pragma once

// 1. Target Windows 10 or later
// 0x0601 was for (Win7). Let's bump it to 0x0A00 (Win10)
#define _WIN32_WINNT 0x0A00
#include <sdkddkver.h>

// 2. The Golden Rule: Disable min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

// 3. Speed up build process
#define WIN32_LEAN_AND_MEAN

// 4. Nuke the old API junk we don't need
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOSYSCOMMANDS
#define NORASTEROPS
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
//#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE

#define STRICT

#include <Windows.h>
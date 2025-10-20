#pragma once

#define PLUGIN_LONG_NAME "Title Format Sandbox Mod"

#define COMPONENT_VERSION_MAJOR 1
#define COMPONENT_VERSION_MINOR 1
#define COMPONENT_VERSION_PATCH 1
#define COMPONENT_VERSION_SUB_PATCH 0

#define MAKE_STRING(text) #text

//#define FIX_VER

#ifdef FIX_VER
#define MAKE_COMPONENT_VERSION(major,minor,patch) MAKE_STRING(major) "." MAKE_STRING(minor) "." MAKE_STRING(patch)
#define FOO_TF_SANDBOX_VERSION MAKE_COMPONENT_VERSION(COMPONENT_VERSION_MAJOR,COMPONENT_VERSION_MINOR,COMPONENT_VERSION_PATCH) " beta 1"
#define MAKE_DLL_VERSION(major,minor,patch,subpatch) MAKE_STRING(major) "." MAKE_STRING(minor) "." MAKE_STRING(patch) "." MAKE_STRING(subpatch)
#else
#define MAKE_COMPONENT_VERSION(major,minor,patch) MAKE_STRING(major) "." MAKE_STRING(minor) "." MAKE_STRING(patch)
#define FOO_TF_SANDBOX_VERSION MAKE_COMPONENT_VERSION(COMPONENT_VERSION_MAJOR,COMPONENT_VERSION_MINOR, COMPONENT_VERSION_PATCH)
#define MAKE_DLL_VERSION(major,minor, patch) MAKE_STRING(major) "." MAKE_STRING(minor) "." MAKE_STRING(patch)
#endif
#define MAKE_API_SDK_VERSION(sdk_ver, sdk_target) MAKE_STRING(sdk_ver) " " MAKE_STRING(sdk_target)

#ifdef FIX_VER
#define DLL_VERSION_NUMERIC COMPONENT_VERSION_MAJOR, COMPONENT_VERSION_MINOR, COMPONENT_VERSION_PATCH, COMPONENT_VERSION_SUB_PATCH
#define DLL_VERSION_STRING MAKE_DLL_VERSION(COMPONENT_VERSION_MAJOR,COMPONENT_VERSION_MINOR,COMPONENT_VERSION_PATCH,COMPONENT_VERSION_SUB_PATCH)
#else
#define DLL_VERSION_NUMERIC COMPONENT_VERSION_MAJOR, COMPONENT_VERSION_MINOR, COMPONENT_VERSION_PATCH
#define DLL_VERSION_STRING MAKE_DLL_VERSION(COMPONENT_VERSION_MAJOR,COMPONENT_VERSION_MINOR,COMPONENT_VERSION_PATCH)
#endif

//SDK ver
#define PLUGIN_FB2K_SDK MAKE_API_SDK_VERSION(FOOBAR2000_SDK_VERSION, FOOBAR2000_TARGET_VERSION)

#define COMPONENT_NAME "foo_tfsandbox_mod"
#define COMPONENT_YEAR "2025"
#define COMPONENT_COPYRIGHT_YEAR "2025"

#define PLUGIN_DLLFILENAME COMPONENT_NAME ".dll"
#define PLUGIN_VERSION FOO_TF_SANDBOX_VERSION

#define PLUGIN_ABOUT \
"Title formatting editor with syntax coloring, code structure view and preview.\n" \
"\n" \
"Original Author: Holger Stenger\n" \
"Mod Author: Dayuyu\n" \
"Version: "FOO_TF_SANDBOX_VERSION"\n" \
"Compiled: "__DATE__ "\n" \
"fb2k SDK: "PLUGIN_FB2K_SDK"\n" \
"\n" \
"Copyright (C) 2008-"COMPONENT_YEAR" Holger Stenger\n\n" \
"Author Website: https://github.com/stengerh/foo_tfsandbox\n" \
"Mod Website: https://github.com/ghDaYuYu/foo_tfsandbox\n" \
"\n" \
"Scintilla and Lexilla source code:\n" \
"Copyright 1998-2025 by Neil Hodgson <neilh@scintilla.org>\n" \
"- http://www.scintilla.org/\n" \
"\n" \
"Some icons by Yusuke Kamiyamane. Licensed under a Creative Commons Attribution 3.0 License.\n" \
"- http://p.yusukekamiyamane.com/\n" \
"- http://creativecommons.org/licenses/by/3.0/\n" \
"\n" \
"Jansson JSON Parser by Petri Lehtinen, http://www.digip.org/jansson/\n" \
"Scintilla ATL interface by Naughter, http://www.naughter.com/scintilla.html\n" \
 "\n" \
"Change log:\n" \
  "= v1.1.0 (by Dayuyu)\n" \
  "* x64 binaries.\n" \
  "* Scintilla 5.5.7\n" \
  "* Lexilla 5.4.5\n" \
  "* fb2k SDK-2025-03-07\n" \
  "* Dark mode\n"

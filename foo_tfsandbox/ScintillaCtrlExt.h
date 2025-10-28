#pragma once
#include "stdafx.h"
#include "ScintillaCtrl.h"

namespace Scintilla {

class ScintillaCtrlExt : public Scintilla::CScintillaCtrl
{
public:
	//fake create, reuse existing instance
	BOOL Create(_In_ HWND hWndParent, _In_ ATL::_U_RECT rect, _In_ DWORD dwStyle, _In_ UINT nID, _In_ DWORD dwExStyle = 0, _In_opt_ LPVOID lpParam = nullptr) {

		m_hWnd = hWndParent;
		m_bDoneInitialSetup = true;

		SetupDirectAccess();
//If we are running as Unicode, then use the UTF8 codepage else disable multi-byte support
#ifdef _UNICODE
		SetCodePage(CpUtf8);
#else
		SetCodePage(0);
#endif

		//Cache the return value from GetWindowThreadProcessId in the m_dwOwnerThreadID member variable
		m_dwOwnerThreadID = GetWindowThreadProcessId(m_hWnd, nullptr);
		return TRUE;
	}
};

}
#pragma once

#include "resource.h"

#include "libPPUI/CDialogResizeHelper.h"
#include "helpers/WindowPositionUtils.h"
#include "helpers/DarkMode.h"

#include "ILexer.h"
#include "Scintilla.h"
#include "atlscilexer.h"
#include "ScintillaCtrl.h"

#include "LibraryScope.h"
#include "titleformat_debug.h"
#include "titleformat_analysis.h"


class CTitleFormatSandboxDialog :
	public CDialogImpl<CTitleFormatSandboxDialog>,
	public message_filter
{
public:
	enum { IDD = IDD_TFDLG };
	enum { ID_SCRIPT_UPDATE_TIMER = 3451 };

	enum {
		indicator_inactive_code = INDIC_CONTAINER,
		indicator_fragment,
		indicator_error,
		indicator_warning,
		indicator_hint,
	};

	CLibraryScope m_lexillaScope;
	CLibraryScope m_scintillaScope;

	CAccelerator m_accel;

	CDialogResizeHelper m_dlgResizeHelper;
	cfgDialogPositionTracker m_dlgPosTracker;

	fb2k::CDarkModeHooks m_dark;

	HWND m_editor = NULL;
	HWND m_preview = NULL;

	CTreeViewCtrl m_treeScript;

	titleformat_debugger m_debugger;

	ast::fragment m_selfrag;

	bool m_script_update_pending;
	//bool m_updating_fragment;

	static CWindow g_wndInstance;

	class ui_v2_config_callback : public ui_config_callback {
		ui_v2_config_callback(CTitleFormatSandboxDialog* dlg) : p_dlg(dlg) {};
		virtual void ui_colors_changed() override;

	private:
		CTitleFormatSandboxDialog* p_dlg;
		friend class CTitleFormatSandboxDialog;
	};

	ui_v2_config_callback* m_ui_v2_cfg_callback = nullptr;

	static void ActivateDialog();

	virtual bool pretranslate_message(MSG *pMsg);
	virtual void OnFinalMessage(HWND) {delete this;}

	///////////////////////////////////////////////////////////////////
	// Constructor(s)

	CTitleFormatSandboxDialog();

	///////////////////////////////////////////////////////////////////
	// Destructor
	
	~CTitleFormatSandboxDialog();

	///////////////////////////////////////////////////////////////////
	// Methods

	void UpdateScript();
	void UpdateScriptSyntaxTree();

	void UpdateTrace();

	void ClearFragment();
	void UpdateFragment(int selStart, int selEnd);

	void ClearInactiveCodeIndicator();
	void UpdateInactiveCodeIndicator();

	void InitControls();
	void SetupTitleFormatStyles(CSciLexerCtrl sciLexer);
	/*void SetupTitleFormatStyles_new();*/
	void SetupPreviewStyles(CSciLexerCtrl sciLexer);

	bool find_fragment(ast::fragment &out, int start, int end);
	inline bool find_fragment(ast::fragment &out, int pos) {return find_fragment(out, pos, pos);}

	///////////////////////////////////////////////////////////////////
	// Message map and handlers

	BEGIN_MSG_MAP(CTitleFormatSandboxDialog)
		CHAIN_MSG_MAP_MEMBER(m_dlgResizeHelper) //1
		CHAIN_MSG_MAP_MEMBER(m_dlgPosTracker)   //2
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_TIMER(OnTimer)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnCancel)
		COMMAND_ID_HANDLER_EX(ID_EDIT_SELECT_ALL, OnSelectAll)
		NOTIFY_HANDLER_EX(IDC_SCRIPT, SCN_MODIFIED, OnScriptModified)
		NOTIFY_HANDLER_EX(IDC_SCRIPT, SCN_UPDATEUI, OnScriptUpdateUI)
		NOTIFY_HANDLER_EX(IDC_SCRIPT, SCN_DWELLSTART, OnScriptDwellStart)
		NOTIFY_HANDLER_EX(IDC_SCRIPT, SCN_DWELLEND, OnScriptDwellEnd)
		NOTIFY_HANDLER_EX(IDC_SCRIPT, SCN_CALLTIPCLICK, OnScriptCallTipClick)
		NOTIFY_HANDLER_EX(IDC_TREE, TVN_SELCHANGED, OnTreeSelChanged)
		NOTIFY_HANDLER_EX(IDC_TREE, NM_CUSTOMDRAW, OnTreeCustomDraw)
	END_MSG_MAP()

	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
	void OnDestroy();
	void OnTimer(UINT_PTR nIDEvent);
	void OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnSelectAll(UINT uNotifyCode, int nID, CWindow wndCtl);
	LRESULT OnScriptModified(LPNMHDR pnmh);
	LRESULT OnScriptUpdateUI(LPNMHDR pnmh);
	LRESULT OnScriptDwellStart(LPNMHDR pnmh);
	LRESULT OnScriptDwellEnd(LPNMHDR pnmh);
	LRESULT OnScriptCallTipClick(LPNMHDR pnmh);
	LRESULT OnTreeSelChanged(LPNMHDR pnmh);
	LRESULT OnTreeCustomDraw(LPNMHDR pnmh);

protected:

	Scintilla::CScintillaCtrl& GetCtrl(int id);
	std::unique_ptr<Scintilla::CScintillaCtrl> CreateScintillaControl();
	std::unique_ptr<Scintilla::CScintillaCtrl> m_pScript;	//The scintilla control
	std::unique_ptr<Scintilla::CScintillaCtrl> m_pValue;	//The scintilla control
};

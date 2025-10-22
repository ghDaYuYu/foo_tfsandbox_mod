#include "stdafx.h"
#include <vector>

#include "SDK/console.h"
#include "libPPUI/win32_utility.h"

#include "Scintilla.h"
#include "ScintillaMessages.h"
#include "ScintillaTypes.h"

#include "scintilla/include/ILexer.h"
#include "lexilla/include/SciLexer.h"

#include "Lexilla.h"

#include "ScintillaCtrlExt.h"

#include "colors_json.h"
#include "version.h"
#include "guids.h"

#include "titleformat_node_filter.h"
#include "titleformat_visitor_impl.h"

#include "TitleformatSandboxDialog.h"

#ifndef U2T
#define U2T(Text) pfc::stringcvt::string_os_from_utf8(Text).get_ptr()
#endif

//#define USE_EXPLORER_THEME 1
const size_t kMaxBuffer = 1024 * 1024;

static cfgDialogPosition cfg_window_position(guid_cfg_window_position);

static cfg_string cfg_format(guid_cfg_format,
#ifdef _DEBUG
	"// This is a comment\r\n'test'test$$%%''\r\n$if(%track artist%,'['test'')\r\n// This is another comment\r\n[%comment%]"
#else
	"// Type your title formatting code here.\r\n"
	"// The tree view on the right side shows the structure of the code.\r\n"
	"// Put the cursor on an expression to see its value in the box below.\r\n"
	"// The symbol before the first line shows the truth value of the expression. A check mark means true and a cross means false.\r\n"
	"[%artist% - ]%title%"
#endif
);

static advconfig_checkbox_factory cfg_adv_load_theme_file("Load theme file", "foo_tfsandbox_mod.load_theme_file", guid_cfg_adv_load_theme_file, guid_tfsandbox_branch, order_load_theme_file, true);

static COLORREF BlendColor(COLORREF color1, DWORD weight1, COLORREF color2, DWORD weight2)
{
	int r = (GetRValue(color1) * weight1 + GetRValue(color2) * weight2) / (weight1 + weight2);
	int g = (GetGValue(color1) * weight1 + GetGValue(color2) * weight2) / (weight1 + weight2);
	int b = (GetBValue(color1) * weight1 + GetBValue(color2) * weight2) / (weight1 + weight2);
	return RGB(r, g, b);
}

static void LoadMarkerIcon(CSciLexerCtrl sciLexer, int markerNumber, LPCTSTR name)
{
	CIcon icon;
	icon.LoadIcon(name, 16, 16);

	ICONINFO iconInfo;
	BOOL succeeded = icon.GetIconInfo(&iconInfo);

	CBitmapHandle imageColor = iconInfo.hbmColor;

	BITMAP bitmap;
	succeeded = imageColor.GetBitmap(&bitmap);

	DWORD dwCount = bitmap.bmWidthBytes * bitmap.bmHeight;

	std::vector<char> pixels;
	pixels.resize(dwCount);

	DWORD dwBytesCopied = imageColor.GetBitmapBits(dwCount, (LPVOID) &pixels[0]);

	for (int y = 0; y < 16; ++y)
	{
		for (int x = 0; x < 16; ++x)
		{
			int base = (y * 16 + x) * 4;
			char r = pixels[base + 0];
			char g = pixels[base + 1];
			char b = pixels[base + 2];
			//char a = pixels[base + 3];
			pixels[base + 2] = r;
			pixels[base + 1] = g;
			pixels[base + 0] = b;
			//pixels[base + 3] = a;
		}
	}

	sciLexer.RGBAImageSetWidth(16);
	sciLexer.RGBAImageSetHeight(16);
	sciLexer.RGBAImageSetScale(100);
	sciLexer.MarkerDefineRGBAImage(markerNumber, &pixels[0]);
}

CWindow CTitleFormatSandboxDialog::g_wndInstance;

bool CTitleFormatSandboxDialog::find_fragment(ast::fragment &out, int start, int end)
{
	return ast::find_fragment(out, m_debugger.get_root(), start, end, &ast::node_filter_impl_unknown_function(m_debugger));
}

//snapLeft, snapTop, snapRight, snapBottom
static CDialogResizeHelper::Param resizeParams[] = {
    {IDC_SCRIPT, 0, 0, 1, 1},
    {IDC_VALUE, 0, 1, 1, 1},
    {IDC_TREE, 1, 0, 1, 1}
};

//! Called when user changes configuration of colors (also as a result of toggling dark mode). \n
//! Note that for the duration of these callbacks, both old handles previously returned by query_font() as well as new ones are valid; old font objects are released when the callback cycle is complete.
void CTitleFormatSandboxDialog::ui_v2_config_callback::ui_colors_changed() {
	bool dark = false;
	if (!cfg_adv_load_theme_file.get())
	{
		return;
	}

	colors_json cj;
	auto ui_cm = ui_config_manager::tryGet();
	if (ui_cm.is_valid()) {
		dark = ui_cm->is_dark_mode();
		cj.read_colors_json(dark);
	}
	else {
		console::formatter() << "[" << COMPONENT_NAME << "] : " << "Failed to access ui config manager.";
	}

	p_dlg->InitControls(dark);

}

CTitleFormatSandboxDialog::CTitleFormatSandboxDialog() :m_dlgResizeHelper(resizeParams), m_script_update_pending(false),
	m_dlgPosTracker(cfg_window_position)
{
	m_dlgResizeHelper.set_min_size(342, 232);
	m_dlgResizeHelper.m_autoSizeGrip = false;

	pfc::string8 install_dir = pfc::string_directory(core_api::get_my_full_path());
	pfc::string8 scintilla_path = PFC_string_formatter() << install_dir << "\\Scintilla.dll";

	m_scintillaScope.LoadLibrary(pfc::stringcvt::string_os_from_utf8(scintilla_path.get_ptr()));

	if (!m_scintillaScope.IsLoaded()) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << scintilla_path << " not loaded.";
		return;
	}

	pfc::string8 lexilla_path = PFC_string_formatter() << install_dir << "\\Lexilla.dll";

	m_lexillaScope.LoadLibrary(pfc::stringcvt::string_os_from_utf8(lexilla_path.get_ptr()));

	if (!m_lexillaScope.IsLoaded()) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << lexilla_path << " not loaded.";
		return;
	}
}

CTitleFormatSandboxDialog::~CTitleFormatSandboxDialog()
{
	bool bv2 = core_version_info_v2::get()->test_version(2, 0, 0, 0);
	if (bv2) {
		ui_config_manager::get()->remove_callback(m_ui_v2_cfg_callback);
		delete m_ui_v2_cfg_callback;
	}
}

bool CTitleFormatSandboxDialog::pretranslate_message(MSG *pMsg)
{
	if (GetActiveWindow() == *this)
	{
		if (m_accel.TranslateAccelerator(*this, pMsg))
			return true;
	}
	if (IsDialogMessage(pMsg))
		return true;
	return false;
}

void CTitleFormatSandboxDialog::ActivateDialog()
{
	if (!g_wndInstance)
	{
		CTitleFormatSandboxDialog * dlg /*g_dialog*/ = new CTitleFormatSandboxDialog();

		if (dlg->m_scintillaScope.IsLoaded() && dlg->m_lexillaScope.IsLoaded() &&
			dlg->Create(core_api::get_main_window()) != NULL)
		{

			dlg->ShowWindow(SW_SHOW);
		}
		else
		{
			DWORD dwError = ::GetLastError();
			pfc::string8 message;
			console::formatter() << "[" << COMPONENT_NAME << "] " << ": Could not create window, "
				<< (uFormatSystemErrorMessage(message, dwError) ? message.get_ptr() : pfc::format_hex(dwError).get_ptr());
		}
	}
	else
	{
		g_wndInstance.SetFocus();
	}
}

void CTitleFormatSandboxDialog::InitControls(bool dark_alpha) {

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	auto& rCtrlValue{ GetCtrl(IDC_VALUE) };

	// -----------   VALUE PREVIEW--------------------

	// PANEL BACK/FOREGROUND

	COLORREF crcol = get_gen_color(mgen_colors["background"]);

	rCtrlValue.StyleSetBack(STYLE_DEFAULT, crcol);

	crcol = get_gen_color(mgen_colors["foreground"]);

	rCtrlValue.StyleSetFore(STYLE_DEFAULT, crcol);

	// CURSOR/CARET

	rCtrlScript.SetCaretFore(crcol);

	//TreeView_SetBkColor(m_treeScript, RGB(100, 255, 100));// (m_treeScript, crcol);

	// VALUE SELECTION BACK/FOREGROUND/HIGHLIGHT

	crcol = get_gen_color(mgen_colors["selection background"]);
	rCtrlValue.SetSelBack(STYLE_DEFAULT, crcol);

	crcol = get_gen_color(mgen_colors["selection foreground"]);
	//font color when draggin cursor over text
	rCtrlValue.SetSelFore(STYLE_DEFAULT, crcol/*RGB(255,0,0)*/);

	crcol = get_gen_color(mgen_colors["marker foreground"]);

	rCtrlValue.MarkerSetFore(STYLE_DEFAULT, crcol);

	crcol = get_gen_color(mgen_colors["marker background"]);

	rCtrlValue.MarkerSetBack(STYLE_DEFAULT, crcol);

	rCtrlScript.SetCodePage(SC_CP_UTF8);
	rCtrlScript.SetModEventMask(Scintilla::ModificationFlags::InsertText | Scintilla::ModificationFlags::DeleteText);
	rCtrlScript.SetMouseDwellTime(500);

	rCtrlValue.SetCodePage(SC_CP_UTF8);

	SetIcon(static_api_ptr_t<ui_control>()->get_main_icon());

	// Set up styles
	SetupTitleFormatStyles(m_editor, dark_alpha);

	SetupPreviewStyles(m_preview);

	rCtrlValue.SetReadOnly(true);

#if defined USE_EXPLORER_THEME
	SetWindowTheme(m_treeScript, L"Explorer", NULL);
#endif

	CImageList imageList;

#if defined(USE_EXPLORER_THEME)
	imageList.CreateFromImage(IDB_SYMBOLS32, 16, 2, RGB(255, 0, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
#else
	if (m_dark.IsDark() || IsWine()) {
		imageList.CreateFromImage(IDB_SYMBOLS32, 16, 2, RGB(255, 0, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
	}
	else {
		imageList.CreateFromImage(IDB_SYMBOLS, 16, 2, RGB(255, 0, 255), IMAGE_BITMAP);
	}
#endif
	imageList.SetOverlayImage(6, 1);

	m_treeScript.SetImageList(imageList);
}

void CTitleFormatSandboxDialog::SetupTitleFormatStyles(CSciLexerCtrl sciLexer, bool dark_alpha)
{
	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

	// MARGIN

	rCtrlScript.SetMarginTypeN(0, Scintilla::MarginType::Number);
	rCtrlScript.SetMarginWidthN(0, rCtrlScript.TextWidth(STYLE_LINENUMBER, "_99"));

	sciLexer.SetMarginTypeN(1, SC_MARGIN_BACK);
	sciLexer.SetMarginMaskN(1, SC_MASK_FOLDERS);
	sciLexer.SetMarginWidthN(1, 16);
	sciLexer.SetMarginSensitiveN(1, true);

	// MAIN BACK/FORE COLORS

	COLORREF background;
	COLORREF foreground;
	COLORREF selbackground;
	COLORREF selforeground;

	///////////////////////////////////////////////////////////////

	background = get_gen_color(mgen_colors["background"]);
	foreground = get_gen_color(mgen_colors["foreground"]);

	selbackground = get_gen_color(mgen_colors["selection background"]);
	selforeground = get_gen_color(mgen_colors["selection foreground"]);

	///////////////////////////////////////////////////////////////

	if (IsWine()) {
		TreeView_SetBkColor(m_treeScript, background);
	}

	rCtrlScript.SetEdgeColour(background);

	sciLexer.SetFoldMarginColour(true, background);

	int markerNumbers[] = {
		SC_MARKNUM_FOLDEROPEN,
		SC_MARKNUM_FOLDER,
		SC_MARKNUM_FOLDERSUB,
		SC_MARKNUM_FOLDERTAIL,
		SC_MARKNUM_FOLDEREND,
		SC_MARKNUM_FOLDEROPENMID,
		SC_MARKNUM_FOLDERMIDTAIL
	};

	// MARKERS

	COLORREF cr_mf = get_gen_color(mgen_colors["marker foreground"]);
	COLORREF cr_mb = get_gen_color(mgen_colors["marker background"]);
	COLORREF cr_msb = get_gen_color(mgen_colors["marker selected background"]);

	for (int index = 0; index < (sizeof(markerNumbers) / sizeof(markerNumbers[0])); ++index)
	{
		int markerNumber = markerNumbers[index];
		sciLexer.MarkerSetFore(markerNumber, cr_mf);
		sciLexer.MarkerSetBack(markerNumber, cr_mb);
		sciLexer.MarkerSetBackSelected(markerNumber, cr_msb);
	}

	sciLexer.MarkerDefine(SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
	sciLexer.MarkerDefine(SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
	sciLexer.MarkerDefine(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
	sciLexer.MarkerDefine(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
	sciLexer.MarkerDefine(SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
	sciLexer.MarkerDefine(SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
	sciLexer.MarkerDefine(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);

	// WRAP AND FOLD

	sciLexer.SetAutomaticFold(SC_AUTOMATICFOLD_CLICK);
	sciLexer.SetWrapMode(SC_WRAP_WORD);
	sciLexer.SetWrapVisualFlags(SC_WRAPVISUALFLAG_END);

	// ---------  L E X E R -------------
	
	// FORE/BACKGROUND , SEL FORE/BACKGROUND
	
	//rCtrlScript.SetFoldMarginColour(background);
	//rCtrlScript.SetElementColour(...);

	sciLexer.StyleSetFont(STYLE_DEFAULT, "Consolas");
	//sciLexer.StyleSetSizeFractional(STYLE_DEFAULT, 800);
	sciLexer.StyleSetFore(STYLE_DEFAULT, foreground);
	sciLexer.StyleSetBack(STYLE_DEFAULT, background);

	sciLexer.SetSelFore(true, selforeground);
	sciLexer.SetSelBack(true, selbackground);

	sciLexer.StyleClearAll();

	COLORREF cr_tmp = get_gen_color(mgen_colors["selection foreground"]);

	sciLexer.SetSelFore(true, cr_tmp);

	cr_tmp = get_gen_color(mgen_colors["selection background"]);

	sciLexer.SetSelBack(true, cr_tmp);

	// CALL TIPS
	sciLexer.CallTipUseStyle(50);

	sciLexer.StyleSetFont(STYLE_CALLTIP, "Verdana");

	cr_tmp = get_gen_color(mgen_colors["calltip foreground"]);

	sciLexer.StyleSetFore(STYLE_CALLTIP, cr_tmp);

	cr_tmp = get_gen_color(mgen_colors["calltip background"]);
	sciLexer.StyleSetBack(STYLE_CALLTIP, cr_tmp);

	char outlen[MAX_PATH];
	if ((sciLexer.GetLexerLanguage(outlen) > 0))
	{
		sciLexer.SetProperty("fold", "1");

		// Comments
		COLORREF crcol = get_lex_color(mlex_colors["comment"]);

		sciLexer.StyleSetFore(1 /*SCE_TITLEFORMAT_COMMENTLINE*/, crcol);
		sciLexer.StyleSetFore(1 + 64 /*SCE_TITLEFORMAT_COMMENTLINE*/, BlendColor(crcol, 1, background, 1));

		// Operators
		crcol = get_lex_color(mlex_colors["operator"]);

		sciLexer.StyleSetFore(2, /*SCE_TITLEFORMAT_OPERATOR*/crcol);
		sciLexer.StyleSetBold(2 /*SCE_TITLEFORMAT_OPERATOR*/, true);
		sciLexer.StyleSetFore(2 + 64 /*SCE_TITLEFORMAT_OPERATOR | inactive*/, BlendColor(crcol, 1, background, 1));
		sciLexer.StyleSetBold(2 + 64 /*SCE_TITLEFORMAT_OPERATOR | inactive*/, true);

		// Fields
		crcol = get_lex_color(mlex_colors["field"]);

		sciLexer.StyleSetFore(3 /*SCE_TITLEFORMAT_FIELD*/, crcol);
		sciLexer.StyleSetFore(3 + 64 /*SCE_TITLEFORMAT_FIELD | inactive*/, BlendColor(crcol, 1, background, 1));

		// Strings (Single quoted string)
		crcol = get_lex_color(mlex_colors["literal string"]);

		sciLexer.StyleSetFore(4 /*SCE_TITLEFORMAT_STRING*/, /*foreground*/crcol);
		sciLexer.StyleSetItalic(4 /*SCE_TITLEFORMAT_STRING*/, true);
		sciLexer.StyleSetFore(4 + 64 /*SCE_TITLEFORMAT_STRING | inactive*/, BlendColor(crcol, 1, background, 1));
		sciLexer.StyleSetItalic(4 + 64 /*SCE_TITLEFORMAT_STRING | inactive*/, true);

		// Text (Unquoted string)
		crcol = get_lex_color(mlex_colors["string"]);

		sciLexer.StyleSetFore(5 /*SCE_TITLEFORMAT_LITERALSTRING*/, /*foreground*/crcol);
		sciLexer.StyleSetFore(5 + 64 /*SCE_TITLEFORMAT_LITERALSTRING | inactive*/, BlendColor(crcol, 1, background, 1));

		// Characters (%%, &&, '')
		crcol = get_lex_color(mlex_colors["special string"]);

		sciLexer.StyleSetFore(6 /*SCE_TITLEFORMAT_SPECIALSTRING*/, /*foreground*/crcol);
		sciLexer.StyleSetFore(6 + 64/*SCE_TITLEFORMAT_SPECIALSTRING | inactive*/, BlendColor(crcol, 1, background, 1));

		// Functions (todo:identifier?)
		crcol = get_lex_color(mlex_colors["identifier"]);
		sciLexer.StyleSetFore(7, /*SCE_TITLEFORMAT_IDENTIFIER*/ crcol);
		sciLexer.StyleSetFore(7 + 64 /*SCE_TITLEFORMAT_IDENTIFIER | inactive*/, BlendColor(crcol, 1, background, 1));
	}

	// inactive code

	//sciLexer.IndicSetStyle(indicator_inactive_code, INDIC_DIAGONAL);
	//sciLexer.IndicSetFore(indicator_inactive_code, RGB(64, 64, 64));

	tfRGB col = vindicator_colors[0].second;
	COLORREF crcol = RGB(col.r, col.g, col.b);

	sciLexer.IndicSetStyle(indicator_inactive_code, INDIC_STRAIGHTBOX);
	sciLexer.IndicSetFore(indicator_inactive_code, crcol);
	sciLexer.IndicSetUnder(indicator_inactive_code, true);

	sciLexer.Call(SCI_INDICSETALPHA, indicator_inactive_code, 20);
	sciLexer.Call(SCI_INDICSETOUTLINEALPHA, indicator_inactive_code, 20);

	// selected fragment
	col = vindicator_colors[1].second;
	crcol = RGB(col.r, col.g, col.b);

	sciLexer.IndicSetStyle(indicator_fragment, INDIC_STRAIGHTBOX);
	sciLexer.IndicSetFore(indicator_fragment, crcol);
	sciLexer.IndicSetUnder(indicator_fragment, true);
	if (dark_alpha) {
		auto alpha = sciLexer.IndicGetAlpha(indicator_fragment);
		sciLexer.IndicSetAlpha(indicator_fragment, (std::min)(255, alpha + 30));
	}

	// errors
	col = vindicator_colors[2].second;
	crcol = RGB(col.r, col.g, col.b);

	sciLexer.IndicSetStyle(indicator_error, INDIC_SQUIGGLE);
	sciLexer.IndicSetFore(indicator_error, crcol);
}

void CTitleFormatSandboxDialog::SetupPreviewStyles(CSciLexerCtrl sciLexer)
{
	sciLexer.SetWrapMode(SC_WRAP_WORD);
	sciLexer.SetWrapVisualFlags(SC_WRAPVISUALFLAG_END);

	sciLexer.SetMarginTypeN(0, SC_MARGIN_NUMBER);
	sciLexer.SetMarginWidthN(0, sciLexer.TextWidth(STYLE_LINENUMBER, "_99"));

	sciLexer.SetMarginTypeN(1, SC_MARGIN_BACK);
	sciLexer.SetMarginWidthN(1, 16);

	LoadMarkerIcon(sciLexer, 0, MAKEINTRESOURCE(IDI_FUGUE_CROSS));
	LoadMarkerIcon(sciLexer, 1, MAKEINTRESOURCE(IDI_FUGUE_TICK));
	LoadMarkerIcon(sciLexer, 2, MAKEINTRESOURCE(IDI_FUGUE_EXCLAMATION));
}

Scintilla::CScintillaCtrl& CTitleFormatSandboxDialog::GetCtrl(int id)
{
	if (id == IDC_SCRIPT) {
	
		if (m_pScript == nullptr) //NOLINT(clang-analyzer-cplusplus.NewDelete)
		{
			m_pScript = CreateScintillaControl();
			Scintilla::ScintillaCtrlExt* tmpEx = static_cast<Scintilla::ScintillaCtrlExt*>(m_pScript.get());
			CRect rc;
			HWND hwnd = ::GetDlgItem(m_hWnd,id);
			::GetWindowRect(hwnd, rc);
			//fake, reuse existing instance
			if (tmpEx->Create(hwnd, rc, WS_CHILD | WS_VISIBLE | WS_TABSTOP, id, 0, 0)) {
				//..
			}
			else {
				//..
			}
			tmpEx = nullptr;
		}
		return *m_pScript;
	}
	else if (id == IDC_VALUE) {

		if (m_pValue == nullptr) //NOLINT(clang-analyzer-cplusplus.NewDelete)
		{
			m_pValue = CreateScintillaControl();
			Scintilla::ScintillaCtrlExt* tmpEx = static_cast<Scintilla::ScintillaCtrlExt*>(m_pValue.get());
			
			CRect rc;
			HWND hwnd = ::GetDlgItem(m_hWnd, id);
			::GetWindowRect(hwnd, rc);
			//fake, reuse existing instance
			if (tmpEx->Create(hwnd, rc, WS_CHILD | WS_VISIBLE | WS_TABSTOP, id, 0, 0)) {
				//..
			}
			else {
				//..
			}
			tmpEx = nullptr;
		}
		return *m_pValue;
	}
	else {
		return *((Scintilla::CScintillaCtrl*)nullptr);
	}
}

std::unique_ptr<Scintilla::CScintillaCtrl> CTitleFormatSandboxDialog::CreateScintillaControl()
{
	return std::make_unique<Scintilla::CScintillaCtrl>();
}

BOOL CTitleFormatSandboxDialog::OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
{

	bool bv2 = core_version_info_v2::get()->test_version(2, 0, 0, 0);
	if (bv2 && !m_ui_v2_cfg_callback) {
		m_ui_v2_cfg_callback = new ui_v2_config_callback(this);
		ui_config_manager::get()->add_callback(m_ui_v2_cfg_callback);
	}

	m_editor = GetDlgItem(IDC_SCRIPT);
	m_preview = GetDlgItem(IDC_VALUE);

	m_treeScript.Attach(GetDlgItem(IDC_TREE));

	// --------- INSTANCIATE FT LEXER -------------------

	// TODO: fix manifest
	FARPROC lexer_tf_create_fun = GetProcAddress(m_lexillaScope.GetModule(), LEXILLA_CREATELEXER);
	Lexilla::CreateLexerFn lexilla_tf_create = (Lexilla::CreateLexerFn)(lexer_tf_create_fun);

	char lexer_tf_name[MAX_PATH] = "titleformat";
	void* pLexer = lexilla_tf_create(lexer_tf_name);
	::SendMessage(m_editor, SCI_SETILEXER, 0, (LPARAM)pLexer);

	// --

	m_dark.AddDialog(m_hWnd);
	m_dark.AddControls(m_hWnd);

	bool dark = false;
	colors_json cj;
	std::vector<std::pair<size_t, tfRGB>> vcolors;

	if (cfg_adv_load_theme_file.get()) {

		if (!bv2 || IsWine()) {
			cj.read_colors_json(false);
		}
		else {
			auto ui_cm = ui_config_manager::tryGet();
			if (ui_cm.is_valid()) {
				dark = ui_cm->is_dark_mode();
				cj.read_colors_json(dark);
			}
			else {
				console::formatter() << "[" << COMPONENT_NAME << "] : " << "Failed to access ui config manager.";
			}
		}
	}
	else {
		vgen_colors = vgen_colors_defaults;
		vlex_colors = vlex_colors_defaults;
		vindicator_colors = vindicator_colors_defaults;
	}

	InitControls(dark);

	m_script_update_pending = true;
	//m_updating_fragment = false;

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	rCtrlScript = GetCtrl(IDC_SCRIPT);

	rCtrlScript.SetText(cfg_format);
	rCtrlScript.SetEmptySelection(0);
	rCtrlScript.EmptyUndoBuffer();
	rCtrlScript.SetUndoCollection(true);

	m_accel.LoadAccelerators(IDD);

	static_api_ptr_t<message_loop>()->add_message_filter(this);

	g_wndInstance = static_cast<ATL::CWindow>(*this);

	return TRUE;
}

void CTitleFormatSandboxDialog::OnDestroy()
{
	g_wndInstance = NULL;
	static_api_ptr_t<message_loop>()->remove_message_filter(this);

}

void CTitleFormatSandboxDialog::OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	DestroyWindow();
}

LRESULT CTitleFormatSandboxDialog::OnScriptModified(LPNMHDR pnmh)
{
	//console::formatter() << "[" << COMPONENT_NAME << "] " << "OnScriptModified";
	if (m_script_update_pending)
	{
		KillTimer(ID_SCRIPT_UPDATE_TIMER);
	}
	m_script_update_pending = true;
	SetTimer(ID_SCRIPT_UPDATE_TIMER, 250);
	return 0;
}

LRESULT CTitleFormatSandboxDialog::OnScriptUpdateUI(LPNMHDR pnmh)
{
	SCNotification * pnmsc = (SCNotification *) pnmh;
#ifdef _DEBUG
	{
		console::formatter fmt;
		fmt << "[foo_tfsandbox] OnScriptUpdateUI()";
		if ((pnmsc->updated & SC_UPDATE_CONTENT) != 0)
		{
			fmt << " SC_UPDATE_CONTENT";
		}
		if ((pnmsc->updated & SC_UPDATE_SELECTION) != 0)
		{
			fmt << " SC_UPDATE_SELECTION";
		}
		if ((pnmsc->updated & SC_UPDATE_H_SCROLL) != 0)
		{
			fmt << " SC_UPDATE_H_SCROLL";
		}
		if ((pnmsc->updated & SC_UPDATE_V_SCROLL) != 0)
		{
			fmt << " SC_UPDATE_V_SCROLL";
		}
	}
#endif

	if (!m_script_update_pending && GetFocus() == m_editor)
	{

		auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

		int selStart = rCtrlScript.GetSelectionStart();
		int selEnd = rCtrlScript.GetSelectionEnd();

		UpdateFragment(selStart, selEnd);
	}
	return 0;
}

LRESULT CTitleFormatSandboxDialog::OnTreeSelChanged(LPNMHDR pnmh)
{
	if (!m_script_update_pending)
	{
		LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)pnmh;
		if (pnmtv->itemNew.hItem != 0 && pnmtv->itemNew.lParam != 0)
		{
			ast::node *n = (ast::node *)pnmtv->itemNew.lParam;
			UpdateFragment(n->get_start(), n->get_end());

			auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
			rCtrlScript.GotoPos(n->get_start());
		}
	}
	return 0;
}

LRESULT CTitleFormatSandboxDialog::OnTreeCustomDraw(LPNMHDR pnmh)
{
	LPNMTVCUSTOMDRAW pnmcd = (LPNMTVCUSTOMDRAW) pnmh;
	LRESULT lResult = CDRF_DODEFAULT;

	switch (pnmcd->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		lResult = CDRF_NOTIFYITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT:
		if (!m_script_update_pending)
		{
			bool replaceTextColor;
#ifdef USE_EXPLORER_THEME
			replaceTextColor = true;
#else
			replaceTextColor = (pnmcd->nmcd.uItemState & (CDIS_SELECTED)) == 0;
#endif
			if (pnmcd->nmcd.lItemlParam != 0 && replaceTextColor)
			{
				ast::node *n = (ast::node *)pnmcd->nmcd.lItemlParam;
				bool active = m_debugger.test_value(n);

				// MAIN BACK/FORE COLORS

				COLORREF background = get_gen_color(mgen_colors["background"]);

				COLORREF crcol = 0;

				switch (n->kind())
				{
				case ast::node::kind_block:
					//tree root 
					crcol = get_lex_color(mlex_colors["special string"]);

					pnmcd->clrText = active ? crcol/*(255, 0,0)RGB*/ : BlendColor(crcol, 1, background, 1);
					break;
				case ast::node::kind_call:

					crcol = get_lex_color(mlex_colors["field"]);

					pnmcd->clrText = active ?crcol /*RGB(192, 0, 192)*/ : BlendColor(crcol, 1, background, 1);
					break;
				case ast::node::kind_comment:

					crcol = get_lex_color(mlex_colors["comment"]);

					pnmcd->clrText = crcol; /*RGB(0, 192, 0);*/
					break;
				case ast::node::kind_condition:

					crcol = get_lex_color(mlex_colors["operator"]);

					pnmcd->clrText = active ? crcol/*RGB(255, 222, 255)*/ : BlendColor(crcol, 1, background, 1);
					break;
				case ast::node::kind_field:

					crcol = get_lex_color(mlex_colors["identifier"/*"Keyword1"*/]);

					pnmcd->clrText = active ? crcol/*RGB(150, 150, 255)*/ : BlendColor(crcol/*RGB(0, 0, 192)*/, 1, background, 1);
					break;
				case ast::node::kind_string:

					crcol = get_lex_color(mlex_colors["string"]);

					pnmcd->clrText = active ? crcol/*RGB(60, 255, 60)*/ : BlendColor(crcol, 1, background, 1);
					break;
				}
			}
		}
		break;
	}

	return lResult;
}

void CTitleFormatSandboxDialog::OnSelectAll(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	CWindow wnd = GetFocus();
	if (wnd == m_editor ||
		wnd == m_preview)
	{
		CEdit edit = wnd;
		edit.SetSelAll();
	}
}

void CTitleFormatSandboxDialog::UpdateScript()
{
	KillTimer(ID_SCRIPT_UPDATE_TIMER);
	m_script_update_pending = false;

	m_treeScript.DeleteAllItems();

	ClearFragment();
	ClearInactiveCodeIndicator();

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

	auto illa_wide = rCtrlScript.GetText(kMaxBuffer);
	auto illa_utf8 = Scintilla::CScintillaCtrl::W2UTF8(illa_wide, -1);

	cfg_format = illa_utf8;

	illa_wide.ReleaseBuffer();

	pfc::string_formatter errors;

	try
	{
		pfc::hires_timer parse_timer;
		parse_timer.start();
		m_debugger.parse(illa_utf8);
		illa_utf8.ReleaseBuffer();

		double parse_time = parse_timer.query();

		int error_count = m_debugger.get_parser_errors(errors);

		console::formatter() << "[" << COMPONENT_NAME << "] " << "Script parsed in "<<pfc::format_float(parse_time, 6)<< " seconds";

		if (error_count == 0)
		{
			UpdateTrace();
		}
		else
		{
			auto& rCtrlValue{ GetCtrl(IDC_VALUE) };
			rCtrlValue.MarkerDeleteAll(-1);
			rCtrlValue.SetReadOnly(false);
			rCtrlValue.SetText(errors);
			rCtrlValue.SetReadOnly(true);
			rCtrlValue.MarkerAdd(0, 2);
		}
	}
	catch (std::exception const & exc)
	{
		console::formatter() << "[" << COMPONENT_NAME << "]" <<  "Exception in UpdateScript(): " << exc;
	}
}

namespace ast
{
	class visitor_build_syntax_tree : private visitor
	{
	public:
		visitor_build_syntax_tree(titleformat_debugger &_debugger) : debugger(_debugger) {}
		void run(CTreeViewCtrl _treeView, block_expression *root)
		{
			treeView = _treeView;
			parent = TVI_ROOT;
			param_index = 0;
			root->accept(this);
		}

	private:
		CTreeViewCtrl treeView;
		HTREEITEM parent;
		t_size param_index;

		titleformat_debugger &debugger;

		typedef pfc::vartoggle_t<HTREEITEM> builder_helper;

		HTREEITEM InsertItem(const char *text, int image, node *n, bool warning = false)
		{
			return treeView.InsertItem(TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_PARAM|TVIF_STATE,
				U2T(text), image, image,
				warning ? INDEXTOOVERLAYMASK(1) : 0,
				warning ? LVIS_OVERLAYMASK : 0,
				(LPARAM)n, parent, TVI_LAST);
		}

		void visit(block_expression *n)
		{
			builder_helper helper(parent, parent);
			bool expand = false;
			if (param_index != ~0 && n->get_child_count() != 1)
			{
				parent = InsertItem("...", n->kind(), n);
				expand = true;
			}

			const t_size count = n->get_child_count();
			for (t_size index = 0; index < count; ++index)
			{
				n->get_child(index)->accept(this);
			}

			if (expand) treeView.Expand(parent);
		}

		void visit(comment *n) {}

		void visit(string_constant *n)
		{
			InsertItem(n->get_text().get_ptr(), n->kind(), n);
		}

		void visit(field_reference *n)
		{
			InsertItem(n->get_field_name().get_ptr(), n->kind(), n);
		}

		void visit(condition_expression *n)
		{
			builder_helper helper(parent, parent);
			parent = InsertItem("[ ... ]", n->kind(), n);

			param_index = ~0;
			n->get_param()->accept(this);

			treeView.Expand(parent);
		}

		void visit(call_expression *n)
		{
			builder_helper helper(parent, parent);

			parent = InsertItem(n->get_function_name().get_ptr(), n->kind(), n, debugger.is_function_unknown(n));

			t_size count = n->get_param_count();
			for (t_size index = 0; index < count; ++index)
			{
				param_index = index;
				n->get_param(index)->accept(this);
			}

			treeView.Expand(parent);
		}
	};
} // namespace ast

void CTitleFormatSandboxDialog::UpdateScriptSyntaxTree()
{
	pfc::hires_timer build_timer;
	build_timer.start();

	ast::visitor_build_syntax_tree(m_debugger).run(m_treeScript, m_debugger.get_root());

	double build_time = build_timer.query();
	console::formatter() << "[" << COMPONENT_NAME << "] " << "Script structure built in " << pfc::format_float(build_time, 6) << " seconds";
}

void CTitleFormatSandboxDialog::UpdateTrace()
{
	if (m_script_update_pending) return;

	m_selfrag = ast::fragment();

	metadb_handle_ptr track;
	metadb::g_get_random_handle(track);

	pfc::hires_timer trace_timer;
	trace_timer.start();

	m_debugger.trace(track);

	double trace_time = trace_timer.query();

	console::formatter() << "[" << COMPONENT_NAME << "] " << "Script traced in " << pfc::format_float(trace_time, 6) << " seconds";

	UpdateInactiveCodeIndicator();
	UpdateScriptSyntaxTree();
	
	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	UpdateFragment(rCtrlScript.GetSelectionStart(), rCtrlScript.GetSelectionEnd());
}

void CTitleFormatSandboxDialog::ClearInactiveCodeIndicator()
{
	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	rCtrlScript.SetIndicatorCurrent(indicator_inactive_code);
	rCtrlScript.SetIndicatorValue(1);
	rCtrlScript.IndicatorClearRange(0, rCtrlScript.GetTextLength());
}

void CTitleFormatSandboxDialog::UpdateInactiveCodeIndicator()
{
	inactive_range_walker walker(m_debugger);

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

	rCtrlScript.SetIndicatorCurrent(indicator_inactive_code);
	rCtrlScript.SetIndicatorValue(1);
	rCtrlScript.IndicatorClearRange(0, rCtrlScript.GetTextLength());

	if (m_debugger.get_parser_errors() == 0)
	{
		inactive_range_walker walker(m_debugger);
		size_t count = walker.inactiveRanges.size();
		for (size_t index = 0; index < count; ++index)
		{
			int position = walker.inactiveRanges[index].cpMin;
			int fillLength = walker.inactiveRanges[index].cpMax - walker.inactiveRanges[index].cpMin;

			rCtrlScript.IndicatorFillRange(position, fillLength);
		}
	}
}

void CTitleFormatSandboxDialog::ClearFragment()
{
	m_selfrag = ast::fragment();

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	rCtrlScript.SetIndicatorCurrent(indicator_fragment);
	rCtrlScript.SetIndicatorValue(1);
	rCtrlScript.IndicatorClearRange(0, rCtrlScript.GetTextLength());

}

void CTitleFormatSandboxDialog::UpdateFragment(int selStart, int selEnd)
{
	if (m_script_update_pending/* || m_updating_fragment*/) return;

	//pfc::vartoggle_t<bool> blah(m_updating_fragment, true);

	try
	{
		if (m_debugger.get_parser_errors() == 0)
		{
			ast::fragment selfrag;
			find_fragment(selfrag, selStart, selEnd);

			bool fragment_changed = m_selfrag != selfrag;

			if (!fragment_changed) return;

			m_selfrag = selfrag;

			bool fragment_value_defined = false;
			pfc::string_formatter fragment_string_value;
			bool fragment_bool_value = false;

			for (t_size index = 0; index < selfrag.get_count(); ++index)
			{
				if (selfrag[index]->kind() != ast::node::kind_comment)
				{
					pfc::string_formatter node_string_value;
					bool node_bool_value = false;

					if (m_debugger.get_value(selfrag[index], node_string_value, node_bool_value))
					{
						fragment_value_defined = true;
						fragment_bool_value |= node_bool_value;
						fragment_string_value << node_string_value;
					}
				}
			}

			auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

			rCtrlScript.SetIndicatorCurrent(indicator_fragment);
			rCtrlScript.SetIndicatorValue(1);
			rCtrlScript.IndicatorClearRange(0, rCtrlScript.GetTextLength());

			for (t_size index = 0; index < selfrag.get_count(); ++index)
			{
				ast::position pos = selfrag[index]->get_position();
				if (pos.start != -1)
				{
					rCtrlScript.IndicatorFillRange(pos.start, pos.end - pos.start);
				}
			}

			auto& rCtrlValue{ GetCtrl(IDC_VALUE) };

			rCtrlValue.MarkerDeleteAll(-1);
			rCtrlValue.SetReadOnly(false);
			rCtrlValue.SetText(fragment_string_value);
			rCtrlValue.SetReadOnly(true);

			if (fragment_value_defined)
			{
				rCtrlValue.MarkerAdd(0, fragment_bool_value ? 1 : 0);
			}
		}
	}
	catch (std::exception const & exc)
	{
		console::formatter() << "[" << COMPONENT_NAME << "] " << "Exception in UpdateFragment(): " << exc;
	}
}

void CTitleFormatSandboxDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == ID_SCRIPT_UPDATE_TIMER)
	{
		UpdateScript();
	}
}

LRESULT CTitleFormatSandboxDialog::OnScriptDwellStart(LPNMHDR pnmh)
{
	SCNotification *scn = (SCNotification *)pnmh;

	if (!m_script_update_pending && m_debugger.get_parser_errors() == 0)
	{
		ast::fragment frag;
		if (find_fragment(frag, scn->position))
		{
			if (frag.get_count() == 1)
			{
				ast::position pos = frag[0]->get_position();
				pfc::string_formatter msg;
				switch (frag[0]->kind())
				{
				case ast::node::kind_comment:
					msg << "Comment";
					break;
				case ast::node::kind_string:
					msg << "String constant";
					break;
				case ast::node::kind_field:
					msg << "Field reference";
					break;
				case ast::node::kind_condition:
					msg << "Conditional section";
					break;
				case ast::node::kind_call:
					msg << "Function call";
					break;
				}

				pfc::string_formatter string_value;
				bool bool_value;

				auto & rCtrlScript{ GetCtrl(IDC_SCRIPT) };

				if (m_debugger.get_value(frag[0], string_value, bool_value))
				{

					int tabSize = pfc::max_t(
						rCtrlScript.TextWidth(STYLE_CALLTIP, "Text value: "),
						rCtrlScript.TextWidth(STYLE_CALLTIP, "Boolean value: "));

					rCtrlScript.CallTipUseStyle(tabSize);

					bool truncated = string_value.truncate_eol() || string_value.limit_length(250, "");
					msg << "\nText value:\t" << string_value;
					if (truncated) msg << "...";
					msg << "\nBoolean value:\t" << (bool_value ? "true" : "false");
				}
				else
				{
					msg << "\nNot evaluated";
				}
				rCtrlScript.CallTipCancel();
				rCtrlScript.CallTipShow(pos.start, msg);
			}
		}
	}

	return 0;
}

LRESULT CTitleFormatSandboxDialog::OnScriptDwellEnd(LPNMHDR pnmh)
{
	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	rCtrlScript.CallTipCancel();
	return 0;
}

LRESULT CTitleFormatSandboxDialog::OnScriptCallTipClick(LPNMHDR pnmh)
{
	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	rCtrlScript.CallTipCancel();
	return 0;
}

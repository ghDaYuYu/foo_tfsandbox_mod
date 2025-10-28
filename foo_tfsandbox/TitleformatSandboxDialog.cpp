#include "stdafx.h"
#include <vector>

#include "libPPUI/win32_utility.h"

#include "Scintilla.h"
#include "ScintillaMessages.h"
#include "ScintillaTypes.h"

#include "scintilla/include/ILexer.h"
#include "lexilla/include/SciLexer.h"

#include "Lexilla.h"

#include "ScintillaCtrlExt.h"

#include "version.h"
#include "colors_json.h"
#include "guids.h"

#include "LibraryModule.h"

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
static advconfig_checkbox_factory cfg_adv_use_console_font("Use foobar2000 console font and font size", "foo_tfsandbox_mod.use_console_font", guid_cfg_adv_use_console_font, guid_tfsandbox_branch, order_use_console_font, 8);
static advconfig_integer_factory cfg_adv_tf_font_size("Stock font size (6..24)", "foo_tfsandbox_mod.tf_font_size", guid_cfg_adv_tf_font_size, guid_tfsandbox_branch, order_tf_font_size, 9, 6 /*minimum value*/, 24 /*maximum value*/);

static COLORREF BlendColor(COLORREF color1, DWORD weight1, COLORREF color2, DWORD weight2)
{
	int r = (GetRValue(color1) * weight1 + GetRValue(color2) * weight2) / (weight1 + weight2);
	int g = (GetGValue(color1) * weight1 + GetGValue(color2) * weight2) / (weight1 + weight2);
	int b = (GetBValue(color1) * weight1 + GetBValue(color2) * weight2) / (weight1 + weight2);
	return RGB(r, g, b);
}

static void LoadMarkerIcon(Scintilla::CScintillaCtrl& sciLexer, int markerNumber, LPCTSTR name)
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

void CTitleFormatSandboxDialog::ui_v2_config_callback::ui_fonts_changed() {

	if (cfg_adv_use_console_font.get()) {

		p_dlg->SetupFonts();

		bool dark = false;
		dark = ui_config_manager::g_is_dark_mode();
		p_dlg->InitControls(dark);
	}
}
//! Called when user changes configuration of colors (also as a result of toggling dark mode). \n
//! Note that for the duration of these callbacks, both old handles previously returned by query_font() as well as new ones are valid; old font objects are released when the callback cycle is complete.
void CTitleFormatSandboxDialog::ui_v2_config_callback::ui_colors_changed() {
	if (!cfg_adv_load_theme_file.get())
	{
		return;
	}

	colors_json cj;
	bool dark = false;
	dark = ui_config_manager::g_is_dark_mode() || is_wine_dark_no_theme;
	cj.read_colors_json(dark);

	p_dlg->InitControls(dark);
}

LONG GetStringRegKey(HKEY hKey, const std::wstring& strValueName, std::wstring& strValue, const std::wstring& strDefaultValue)
{
	strValue = strDefaultValue;
	WCHAR szBuffer[512];
	DWORD dwBufferSize = sizeof(szBuffer);
	ULONG nError;
	nError = RegQueryValueExW(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		strValue = szBuffer;
	}
	return nError;
}

bool CheckDarkLuminance()
{
	bool check_dark = false;
	
	HKEY openKey = HKEY_CURRENT_USER;
	const char* regkey = "Control Panel\\Colors";

	HKEY hKey = 0;
	TCHAR wregkey[MAX_PATH] = { 0 };
	pfc::stringcvt::convert_utf8_to_wide(wregkey, MAX_PATH, regkey, strlen(regkey));

	LONG retValue = RegOpenKeyEx(openKey, wregkey, 0, KEY_READ, &hKey);

	std::wstring strValueOfButtonFace;
	std::wstring strValueOfButtonText;

	bool done = GetStringRegKey(hKey, L"ButtonFace", strValueOfButtonFace, L"") == ERROR_SUCCESS;
	done &= GetStringRegKey(hKey, L"ButtonText", strValueOfButtonText, L"") == ERROR_SUCCESS;

	if (done) {

		pfc::string8 buf = W2U(strValueOfButtonFace.c_str());
		auto tokens = StringSplit(buf.c_str(), ' ');

		COLORREF back = RGB(atoi(tokens[0].c_str()), atoi(tokens[1].c_str()), atoi(tokens[2].c_str()));

		buf = W2U(strValueOfButtonText.c_str());
		tokens = StringSplit(buf.c_str(), ' ');

		COLORREF fore = RGB(atoi(tokens[0].c_str()), atoi(tokens[1].c_str()), atoi(tokens[2].c_str()));

		check_dark = colors_json::is_dark_luminance(fore, back);
	}

	RegCloseKey(hKey);
	return check_dark;
}

CTitleFormatSandboxDialog::CTitleFormatSandboxDialog() : m_dlgResizeHelper(resizeParams), m_script_update_pending(false),
	m_dlgPosTracker(cfg_window_position)
{
	m_dlgResizeHelper.set_min_size(342, 232);
	//m_dlgResizeHelper.m_autoSizeGrip = false;

	std::call_once(is_v2_or_wine_dark_flag, []() {
		is_v2 = core_version_info_v2::get()->test_version(2, 0, 0, 0);
		is_wine_dark_no_theme = false;

		if (IsWine()) {

			bool wine_dark = CheckDarkLuminance();

			TCHAR theme_name[MAX_PATH] = {0};
			HRESULT hres = GetCurrentThemeName(theme_name, MAX_PATH, nullptr, 0, nullptr, 0);
			if (hres == S_OK) {
				hres = GetThemeDocumentationProperty(theme_name,
					SZ_THDOCPROP_DISPLAYNAME, theme_name, MAX_PATH);
			}
			pfc::string8 str_cvt = pfc::stringcvt::string_utf8_from_os(theme_name, MAX_PATH);
			is_wine_dark_no_theme = wine_dark && !str_cvt.length();
		}
	});

	/*bool res = */RegisterScintillaModule(m_scintillaScope);
}

CTitleFormatSandboxDialog::~CTitleFormatSandboxDialog()
{
	if (is_v2) {
		ui_config_manager::get()->remove_callback(m_ui_v2_cfg_callback);
		delete m_ui_v2_cfg_callback;
	}

	if (m_hTreeFont && !m_tree_font_managed) {
		DeleteObject(m_hTreeFont);
		m_hTreeFont = nullptr;
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

		if (dlg->m_scintillaScope.IsLoaded() /*&& dlg->m_lexillaScope.IsLoaded()*/ &&
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

	// DEFAULT STYLE - BACK/FOREGROUND

	COLORREF crcol = get_gen_color(mgen_colors["background"]);

	rCtrlValue.StyleSetBack(STYLE_DEFAULT, crcol);

	crcol = get_gen_color(mgen_colors["foreground"]);

	rCtrlValue.StyleSetFore(STYLE_DEFAULT, crcol);

	// CURSOR/CARET

	rCtrlScript.SetCaretFore(crcol);

	// VALUE SELECTION BACK/FOREGROUND/HIGHLIGHT

	crcol = get_gen_color(mgen_colors["selection background"]);
	rCtrlValue.SetSelBack(STYLE_DEFAULT, crcol);

	crcol = get_gen_color(mgen_colors["selection foreground"]);

	rCtrlValue.SetSelFore(STYLE_DEFAULT, crcol/*RGB(255,0,0)*/);


	// MARKER BACK/FOREGROUND

	crcol = get_gen_color(mgen_colors["marker foreground"]);

	rCtrlValue.MarkerSetFore(STYLE_DEFAULT, crcol);

	crcol = get_gen_color(mgen_colors["marker background"]);

	rCtrlValue.MarkerSetBack(STYLE_DEFAULT, crcol);

	// CODE PAGE

	rCtrlScript.SetCodePage(SC_CP_UTF8);
	rCtrlValue.SetCodePage(SC_CP_UTF8);

	// EVENTS / DWELL

	rCtrlScript.SetModEventMask(Scintilla::ModificationFlags::InsertText | Scintilla::ModificationFlags::DeleteText);
	rCtrlScript.SetMouseDwellTime(500);

	// MAIN SETUPS

	SetupTitleFormatStyles(dark_alpha);
	SetupPreviewStyles();

	// READ ONLY

	rCtrlValue.SetReadOnly(true);

	// IMAGE LIST

#if defined USE_EXPLORER_THEME
	SetWindowTheme(m_treeScript, L"Explorer", NULL);
#endif

	CImageList imageList;

#if defined(USE_EXPLORER_THEME)
	imageList.CreateFromImage(IDB_SYMBOLS32, 16, 2, RGB(255, 0, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
#else

	if (dark_alpha || is_wine_dark_no_theme) {
		imageList.CreateFromImage(IDB_SYMBOLS32_DARK, 16, 2, RGB(255, 0, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
	}
	else {
		imageList.CreateFromImage(IDB_SYMBOLS32, 16, 2, RGB(255, 0, 255), IMAGE_BITMAP, LR_CREATEDIBSECTION);
	}
	else {
		imageList.CreateFromImage(IDB_SYMBOLS, 16, 2, RGB(255, 0, 255), IMAGE_BITMAP);
	}
#endif
	imageList.SetOverlayImage(6, 1);

	m_treeScript.SetImageList(imageList);
}

void CTitleFormatSandboxDialog::SetupTitleFormatStyles(bool dark_alpha)
{

	COLORREF background;
	COLORREF foreground;
	COLORREF selbackground;
	COLORREF selforeground;

	background = get_gen_color(mgen_colors["background"]);
	foreground = get_gen_color(mgen_colors["foreground"]);

	selbackground = get_gen_color(mgen_colors["selection background"]);
	selforeground = get_gen_color(mgen_colors["selection foreground"]);

	//--

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

	//(1) 
	rCtrlScript.StyleSetFore(STYLE_DEFAULT, foreground);
	rCtrlScript.StyleSetBack(STYLE_DEFAULT, background);

	//(2)
	rCtrlScript.StyleClearAll();

	// MARGIN

	rCtrlScript.MarginSetStyle(0, STYLE_LINENUMBER);

	// first margin column for line numbers...

	rCtrlScript.SetMarginTypeN(0, Scintilla::MarginType::Number);
	rCtrlScript.SetMarginWidthN(0, rCtrlScript.TextWidth(STYLE_LINENUMBER, "_999"));

	// second margin column for line numbers...

	rCtrlScript.SetMarginTypeN(1, Scintilla::MarginType::Back);
	rCtrlScript.SetMarginMaskN(1, SC_MASK_FOLDERS);
	rCtrlScript.SetMarginWidthN(1, 16);
	rCtrlScript.SetMarginSensitiveN(1, true);

	//	// line number margin style

	rCtrlScript.StyleSetFore(STYLE_LINENUMBER, foreground);
	rCtrlScript.StyleSetBack(STYLE_LINENUMBER, BlendColor(foreground, 1, background, 20));

	// WINE TREE VIEW

	if (IsWine()) {
		TreeView_SetBkColor(m_treeScript, background);
	}

	rCtrlScript.SetFoldMarginColour(true, background);

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
		rCtrlScript.MarkerSetFore(markerNumber, cr_mf);
		rCtrlScript.MarkerSetBack(markerNumber, cr_mb);
		rCtrlScript.MarkerSetBackSelected(markerNumber, cr_msb);
	}

	rCtrlScript.MarkerDefine(SC_MARKNUM_FOLDEROPEN, Scintilla::MarkerSymbol::BoxMinus/*SC_MARK_BOXMINUS*/);
	rCtrlScript.MarkerDefine(SC_MARKNUM_FOLDER, Scintilla::MarkerSymbol::BoxPlus/*SC_MARK_BOXPLUS*/);
	rCtrlScript.MarkerDefine(SC_MARKNUM_FOLDERSUB, Scintilla::MarkerSymbol::VLine/*SC_MARK_VLINE*/);
	rCtrlScript.MarkerDefine(SC_MARKNUM_FOLDERTAIL, Scintilla::MarkerSymbol::LCorner/*SC_MARK_LCORNER*/);
	rCtrlScript.MarkerDefine(SC_MARKNUM_FOLDEREND, Scintilla::MarkerSymbol::BoxPlusConnected/*SC_MARK_BOXPLUSCONNECTED*/);
	rCtrlScript.MarkerDefine(SC_MARKNUM_FOLDEROPENMID, Scintilla::MarkerSymbol::BoxMinusConnected/*SC_MARK_BOXMINUSCONNECTED*/);
	rCtrlScript.MarkerDefine(SC_MARKNUM_FOLDERMIDTAIL, Scintilla::MarkerSymbol::TCorner/*SC_MARK_TCORNER*/);

	// WRAP AND FOLD

	rCtrlScript.SetAutomaticFold(Scintilla::AutomaticFold::Click/*SC_AUTOMATICFOLD_CLICK*/);
	rCtrlScript.SetWrapMode(Scintilla::Wrap::Word/*SC_WRAP_WORD*/);
	rCtrlScript.SetWrapVisualFlags(Scintilla::WrapVisualFlag::End/*SC_WRAPVISUALFLAG_END*/);

	// SELECTION

	COLORREF cr_tmp = get_gen_color(mgen_colors["selection foreground"]);

	rCtrlScript.SetSelFore(true, cr_tmp);

	cr_tmp = get_gen_color(mgen_colors["selection background"]);

	rCtrlScript.SetSelBack(true, cr_tmp);

	// CALL TIPS

	rCtrlScript.CallTipUseStyle(50);

	cr_tmp = get_gen_color(mgen_colors["calltip foreground"]);

	rCtrlScript.StyleSetFore(STYLE_CALLTIP, cr_tmp);

	cr_tmp = get_gen_color(mgen_colors["calltip background"]);
	rCtrlScript.StyleSetBack(STYLE_CALLTIP, cr_tmp);

	char outlen[MAX_PATH] = {0};

	if (rCtrlScript.GetLexerLanguage(outlen) && !strncmp(outlen, "titleformat",MAX_PATH)) {

		// LEXER

		// Comments
		COLORREF crcol = get_lex_color(mlex_colors["comment"]);

		rCtrlScript.StyleSetFore(1 /*SCE_TITLEFORMAT_COMMENTLINE*/, crcol);
		rCtrlScript.StyleSetFore(1 + 64 /*SCE_TITLEFORMAT_COMMENTLINE*/, BlendColor(crcol, 1, background, 1));

		// Operators
		crcol = get_lex_color(mlex_colors["operator"]);

		rCtrlScript.StyleSetFore(2, /*SCE_TITLEFORMAT_OPERATOR*/crcol);
		rCtrlScript.StyleSetBold(2 /*SCE_TITLEFORMAT_OPERATOR*/, true);
		rCtrlScript.StyleSetFore(2 + 64 /*SCE_TITLEFORMAT_OPERATOR | inactive*/, BlendColor(crcol, 1, background, 1));
		rCtrlScript.StyleSetBold(2 + 64 /*SCE_TITLEFORMAT_OPERATOR | inactive*/, true);

		// Fields
		crcol = get_lex_color(mlex_colors["field"]);

		rCtrlScript.StyleSetFore(3 /*SCE_TITLEFORMAT_FIELD*/, crcol);
		rCtrlScript.StyleSetFore(3 + 64 /*SCE_TITLEFORMAT_FIELD | inactive*/, BlendColor(crcol, 1, background, 1));

		// Strings (Single quoted string)
		crcol = get_lex_color(mlex_colors["literal string"]);

		rCtrlScript.StyleSetFore(4 /*SCE_TITLEFORMAT_STRING*/, crcol);
		rCtrlScript.StyleSetItalic(4 /*SCE_TITLEFORMAT_STRING*/, true);
		rCtrlScript.StyleSetFore(4 + 64 /*SCE_TITLEFORMAT_STRING | inactive*/, BlendColor(crcol, 1, background, 1));
		rCtrlScript.StyleSetItalic(4 + 64 /*SCE_TITLEFORMAT_STRING | inactive*/, true);

		// Text (Unquoted string)
		crcol = get_lex_color(mlex_colors["string"]);

		rCtrlScript.StyleSetFore(5 /*SCE_TITLEFORMAT_LITERALSTRING*/, crcol);
		rCtrlScript.StyleSetFore(5 + 64 /*SCE_TITLEFORMAT_LITERALSTRING | inactive*/, BlendColor(crcol, 1, background, 1));

		// Characters (%%, &&, '')
		crcol = get_lex_color(mlex_colors["special string"]);

		rCtrlScript.StyleSetFore(6 /*SCE_TITLEFORMAT_SPECIALSTRING*/, crcol);
		rCtrlScript.StyleSetFore(6 + 64/*SCE_TITLEFORMAT_SPECIALSTRING | inactive*/, BlendColor(crcol, 1, background, 1));

		// Functions (todo:identifier?)
		crcol = get_lex_color(mlex_colors["identifier"]);
		rCtrlScript.StyleSetFore(7, /*SCE_TITLEFORMAT_IDENTIFIER*/ crcol);
		rCtrlScript.StyleSetFore(7 + 64 /*SCE_TITLEFORMAT_IDENTIFIER | inactive*/, BlendColor(crcol, 1, background, 1));
	}

	// INDICATORS

	// inactive code
	tfRGB col = vindicator_colors[0].second;
	COLORREF crcol = RGB(col.r, col.g, col.b);

	rCtrlScript.IndicSetStyle(indicator_inactive_code, Scintilla::IndicatorStyle::StraightBox/*INDIC_STRAIGHTBOX*/);
	rCtrlScript.IndicSetFore(indicator_inactive_code, crcol);
	rCtrlScript.IndicSetUnder(indicator_inactive_code, true);

	rCtrlScript.Call(SCI_INDICSETALPHA, indicator_inactive_code, 20);
	rCtrlScript.Call(SCI_INDICSETOUTLINEALPHA, indicator_inactive_code, 20);

	// selected fragment
	col = vindicator_colors[1].second;
	crcol = RGB(col.r, col.g, col.b);

	rCtrlScript.IndicSetStyle(indicator_fragment, Scintilla::IndicatorStyle::StraightBox/*INDIC_STRAIGHTBOX*/);
	rCtrlScript.IndicSetFore(indicator_fragment, /*RGB(255,0,0)*/crcol);
	rCtrlScript.IndicSetUnder(indicator_fragment, true);

	if (dark_alpha) {
		auto alpha = rCtrlScript.IndicGetAlpha(indicator_fragment);
		//todo
		int alpha_val = (std::min)(255, (int)alpha + 30);
		rCtrlScript.Call(static_cast<UINT>(Scintilla::Message::IndicSetAlpha), static_cast<WPARAM>(indicator_fragment), static_cast<LPARAM>(alpha_val));
	}

	// errors
	col = vindicator_colors[2].second;
	crcol = RGB(col.r, col.g, col.b);

	rCtrlScript.IndicSetStyle(indicator_error, Scintilla::IndicatorStyle::Squiggle/*INDIC_SQUIGGLE*/);
	rCtrlScript.IndicSetFore(indicator_error, crcol);
	rCtrlScript.Call(SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");
}

void CTitleFormatSandboxDialog::SetupFonts() {

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

	// CALLTIPS
	
	rCtrlScript.StyleSetFont(STYLE_CALLTIP, "Verdana");
	
	// LEXER

	LOGFONT lf_desired_ui_font;
	memset(&lf_desired_ui_font, 0, sizeof(LOGFONT));
	int desired_ui_font_size = 0;

	auto PointSize = 0;

	if (is_v2 && cfg_adv_use_console_font.get()) {

		auto ui_cfg_mng = ui_config_manager::get();

		m_hFont = ui_cfg_mng->query_font(ui_font_console);

		CFont cFont;
		cFont.Attach(m_hFont);
		cFont.GetLogFont(lf_desired_ui_font);
		desired_ui_font_size = lf_desired_ui_font.lfHeight;
		char str[MAX_PATH] = {0};
		auto wlength = pfc::stringcvt::convert_wide_to_utf8(str, MAX_PATH, lf_desired_ui_font.lfFaceName, MAX_PATH);
		LPSTR lpstr = const_cast<char*>(str);

		// set font

		rCtrlScript.StyleSetFont(STYLE_DEFAULT, lpstr);
		rCtrlScript.StyleSetFont(STYLE_LINENUMBER, lpstr);

		//todo
		CWindowDC dc(m_editor);
		int LogPixelsY =  GetDeviceCaps(dc, LOGPIXELSY);
		PointSize = MulDiv(-lf_desired_ui_font.lfHeight, 72, LogPixelsY);

		//todo: config panel

		// font size

		//default font
		rCtrlScript.StyleSetSizeFractional(STYLE_DEFAULT, PointSize * 100);
		// line-number margin font
		rCtrlScript.StyleSetSizeFractional(STYLE_LINENUMBER, PointSize * 100);

		cFont.Detach();
	}
	else {
		rCtrlScript.StyleSetFont(STYLE_DEFAULT, "Consolas");
		rCtrlScript.StyleSetSizeFractional(STYLE_DEFAULT, cfg_adv_tf_font_size.get() * 100);
		m_font_managed = true;
	}

	// VALUES (font size)

	auto& rCtrlValue{ GetCtrl(IDC_VALUE) };
	if (is_v2 && cfg_adv_use_console_font.get()) {
		rCtrlValue.StyleSetSizeFractional(STYLE_DEFAULT, PointSize/*cfg_adv_tf_font_size.get()*/ * 100);
		rCtrlValue.StyleSetSizeFractional(STYLE_LINENUMBER, PointSize/*cfg_adv_tf_font_size.get()*/ * 100);
	}
	else {
		int desiredHeight = cfg_adv_tf_font_size.get();
		rCtrlValue.StyleSetSizeFractional(STYLE_DEFAULT, desiredHeight * 100);
		rCtrlValue.StyleSetSizeFractional(STYLE_LINENUMBER, desiredHeight * 100);
	}

	// TREE

	if (is_v2 && cfg_adv_use_console_font.get()) {

		auto ui_cfg_mng = ui_config_manager::get();

		if (!m_hFont) {
			m_hFont = ui_cfg_mng->query_font(ui_font_console);
			m_tree_font_managed = true;
		}

		BOOL redraw = TRUE;
		::SendMessage(m_treeScript, WM_SETFONT, (WPARAM)m_hFont, redraw);

	}
	else {

		//"Consolas",...

		if (m_hTreeFont && !m_tree_font_managed) {
			DeleteObject(m_hTreeFont);
			m_hTreeFont = nullptr;
		}

		int desiredHeight = cfg_adv_tf_font_size.get();

		CWindowDC dc(m_editor);
		const int height = -MulDiv(desiredHeight, dc.GetDeviceCaps(LOGPIXELSY), /*dpi*/72);
		ReleaseDC(dc);

		m_hTreeFont = CreateFont(height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Consolas"));

		if (m_hTreeFont) {
			m_tree_font_managed = false;
			BOOL redraw = TRUE;
			::SendMessage(m_treeScript, WM_SETFONT, (WPARAM)m_hTreeFont, redraw);
		}
	}
}

void CTitleFormatSandboxDialog::SetupPreviewStyles()
{

	auto& rCtrlValue{ GetCtrl(IDC_VALUE) };

	COLORREF background = get_gen_color(mgen_colors["background"]);
	COLORREF foreground = get_gen_color(mgen_colors["foreground"]);

	// WRAP

	rCtrlValue.SetWrapMode(Scintilla::Wrap::Word/*(SC_WRAP_WORD)*/);
	rCtrlValue.SetWrapVisualFlags(Scintilla::WrapVisualFlag::End/*(SC_WRAPVISUALFLAG_END)*/);

	// MARGINS

	// 0

	rCtrlValue.SetMarginTypeN(0, Scintilla::MarginType::Number/*, SC_MARGIN_NUMBER*/);
	rCtrlValue.SetMarginWidthN(0, rCtrlValue.TextWidth(STYLE_LINENUMBER, "_999"));

	// 1

	rCtrlValue.SetMarginTypeN(1,Scintilla::MarginType::Back/*, SC_MARGIN_BACK*/);
	rCtrlValue.SetMarginWidthN(1, 16); //pixel width

	// line number margin style

	rCtrlValue.StyleSetFore(STYLE_LINENUMBER, foreground);
	rCtrlValue.StyleSetBack(STYLE_LINENUMBER, BlendColor(foreground, 1, background, 20));

	// MARKER ICONS

	LoadMarkerIcon(rCtrlValue, 0, MAKEINTRESOURCE(IDI_FUGUE_CROSS));
	LoadMarkerIcon(rCtrlValue, 1, MAKEINTRESOURCE(IDI_FUGUE_TICK));
	LoadMarkerIcon(rCtrlValue, 2, MAKEINTRESOURCE(IDI_FUGUE_EXCLAMATION));

	// FORE/BACKGROUND (LEXER NOT DEFINED)

	rCtrlValue.StyleSetFore(0, foreground);
	rCtrlValue.StyleSetBack(0, background);
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

	SetIcon(static_api_ptr_t<ui_control>()->get_main_icon());

	if (is_v2 && m_ui_v2_cfg_callback == nullptr) {
		m_ui_v2_cfg_callback = new ui_v2_config_callback(this);
		ui_config_manager::get()->add_callback(m_ui_v2_cfg_callback);
	}

	m_editor = GetDlgItem(IDC_SCRIPT);
	m_preview = GetDlgItem(IDC_VALUE);

	m_treeScript.Attach(GetDlgItem(IDC_TREE));

	// --------- INSTANCIATE FT LEXER -------------------

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };
	char outlen[MAX_PATH] = { 0 };
	if (rCtrlScript.GetLexerLanguage(outlen) && !strncmp(outlen, "titleformat", MAX_PATH)) {
		m_lexillaScope.SetWasRegistered(true);
	}
	else {
		if (RegisterLexillaModule(m_lexillaScope)) {

			FARPROC lexer_tf_create_fun = GetProcAddress(m_lexillaScope.GetModule(), LEXILLA_CREATELEXER);

			Lexilla::CreateLexerFn lexilla_tf_create = reinterpret_cast<Lexilla::CreateLexerFn>(lexer_tf_create_fun);

			char lexer_tf_name[MAX_PATH] = "titleformat";
			void* pLexer = lexilla_tf_create(lexer_tf_name);
			::SendMessage(m_editor, SCI_SETILEXER, 0, (LPARAM)pLexer);
		}
	}

	m_dark.AddDialog(m_hWnd);
	m_dark.AddControls(m_hWnd);

	bool dark = false;
	colors_json cj;

	std::vector<std::pair<size_t, tfRGB>> vcolors;

	if (cfg_adv_load_theme_file.get()) {

		if (!is_v2) {
				cj.read_colors_json(false);
		}
		else {
				dark = ui_config_manager::g_is_dark_mode() || is_wine_dark_no_theme;
				cj.read_colors_json(dark);
		}
	}
	else {
		// no theme - default colors
		vgen_colors = vgen_colors_defaults;
		vlex_colors = vlex_colors_defaults;
		vindicator_colors = vindicator_colors_defaults;
	}

	SetupFonts();
	InitControls(dark);

	auto& rCtrlScript{ GetCtrl(IDC_SCRIPT) };

	m_script_update_pending = true;
	//m_updating_fragment = false;

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
		CTreeViewCtrl treeView = nullptr;
		HTREEITEM parent = {};
		t_size param_index = 0;

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
	if (m_script_update_pending) {

		return;

	}

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

					int tabSize = pfc::max_t(rCtrlScript.TextWidth(STYLE_CALLTIP, "Text value: "),
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

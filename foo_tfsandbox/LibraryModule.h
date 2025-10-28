#pragma once
#include "stdafx.h"
#include "LibraryScope.h"
#include "version.h"

#ifndef U2T
#define U2T(Text) pfc::stringcvt::string_os_from_utf8(Text).get_ptr()
#endif

//todo

inline bool RegisterScintillaModule(CLibraryScope& libScope) {

	pfc::string8 install_dir = pfc::string_directory(core_api::get_my_full_path());
	pfc::string8 scintilla_path = PFC_string_formatter() << install_dir << "\\Scintilla.dll";

	try {
		try {
			WNDCLASSW wc = { 0 };
			bool loaded = false;
			if (GetClassInfo(core_api::get_my_instance(), _T("tfsm_Scintilla"), &wc)) {
				libScope.SetWasRegistered(true);
			}

			bool v = !libScope.WasRegistered() && filesystem::g_exists(scintilla_path, fb2k::noAbort);

			if (v) {
				libScope.LoadLibrary(U2T(scintilla_path.get_ptr()));
			}
		}
		catch (std::exception const& e) {
			console::formatter() << "[" << COMPONENT_NAME << "] :" << "Componenet error loading scindilla.dll: " << e << " (on: " << scintilla_path << ")";
			throw;
		}
	}
	catch (exception_io_denied const&) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "Access denied.";//"Componenet error loading lexilla.dll: " << e << " (on: " << lexilla_path << ")";
		//return;
	}
	catch (exception_io_sharing_violation const&) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "IO sharing violation.";
		//return;
	}
	catch (exception_io_file_corrupted const&) { // happens
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "File corrupted.";
		//return;
	}
	catch (...) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "Unknown error.";
		//return;
	}

	if (!libScope.IsLoaded()) {
		auto le = GetLastError();
		console::formatter() << "[" << COMPONENT_NAME << "] :" << scintilla_path << " not loaded." << " Last error: " << le;
		return false;
	}
	return true;
}

inline bool RegisterLexillaModule(CLibraryScope& libScope) {

	pfc::string8 install_dir = pfc::string_directory(core_api::get_my_full_path());
	pfc::string8 lexilla_path = PFC_string_formatter() << install_dir << "\\Lexilla.dll";

	try {
		try {
			WNDCLASSW wc = { 0 };
			bool loaded = false;
			if (GetClassInfo(core_api::get_my_instance(), _T("Lexilla"), &wc)) {
				libScope.SetWasRegistered(true);
			}

			bool v = !libScope.WasRegistered() && filesystem::g_exists(lexilla_path, fb2k::noAbort);

			if (v) {
				libScope.LoadLibrary(U2T(lexilla_path.get_ptr()));
			}
		}
		catch (std::exception const& e) {
			console::formatter() << "[" << COMPONENT_NAME << "] :" << "Componenet error loading lexilla.dll: " << e << " (on: " << lexilla_path << ")";
			throw;
		}
	}
	catch (exception_io_denied const&) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "Access denied.";//"Componenet error loading lexilla.dll: " << e << " (on: " << lexilla_path << ")";
		//return;
	}
	catch (exception_io_sharing_violation const&) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "IO sharing violation.";
		//return;
	}
	catch (exception_io_file_corrupted const&) { // happens
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "File corrupted.";
		//return;
	}
	catch (...) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << "Unknown error.";
		//return;
	}

	if (!libScope.IsLoaded()) {
		console::formatter() << "[" << COMPONENT_NAME << "] :" << lexilla_path << " not loaded.";
		return false;
	}
	return true;
}
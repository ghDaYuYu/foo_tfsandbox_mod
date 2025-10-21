// foo_tfsandbox.cpp : Defines the exported functions for the DLL application.
//
#include "SDK/initquit.h"
#include "stdafx.h"
#include "version.h"
#include "colors_json.h"

DECLARE_COMPONENT_VERSION(
	PLUGIN_LONG_NAME,
	PLUGIN_VERSION,
	PLUGIN_ABOUT
);

FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;


class initquit_tfsandbox : public initquit
{
	virtual void on_init() override {
		/*bool res = */colors_json::copy_installation_theme_files();
	}
	virtual void on_quit() override {

	}
};

static initquit_factory_t<initquit_tfsandbox> foo_initquit;
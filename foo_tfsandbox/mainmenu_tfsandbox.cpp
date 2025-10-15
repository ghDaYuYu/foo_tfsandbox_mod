#include "stdafx.h"
#include "TitleformatSandboxDialog.h"
#include "string_from_resources.h"
#include "mainmenu_commands_impl_single.h"

class mainmenu_commands_tfsandbox : public mainmenu_commands_impl_single
{
public:
	virtual GUID get_command()
	{
		// {C3503BB8-2665-418C-9E05-6F9602B12F6A} mod
		static const GUID guid = { 0xc3503bb8, 0x2665, 0x418c, { 0x9e, 0x5, 0x6f, 0x96, 0x2, 0xb1, 0x2f, 0x6a } };

		return guid;
	}

	virtual void get_name(pfc::string_base & p_out)
	{
		p_out = string_from_resources(IDS_CMD_TFDBG_NAME);
	}

	virtual bool get_description(pfc::string_base & p_out)
	{
		p_out = string_from_resources(IDS_CMD_TFDBG_DESCRIPTION);
		return true;
	}

	virtual GUID get_parent()
	{
		return mainmenu_groups::view;
	}

	virtual void execute(service_ptr_t<service_base> p_callback)
	{
		CTitleFormatSandboxDialog::ActivateDialog();
	}
};

static service_factory_single_t<mainmenu_commands_tfsandbox> g_mainmenu_commands_tfsandbox_factory;

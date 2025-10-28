#pragma once

class CLibraryScope
{
public:
	CLibraryScope();
	explicit CLibraryScope(LPCTSTR pszName);
	~CLibraryScope();

	bool LoadLibrary(LPCTSTR pszName);

	bool IsLoaded() const {return m_hDll != NULL || m_was_registered;}

	void SetWasRegistered(bool was_registered) { m_was_registered = was_registered; }
	bool WasRegistered() { return m_was_registered; }

	HMODULE GetModule() { return m_hDll; }

private:
	bool m_was_registered = false;
	HMODULE m_hDll;
};


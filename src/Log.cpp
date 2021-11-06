#include "Log.h"

//#define FMT_HEADER_ONLY
//#include <fmt\xchar.h>

#include <spdlog/spdlog.h>
#include <wx/wx.h>


CMyLogger::CMyLogger()
{

	if (m_file.Exists(m_filename))
	{
		if (!m_file.Access(m_filename, wxFile::write))
		{
			wxLogFatalError("Unable to open log file.");
		}
	}

	m_file.Open(m_filename, wxFile::write);

}

void CMyLogger::DoLogText(const wxString& msg)
{
	bool success = m_file.Write(msg);

	if (!success)
	{
		wxLogError("Failed to Write to Log File.");
	}
}
/*
* The release version is designed to run as an administrator
* UAC Execution Level /level=requireAdministrator
* The debug version leaves UAC alone.
* 
* I'm building to x64 primarily because fmt library
* only 64-bit.
*/

#pragma warning(disable : 4996)
#include <wx/wx.h>

#include "NPClient.h"
#include "Display.h"
#include "Watchdog.h"
#include "Track.h"
#include "Exceptions.h"
#include "Config.h"
#include "Log.h"

#define FMT_HEADER_ONLY
#include <fmt\format.h>
#include <fmt\xchar.h>

#define USHORT_MAX_VAL 65535 // SendInput with absolute mouse movement takes a short int

bool g_pauseTracking = false;

// Uncomment this line for testing to prevent program
// from attaching to NPTrackIR and supersede control
//#define TEST_NO_TRACK

//void disconnectTrackIR(void);

CTracker::CTracker(wxEvtHandler* m_parent, HWND hWnd, CConfig* config)
{

	// ## Program flow ##
	// 
	// first thing is ping windows for info
	// validate settings and build display structures
	// start watchdog thread
	// load trackir dll and getproc addresses
	// connect to trackir and request data type
	// receive data frames
	// decode data
	// hand data off to track program
	// calculate mouse position
	// form & send mouse move command

	//Error Conditions To Check For RAII:
	// - unable to initialize thread
	// - unable to load dll
	// - any NP dll function call failure



	logToWix(fmt::format("Starting Initialization Of TrackIR\n"));

	WinSetup();
	
	DisplaySetup(config);

	// After settings are loaded, start accepting connections & msgs
	// on a named pipe to externally controll the track IR process
	// Start the watchdog thread
	if ((config)->bWatchdog)
	{
		// Watchdog thread may return NULL
		m_hWatchdogThread = WatchDog::WD_StartWatchdog();

		if (NULL == m_hWatchdogThread)
		{
			logToWix("Watchdog thread failed to initialize!");
		}
	}

	logToWix(fmt::format("\n{:-^50}\n", "TrackIR Init Status"));

	// Find and load TrackIR DLL
	TCHAR sDll[MAX_PATH];
	std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>> convert;
	std::wstring wide_string = convert.from_bytes((config) -> sTrackIR_dll_location);
	const wchar_t* temp_wide_string = wide_string.c_str();
	wcscpy_s(sDll, MAX_PATH, temp_wide_string);

	// Load trackir dll and resolve function addresses
	NPRESULT rslt = NP_OK;
	rslt = NPClient_Init(sDll);

	if (NP_OK == rslt)
	{
		logToWix("NP Initialization:      Succeeded\n");
	}
	else if (NP_ERR_DLL_NOT_FOUND == rslt)
	{
		logToWix(fmt::format("\nFAILED TO LOAD TRACKIR DLL: {}\n\n", rslt));
		throw Exception("Failed to load track IR DLL.");
	}
	else
	{
		logToWix(fmt::format("\nNP INITIALIZATION FAILED WITH CODE: {}\n\n", rslt));
		throw Exception("Failed to initialize track IR.");
	}

	// NP needs a window handle to send data frames to
	HWND hConsole = hWnd;

	// In the program early after the DLL is loaded for testing
	// so that this program doesn't boot control
	// from my local copy.
#ifndef TEST_NO_TRACK
	// NP_RegisterWindowHandle
	rslt = NP_OK;
	rslt = NP_RegisterWindowHandle(hConsole);

	// 7 is a magic number I found through experimentation.
	// It means another program has its window handle registered
	// already.
	if (rslt == 7)
	{
		NP_UnregisterWindowHandle();
		logToWix(fmt::format("\nBOOTING CONTROL OF PREVIOUS MOUSETRACKER INSTANCE!\n\n"));
		Sleep(2);
		rslt = NP_RegisterWindowHandle(hConsole);
	}

	if (NP_OK == rslt)
	{
		logToWix("Register Window Handle:      Succeeded\n");
	}
	else
	{
		logToWix(fmt::format("Register Window Handle: Failed {:>3}\n", rslt));
		throw Exception("");
	}

	// I'm skipping query the software version, I don't think its necessary

	// Request roll, pitch. See NPClient.h
	rslt = NP_RequestData(NPPitch | NPYaw);
	if (NP_OK == rslt)
	{
		logToWix("Request Data:         Succeeded\n");
	}
	else
	{
		logToWix(fmt::format("Request Data:        Failed: {:>3}\n", rslt));
		throw Exception("");
	}

	rslt = NP_RegisterProgramProfileID((config)->profile_ID);
	if (NP_OK == rslt)
	{
		logToWix("Register Program Profile ID:         Succeeded\n");
	}
	else
	{
		logToWix(fmt::format("Register Program Profile ID:       Failed: {:>3}\n", rslt));
		throw Exception("");
	}

#endif
}

int CTracker::trackStart(CConfig* config)
{
#ifndef TEST_NO_TRACK
	// Skipping this too. I think this is for legacy games
	// NP_StopCursor

	NPRESULT rslt = NP_StartDataTransmission();
	logToWix(fmt::format("Start Data Transmission:     {:>3}\n", rslt));
#endif

	NPRESULT gdf;
	tagTrackIRData* pTIRData, TIRData;
	pTIRData = &TIRData;

	unsigned short last_frame = 0;
	int dropped_frames = 0;

	while(true)
	{
		gdf = NP_GetData(pTIRData);
		if (NP_OK == gdf)
		{
			unsigned short status = (*pTIRData).wNPStatus;
			unsigned short framesig = (*pTIRData).wPFrameSignature;
			float yaw = -(*pTIRData).fNPYaw; // Make Future optimization to not need a negative sign
			float pitch = -(*pTIRData).fNPPitch;

			// Don't try to move the mouse when TrackIR is paused
			if (framesig == last_frame)
			{
				// logToWix(fmt::format("Tracking Paused\n");
				// TrackIR5 supposedly operates at 120hz
				// 8ms is approximately 1 frame
				Sleep(8);
				continue;
			}

			if (g_pauseTracking == false)
			{
				MouseMove((config)->m_iMonitorCount, yaw, pitch);
			}

			// If I would like to log rotational data
			//logToWix(fmt::format("fNPYaw: {:f}\n", yaw));
			//logToWix(fmt::format("fNPPitch: {:f}\n", pitch));

			// I don't actually really care about dropped frames
			//if ((framesig != last_frame + 1) && (last_frame != 0))
			//{
				//logToWix(fmt::format("dropped"));
				//dropped_frames = dropped_frames + framesig - last_frame - 1;
			//}

			last_frame = framesig;
		}
		else if (NP_ERR_DEVICE_NOT_PRESENT == gdf)
		{
			logToWix(fmt::format("\n\nDEVICE NOT PRESENT\nSTOPPING TRACKING...\nPLEASE RESTART PROGRAM\n\n"));
			break;
		}

		Sleep(8);
	}
	
	return 0;
}

void CTracker::trackStop()
{
	NP_StopDataTransmission();
	NP_UnregisterWindowHandle();
}

BOOL CTracker::WrapperPopulateVirtMonitorBounds(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM lParam)
{
	CTracker* pThis = reinterpret_cast<CTracker*>(lParam);
	BOOL winreturn = pThis->PopulateVirtMonitorBounds(hMonitor, hdcMonitor, lprcMonitor);
	return true;
}

BOOL CTracker::PopulateVirtMonitorBounds(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor)
{
	static int count{ 0 };
	MONITORINFOEX Monitor;
	Monitor.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMonitor, &Monitor);

	m_displays[count].pix_left = static_cast <unsigned int> (Monitor.rcMonitor.left); // from long
	m_displays[count].pix_right = static_cast <unsigned int> (Monitor.rcMonitor.right); // from long
	m_displays[count].pix_top = static_cast <unsigned int> (Monitor.rcMonitor.top); // from long
	m_displays[count].pix_bottom = static_cast <unsigned int> (Monitor.rcMonitor.bottom); // from long

	// Remember to not use {s} for string types. This causes an error for some reason.
	logToWix(fmt::format(L"MON Name:{:>15}\n", Monitor.szDevice));

	logToWix(fmt::format("MON {} Left:   {:>10}\n", count, m_displays[count].pix_left));
	logToWix(fmt::format("MON {} Right:  {:>10}\n", count, m_displays[count].pix_right));
	logToWix(fmt::format("MON {} Top:    {:>10}\n", count, m_displays[count].pix_top));
	logToWix(fmt::format("MON {} Bottom: {:>10}\n", count, m_displays[count].pix_bottom));

	if (Monitor.rcMonitor.left < m_virt_origin_x)
	{
		m_virt_origin_x = Monitor.rcMonitor.left;
	}
	if (Monitor.rcMonitor.top < m_virt_origin_y)
	{
		m_virt_origin_y = Monitor.rcMonitor.top;
	}

	count++;
	return true;
};

void CTracker::WinSetup()
{
	logToWix(fmt::format("\n{:-^50}\n", "Windows Environment Info"));

	int num_monitors = GetSystemMetrics(SM_CMONITORS);
	int VM_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);  // width of total bounds of all screens
	int VM_height = GetSystemMetrics(SM_CYVIRTUALSCREEN); // height of total bounds of all screens

	logToWix(fmt::format("{} Monitors Found\n", num_monitors));
	logToWix(fmt::format("Width of Virtual Desktop:  {:>5}\n", VM_width));
	logToWix(fmt::format("Height of Virtual Desktop: {:>5}\n", VM_height));

	if (DEFAULT_MAX_DISPLAYS < num_monitors)
	{
		logToWix(fmt::format("More Than {} Displays Found.\nIncrease max number of m_displays.\n", DEFAULT_MAX_DISPLAYS));
	}

	/* #################################################################
	For a future feature to automatically detect monitor configurations
	and select the correct profile.

	DISPLAY_DEVICEA display_device_info;
	display_device_info.cb = sizeof(DISPLAY_DEVICEA);

	for (int i = 0; ; i++)
	{
		BOOL result = EnumDisplayDevicesA(
			0,
			i,
			&display_device_info,
			0
		);

		if (result)
		{
			//std::cout << i << " Name: " << display_device_info.DeviceName << "  DeviceString: " << display_device_info.DeviceString << std::endl;
			logToWix(fmt::format("{} Name: {} DeviceString: {}\n", i, display_device_info.DeviceName, display_device_info.DeviceString));
		}
		else
		{
			break;
		}
	}
	######################################################################## */

	m_x_PxToABS = USHORT_MAX_VAL / static_cast<float>(VM_width);
	m_y_PxToABS = USHORT_MAX_VAL / static_cast<float>(VM_height);

	logToWix(fmt::format("\nVirtual Desktop Pixel Bounds\n"));
	EnumDisplayMonitors(NULL, NULL, WrapperPopulateVirtMonitorBounds, reinterpret_cast<LPARAM>(this));

	logToWix(fmt::format("\nVirtual Origin Offset Horizontal: {:d}\n", m_virt_origin_x));
	logToWix(fmt::format("Virtual Origin Offset Vertical:   {:d}\n", m_virt_origin_y));

	return;
}

void CTracker::DisplaySetup(CConfig* config)
{
	for (int i = 0; i < config->m_iMonitorCount; i++)
	{
		m_displays[i].rot_left = (config)->bounds[i].left;
		m_displays[i].rot_right = (config)->bounds[i].right;
		m_displays[i].rot_top = (config)->bounds[i].top;
		m_displays[i].rot_bottom = (config)->bounds[i].bottom;

		m_displays[i].left_padding = (config)->bounds[i].pad_left;
		m_displays[i].right_padding = (config)->bounds[i].pad_right;
		m_displays[i].top_padding = (config)->bounds[i].pad_top;
		m_displays[i].bottom_padding = (config)->bounds[i].pad_bottom;

		m_displays[i].setAbsBounds(m_virt_origin_x, m_virt_origin_y, m_x_PxToABS, m_y_PxToABS);
	}
	logToWix(fmt::format("\nVirtual Desktop Pixel Bounds (abs)\n"));
	for (int i = 0; i < config->m_iMonitorCount; i++)
	{
		logToWix(fmt::format("MON {} pix_abs_left:   {:>10d}\n", i, m_displays[i].pix_abs_left));
		logToWix(fmt::format("MON {} pix_abs_right:  {:>10d}\n", i, m_displays[i].pix_abs_right));
		logToWix(fmt::format("MON {} pix_abs_top:    {:>10d}\n", i, m_displays[i].pix_abs_top));
		logToWix(fmt::format("MON {} pix_abs_bottom: {:>10d}\n", i, m_displays[i].pix_abs_bottom));
	}
	logToWix(fmt::format("\n16-bit Coordinate Bounds\n"));
	for (int i = 0; i < config->m_iMonitorCount; i++)
	{
		logToWix(fmt::format("MON {} abs_left:       {:>12.1f}\n", i, m_displays[i].abs_left));
		logToWix(fmt::format("MON {} abs_right:      {:>12.1f}\n", i, m_displays[i].abs_right));
		logToWix(fmt::format("MON {} abs_top:        {:>12.1f}\n", i, m_displays[i].abs_top));
		logToWix(fmt::format("MON {} abs_bottom:     {:>12.1f}\n", i, m_displays[i].abs_bottom));
	}
	logToWix(fmt::format("\nRotational Bounds\n"));
	for (int i = 0; i < config->m_iMonitorCount; i++)
	{
		logToWix(fmt::format("MON {} rot_left:       {:>13.2f}\n", i, m_displays[i].rot_left));
		logToWix(fmt::format("MON {} rot_right:      {:>13.2f}\n", i, m_displays[i].rot_right));
		logToWix(fmt::format("MON {} rot_top:        {:>13.2f}\n", i, m_displays[i].rot_top));
		logToWix(fmt::format("MON {} rot_bottom:     {:>13.2f}\n", i, m_displays[i].rot_bottom));
	}

	logToWix(fmt::format("\n{:-^}\n", ""));

	return;
}

inline void SendMyInput(float x, float y)
{
	static MOUSEINPUT mi = {
		0,
		0,
		0,
		MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK,
		0,
		0
	};

	static INPUT ip = {
		INPUT_MOUSE,
		mi
	};

	ip.mi.dx = static_cast<LONG>(x);
	ip.mi.dy = static_cast<LONG>(y);

	SendInput(1, &ip, sizeof(INPUT));

	return;
}

void CTracker::MouseMove(int num_monitors, float yaw, float pitch)
{

	// set the last screen initially equal to the main display for me
	// TODO: find a way to specify in settings or get from windows
	static int last_screen = 1;

	// variables used for linear interpolation
	static float rl;
	static float al;
	static float mx;
	static float x;
	static float rt;
	static float at;
	static float my;
	static float y;

	// Check if the head is pointing to a screen
	// The return statement is never reached if the head is pointing outside the bounds of any of the screens

	for (int i = 0; i <= num_monitors - 1; i++)
	{
		if ((yaw > m_displays[i].rot_s15bit_left) && (yaw < m_displays[i].rot_s15bit_right) && (pitch < m_displays[i].rot_s15bit_top) && (pitch > m_displays[i].rot_s15bit_bottom))
		{
			// I wrote it out for maintainability
			// its plenty fast anyway for a 60hz limited display
			rl = m_displays[i].rot_s15bit_left;
			al = m_displays[i].abs_left;
			mx = m_displays[i].xSlope;
			x = mx * (yaw - rl) + al;
			rt = m_displays[i].rot_s15bit_top;
			at = m_displays[i].abs_top;
			my = m_displays[i].ySlope;
			y = my * (rt - pitch) + at;
			// load the coordinates into my input structure
			// need to cast to an integer because resulting calcs are floats
			SendMyInput(x, y);
			last_screen = i;
			//logToWix(fmt::format("(%f, %f)", y, x); // for testing
			return;
		}
	}

	// If the head is pointing outside of the bounds of a screen the mouse should snap to the breached edge
	// It could either be the pitch or the yaw axis that is too great or too little
	// To do this assume the pointer came from the last screen, just asign the mouse position to the absolute limit from the screen it came from
	if (yaw < m_displays[last_screen].rot_s15bit_left)
	{
		x = m_displays[last_screen].abs_left + m_displays[last_screen].left_padding * m_x_PxToABS;
	}
	else if (yaw > m_displays[last_screen].rot_s15bit_right)
	{
		x = m_displays[last_screen].abs_right - m_displays[last_screen].right_padding * m_x_PxToABS;
	}
	else
	{
		rl = m_displays[last_screen].rot_s15bit_left;
		al = m_displays[last_screen].abs_left;
		mx = m_displays[last_screen].xSlope;
		x = mx * (yaw - rl) + al;
	}

	if (pitch > m_displays[last_screen].rot_s15bit_top)
	{
		y = m_displays[last_screen].abs_top + m_displays[last_screen].top_padding * m_y_PxToABS;
	}
	else if (pitch < m_displays[last_screen].rot_s15bit_bottom)
	{
		y = m_displays[last_screen].abs_bottom - m_displays[last_screen].bottom_padding * m_y_PxToABS;
	}
	else
	{
		rt = m_displays[last_screen].rot_s15bit_top;
		at = m_displays[last_screen].abs_top;
		my = m_displays[last_screen].ySlope;
		y = my * (rt - pitch) + at;
	}

	// logToWix(fmt::format("off monitors"));
	SendMyInput(x, y);

	return;

}
// Future function to implement
int getDllLocationFromRegistry()
{
	TCHAR szPath[MAX_PATH * 2];
	HKEY pKey = NULL;
	LPTSTR pszValue;
	DWORD dwSize;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\NaturalPoint\\NATURALPOINT\\NPClient Location"), 0, KEY_READ, &pKey) != ERROR_SUCCESS)
	{
		//MessageBox(hWnd, _T("DLL Location key not present"), _T("TrackIR Client"), MB_OK);
		logToWix(fmt::format("Registry Error: DLL Location key not present"));
		return false;
	}
	if (RegQueryValueEx(pKey, _T("Path"), NULL, NULL, NULL, &dwSize) != ERROR_SUCCESS)
	{
		//MessageBox(hWnd, _T("Path value not present"), _T("TrackIR Client"), MB_OK);
		logToWix(fmt::format("Registry Error: Path value not present"));
		return false;
	}
	pszValue = (LPTSTR)malloc(dwSize);
	if (pszValue == NULL)
	{
		//MessageBox(hWnd, _T("Insufficient memory"), _T("TrackIR Client"), MB_OK);
		logToWix(fmt::format("Registry Error: Insufficient memory"));
		return false;
	}
	if (RegQueryValueEx(pKey, _T("Path"), NULL, NULL, (LPBYTE)pszValue, &dwSize) != ERROR_SUCCESS)
	{
		::RegCloseKey(pKey);
		//MessageBox(hWnd, _T("Error reading location key"), _T("TrackIR Client"), MB_OK);
		logToWix(fmt::format("Registry Error: Error reading location key"));
		return false;
	}
	else
	{
		::RegCloseKey(pKey);
		_tcscpy_s(szPath, pszValue);
		free(pszValue);
	}
}

/*

TODO:
create class cpp/h for display
maybe create additional headers for NP stuff
use include gaurds? #ifdef
delete debug .cpp files
make toml more robust to non floats
move toml load to seperate file
make uniform standard for variables and functions for cases
variables prefixed and Camel

*/
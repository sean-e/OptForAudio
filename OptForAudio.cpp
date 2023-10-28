/*
 * OptForAudio
 * License: UNLICENSE
 *
 * I (Sean Echevarria) wrote this to do repetitive system housekeeping necessary
 * to run Amplitube on a Dell laptop without audio dropouts.
 * 
 * It:
 *	 Runs elevated
 *   Disables display power down
 *   Disables screensaver
 *   Disables Wi-Fi interface
 *   Disables CPU throttling
 *   Launches a list of programs (not elevated by default)
 *   Waits for all of the programs to exit
 *   Restores all changes it made
 * 
 * Let me know if you modify, extend or use this.
 * My website: http://www.creepingfog.com/sean/
 * Contact Sean: "fester" at the domain of my website
 */


#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi")
#include <powersetting.h>
#pragma comment(lib, "PowrProf")


// one day, these might be controlled by command line params
bool gDisplayRequired = true;
bool gDisableScreensaver = true;
bool gDisableCPUThrottle = true;
bool gDisableWifi = true;
bool gRunUtilApps = false;


void ReportStatus(std::wstring msg);
HANDLE LaunchProgram(std::wstring cmdline, bool launchUsingShellToken = true);

int main()
{
	// Change system settings before launching the program(s)
	//
	ReportStatus(L"Optimizing system for realtime audio...");

	const EXECUTION_STATE prevExecState = gDisplayRequired ? ::SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS) : 0;

	UINT prevScreenSaverTimeout = 0;
	if (gDisableScreensaver)
	{
		::SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &prevScreenSaverTimeout, 0);
		::SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, FALSE, nullptr, 0);
	}

	if (gDisableCPUThrottle)
	{
		// https ://learn.microsoft.com/en-us/windows/win32/api/powerbase/nf-powerbase-callntpowerinformation
		// Changes made to the current system power policy using CallNtPowerInformation are immediate, but they are not persistent;
		// https://learn.microsoft.com/en-us/windows/win32/power/processor-performance-control-policy-constants
		// https://stackoverflow.com/questions/9721218/trying-to-disable-processor-idle-states-c-states-on-windows-pc

		GUID *scheme;
		::PowerGetActiveScheme(nullptr, &scheme);
		::PowerWriteACValueIndex(nullptr, scheme, &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_IDLE_DISABLE, 1);
		::PowerSetActiveScheme(nullptr, scheme);
	}

	if (gDisableWifi)
	{
		// run without waiting for command to complete since it is slow to run
		::system(R"(start /min %windir%\System32\netsh.exe interface set interface "Wi-Fi" disabled)");
	}


	// launch program(s), unelevated
	//
	std::vector<std::wstring> programs;

	if (gRunUtilApps)
	{
		programs.emplace_back(LR"(C:\Program Files\RightMark\ppmpanel\ppmpanel.exe)");
		programs.emplace_back(LR"(C:\Program Files\LatencyMon\LatMon.exe)");
	}

	// could add command line support to launch programs rather than hardcoding this program list 
	// (cmd line would be semi-colon delimited list?)
	programs.emplace_back(LR"(C:\Program Files\IK Multimedia\AmpliTube 5\AmpliTube 5.exe)");

	ReportStatus(L"Starting programs...");
	std::vector<HANDLE> processes;
	for (const auto& it : programs)
	{
		if (::PathFileExists(it.c_str()))
		{
			HANDLE tmp = LaunchProgram(it);
			if (!tmp)
			{
				ReportStatus(L"Failed to launch: " + it);
			}
			else
			{
				processes.push_back(tmp);
			}
		}
		else
			ReportStatus(L"Program not found: " + it);
	}

	// Wait for all launched programs to exit
	//
	if (!processes.empty())
	{
		::Sleep(1000);
		ReportStatus(L"Waiting for programs to exit...");
		::WaitForMultipleObjects((DWORD)processes.size(), &processes[0], TRUE, INFINITE);

		for (auto it : processes)
			::CloseHandle(it);

		processes.clear();
	}

	// restore all changed system states
	//
	ReportStatus(L"Restoring system settings...");

	if (gDisableWifi)
	{
		// run without waiting for command to complete since it is slow to run
		::system(R"(start /min %windir%\System32\netsh.exe interface set interface "Wi-Fi" enabled)");
	}

	if (gDisableCPUThrottle)
	{
		GUID *scheme;
		::PowerGetActiveScheme(nullptr, &scheme);
		::PowerWriteACValueIndex(nullptr, scheme, &GUID_PROCESSOR_SETTINGS_SUBGROUP, &GUID_PROCESSOR_IDLE_DISABLE, 0);
		::PowerSetActiveScheme(nullptr, scheme);
	}

	if (gDisableScreensaver)
		::SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, prevScreenSaverTimeout, nullptr, 0);

	if (gDisplayRequired)
		::SetThreadExecutionState(prevExecState);

	ReportStatus(L"Completed, pausing before exit...");
	::Sleep(5000);
}

HANDLE
LaunchProgram(std::wstring inStr, bool launchUsingShellToken /*= true*/)
{
	constexpr int kBufLen = 2048;
	const std::unique_ptr<wchar_t[]> tmpVec(new wchar_t[kBufLen]);
	wchar_t* tmp = &tmpVec[0];
	::ExpandEnvironmentStringsW(inStr.c_str(), tmp, kBufLen);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);

	if (launchUsingShellToken)
	{
		// unelevated launch
		// assume we are running with elevation, so we need to launch the programs unelevated
		// https://stackoverflow.com/questions/1173630/how-do-you-de-elevate-privileges-for-a-child-process
		// https://devblogs.microsoft.com/oldnewthing/20190425-00/?p=102443https://devblogs.microsoft.com/oldnewthing/20190425-00/?p=102443
		// https://stackoverflow.com/questions/17765568/how-to-start-a-process-unelevated/40687129#40687129
		
		HWND hShellWnd = ::GetShellWindow();
		if (!hShellWnd)
		{
			ReportStatus(L"GetShellWindow failed");
			return nullptr;
		}

		DWORD shellProcessId = 0;
		::GetWindowThreadProcessId(hShellWnd, &shellProcessId);
		if (!shellProcessId)
		{
			ReportStatus(L"GetWindowThreadProcessId failed");
			return nullptr;
		}

		HANDLE hShellProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, shellProcessId);
		if (!hShellProcess)
		{
			ReportStatus(L"OpenProcess failed");
			return nullptr;
		}

		HANDLE hShellToken = nullptr;
		if (!::OpenProcessToken(hShellProcess, TOKEN_DUPLICATE, &hShellToken))
		{
			ReportStatus(L"OpenProcessToken failed");
			::CloseHandle(hShellProcess);
			return nullptr;
		}

		::CloseHandle(hShellProcess);
		hShellProcess = nullptr;

		constexpr DWORD tokenAccess = TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID;
		HANDLE hToken = nullptr;
		if (!::DuplicateTokenEx(hShellToken, tokenAccess, nullptr, SecurityImpersonation, TokenPrimary, &hToken))
		{
			ReportStatus(L"DuplicateTokenEx failed");
			return nullptr;
		}

		// https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithtokenw
		if (!::CreateProcessWithTokenW(hToken, 0, nullptr, tmp, 
			CREATE_NEW_PROCESS_GROUP | HIGH_PRIORITY_CLASS, nullptr, nullptr, &si, &pi))
		{
			(void)GetLastError();
			// retry with normal CreateProcess -- maybe elevated assumption didn't apply (as when debugging)
		}

		::CloseHandle(hToken);
	}

	if (!launchUsingShellToken || !pi.hProcess)
	{
		// if launchUsingShellToken was true, then CreateProcessWithTokenW must have failed; retry this way
		if (!::CreateProcess(tmp, nullptr, nullptr, nullptr, FALSE,
			CREATE_NEW_PROCESS_GROUP | HIGH_PRIORITY_CLASS, nullptr, nullptr, &si, &pi))
		{
			(void)GetLastError();
			ReportStatus(L"CreateProcess failed");
			return nullptr;
		}
	}

	::CloseHandle(pi.hThread);
	return pi.hProcess;
}

void
ReportStatus(std::wstring msg)
{
	msg = L"[OptForAudio] " + msg + L"\n";
	::wprintf(msg.c_str());
	::OutputDebugStringW(msg.c_str());
}

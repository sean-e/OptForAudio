/*
 * OptForAudio
 * License: UNLICENSE
 *
 * I (Sean Echevarria) wrote this to do repetitive system housekeeping necessary 
 * to run Amplitube on an old Dell laptop with significantly reduced audio 
 * dropouts and buffer underruns.
 * 
 * It:
 *   Runs elevated
 *   Disables display power down
 *   Disables screensaver
 *   Disables Wi-Fi interface
 *   Disables CPU throttling
 *   Disables Microsoft ACPI-Compliant Control Method Battery (device ID ACPI\PNP0C0A\1)
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


bool gDisplayRequired = true;
bool gDisableScreensaver = true;
bool gDisableCPUThrottle = true;
bool gDisableWifi = true;
bool gDisableCoreAffinity = true;
bool gDisableAcpiDevice = true;
bool gRunUtilApps = false;

std::vector<std::wstring> gPrograms;

void ReportStatus(std::wstring msg);
HANDLE LaunchProgram(std::wstring cmdline, bool launchUsingShellToken = true);

int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
	// command line parameters to override default behavior
	for (int i = 1; i < argc; i++)
	{
		if (_wcsicmp(argv[i], L"-xDisplay") == 0)
			gDisplayRequired = false;
		else if (_wcsicmp(argv[i], L"-xScreensaver") == 0)
			gDisableScreensaver = false;
		else if (_wcsicmp(argv[i], L"-xCpuThrottle") == 0)
			gDisableCPUThrottle = false;
		else if (_wcsicmp(argv[i], L"-xWifi") == 0)
			gDisableWifi = false;
		else if (_wcsicmp(argv[i], L"-xCoreAffinity") == 0)
			gDisableCoreAffinity = false;
		else if (_wcsicmp(argv[i], L"-xAcpi") == 0)
			gDisableAcpiDevice = false;
		else if (_wcsicmp(argv[i], L"-runUtils") == 0)
			gRunUtilApps = true;
		else if (argv[i][0] == L'-')
		{
			ReportStatus(L"unrecognized command line parameter: ");
			ReportStatus(argv[i]);
			ReportStatus(L"Exiting...");
			::Sleep(5000);
			return -1;
		}
		else if (_wcsicmp(argv[i], L"?") == 0)
		{
			// todo: display help
		}
		else
		{
			// todo: check for semi-colon delimited program list; parse and add to gPrograms if is a file
		}
	}


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
		::_wsystem(LR"(start /min %windir%\System32\netsh.exe interface set interface "Wi-Fi" disabled)");
	}

	if (gDisableAcpiDevice)
	{
		::_wsystem(LR"(%windir%\System32\pnputil.exe /disable-device ACPI\PNP0C0A\1)");
	}


	std::vector<HANDLE> processes;
	// launch program(s), unelevated
	//
	if (gRunUtilApps)
	{
		std::vector<std::wstring> utilPrograms;
		utilPrograms.emplace_back(LR"(C:\Program Files\RightMark\ppmpanel\ppmpanel.exe)");
		utilPrograms.emplace_back(LR"(C:\Program Files\LatencyMon\LatMon.exe)");

		ReportStatus(L"Starting util programs...");
		for (const auto& it : utilPrograms)
		{
			if (::PathFileExists(it.c_str()))
			{
				HANDLE tmp = LaunchProgram(it);
				if (tmp)
					processes.push_back(tmp);
				else
					ReportStatus(L"Failed to launch: " + it);
			}
			else
				ReportStatus(L"Program not found: " + it);
		}
	}

	gPrograms.emplace_back(LR"(C:\Program Files\IK Multimedia\AmpliTube 5\AmpliTube 5.exe)");

	ReportStatus(L"Starting programs...");
	for (const auto& it : gPrograms)
	{
		if (::PathFileExists(it.c_str()))
		{
			HANDLE tmp = LaunchProgram(it);
			if (tmp)
			{
				if (gDisableCoreAffinity)
				{
					DWORD_PTR procAffinityMask = 0, systemAffinityMask = 0;
					::GetProcessAffinityMask(tmp, &procAffinityMask, &systemAffinityMask);
					// disable core 0 and 1:
					procAffinityMask &= 0xfffffffffffffffc;
					if (!::SetProcessAffinityMask(tmp, procAffinityMask))
						ReportStatus(L"process affinity fail");
				}

				processes.push_back(tmp);
			}
			else
				ReportStatus(L"Failed to launch: " + it);
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
		::_wsystem(LR"(start /min %windir%\System32\netsh.exe interface set interface "Wi-Fi" enabled)");
	}

	if (gDisableAcpiDevice)
	{
		::_wsystem(LR"(%windir%\System32\pnputil.exe /enable-device ACPI\PNP0C0A\1)");
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

	return 0;
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

// StubExecutable.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "StubExecutable.h"

#include "semver200.h"

using namespace std;

wchar_t* FindRootAppDir() 
{
	wchar_t* ourDirectory = new wchar_t[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), ourDirectory, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(ourDirectory, L'\\');
	if (!lastSlash) {
		delete[] ourDirectory;
		return NULL;
	}

	// Null-terminate the string at the slash so now it's a directory
	*lastSlash = 0x0;
	return ourDirectory;
}

wchar_t* FindOwnExecutableName() 
{
	wchar_t* ourDirectory = new wchar_t[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), ourDirectory, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(ourDirectory, L'\\');
	if (!lastSlash) {
		delete[] ourDirectory;
		return NULL;
	}

	wchar_t* ret = _wcsdup(lastSlash + 1);
	delete[] ourDirectory;
	return ret;
}

// Helper to check if a string is a valid number (doesn't contain non-numeric characters)
bool isNumericString(const std::string& str) {
    for (char c : str) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return !str.empty();
}

std::wstring FindLatestAppDir() 
{
	std::wstring ourDir;
	ourDir.assign(FindRootAppDir());

	ourDir += L"\\app-*";

	WIN32_FIND_DATA fileInfo = { 0 };
	HANDLE hFile = FindFirstFile(ourDir.c_str(), &fileInfo);
	if (hFile == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	version::Semver200_version acc("0.0.0");
	std::wstring acc_s;
	
	// Track the highest build number separately for versions with same major.minor.patch
	int highestBuildNumber = -1;

	do {
		std::wstring appVer = fileInfo.cFileName;
		appVer = appVer.substr(4);   // Skip 'app-'
		if (!(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			continue;
		}

		std::string s(appVer.begin(), appVer.end());

		version::Semver200_version thisVer(s);

		// Check if the versions are equal in terms of major.minor.patch
		bool sameVersion = (thisVer.major() == acc.major() && 
		                   thisVer.minor() == acc.minor() && 
		                   thisVer.patch() == acc.patch());
		
		// If they're the same version, compare build metadata
		if (sameVersion) {
		    // Extract build number if it exists
		    int buildNumber = -1;
		    if (thisVer.build().size() > 0) {
		        std::string buildStr = thisVer.build()[0];
		        if (isNumericString(buildStr)) {
		            buildNumber = std::stoi(buildStr);
		        }
		    }
		    
		    // If this build number is higher, use it
		    if (buildNumber > highestBuildNumber) {
		        highestBuildNumber = buildNumber;
		        acc = thisVer;
		        acc_s = appVer;
		    }
		}
		// Otherwise use normal semver comparison
		else if (thisVer > acc) {
			acc = thisVer;
			acc_s = appVer;
			
			// Reset highest build number for the new version
			highestBuildNumber = -1;
			if (thisVer.build().size() > 0) {
			    std::string buildStr = thisVer.build()[0];
			    if (isNumericString(buildStr)) {
			        highestBuildNumber = std::stoi(buildStr);
			    }
			}
		}
	} while (FindNextFile(hFile, &fileInfo));

	if (acc == version::Semver200_version("0.0.0")) {
		return NULL;
	}

	ourDir.assign(FindRootAppDir());
	std::wstringstream ret;
	ret << ourDir << L"\\app-" << acc_s;

	FindClose(hFile);
	return ret.str();
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	std::wstring appName;
	appName.assign(FindOwnExecutableName());

	std::wstring workingDir(FindLatestAppDir());
	std::wstring fullPath(workingDir + L"\\" + appName);

	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = nCmdShow;

	std::wstring cmdLine(L"\"");
	cmdLine += fullPath;
	cmdLine += L"\" ";
	cmdLine += lpCmdLine;

	wchar_t* lpCommandLine = _wcsdup(cmdLine.c_str());
	wchar_t* lpCurrentDirectory = _wcsdup(workingDir.c_str());
	if (!CreateProcess(NULL, lpCommandLine, NULL, NULL, true, 0, NULL, lpCurrentDirectory, &si, &pi)) {
		return -1;
	}

	AllowSetForegroundWindow(pi.dwProcessId);
	WaitForInputIdle(pi.hProcess, 5 * 1000);
	return 0;
}

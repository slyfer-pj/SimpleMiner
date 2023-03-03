#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <windows.h>			// #include this (massive, platform-specific) header in very few places
#include <math.h>
#include <cassert>
#include <crtdbg.h>
#include "App.hpp"
#include <objbase.h>
#include "Engine/Core/EngineCommon.hpp"

extern App* g_theApp;

int WINAPI WinMain(HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int)
{
	CoInitialize(NULL);
	UNUSED(applicationInstanceHandle)
	UNUSED(commandLineString);

	g_theApp = new App();
	g_theApp->Startup();
	g_theApp->Run();
	g_theApp->Shutdown();
	delete g_theApp;
	g_theApp = nullptr;

	CoUninitialize();

	return 0;
}



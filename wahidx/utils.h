#pragma once

#include <windows.h>

namespace wahidx
{
	namespace utils
	{

		static BOOL RegisterNotification(HWND hwnd);

		static void UnregisterNotification(HWND hwnd);

	};
};
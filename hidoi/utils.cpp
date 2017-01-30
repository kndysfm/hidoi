#include "utils.h"
EXTERN_C
{
#include <hidsdi.h>
#include <Dbt.h>
}
#include <map>

using namespace hidoi::utils;


static std::map<HANDLE, HDEVNOTIFY> registeredHandles;

BOOL RegisterNotification(HWND hwnd)
{
	if (registeredHandles.count(hwnd) != 0) return FALSE;

	DEV_BROADCAST_DEVICEINTERFACE    broadcastInterface;

	broadcastInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	broadcastInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	::HidD_GetHidGuid(&broadcastInterface.dbcc_classguid);

	HANDLE handle = ::RegisterDeviceNotification(hwnd,
		&broadcastInterface,
		DEVICE_NOTIFY_WINDOW_HANDLE
	);
	if (handle != INVALID_HANDLE_VALUE)
	{
		registeredHandles[hwnd] = handle;
		return TRUE;
	}
	else return FALSE;
}

void UnregisterNotification(HWND hwnd)
{
	if (registeredHandles.count(hwnd) != 0)
	{
		::UnregisterDeviceNotification(registeredHandles[hwnd]);
		registeredHandles.erase(hwnd);
	}
}



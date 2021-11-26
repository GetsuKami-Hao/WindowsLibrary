#include "stdafx.h"
#include "DisplayEquipment.h"
#include<windows.h>

void Device::Monitor::CheckAndRestoreDisconnectedMonitor()
{
	DWORD           dispNum = 0;
	DISPLAY_DEVICE  displayDevice;
	DEVMODE   defaultMode;
	LONG	result;

	ZeroMemory(&displayDevice, sizeof(displayDevice));
	displayDevice.cb = sizeof(displayDevice);

	while (EnumDisplayDevices(NULL, dispNum, &displayDevice, 0))
	{
		ZeroMemory(&defaultMode, sizeof(DEVMODE));
		defaultMode.dmSize = sizeof(DEVMODE);

		dispNum++;
		if (!(displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
		{
			DEVMODE    devMode;
			ZeroMemory(&devMode, sizeof(devMode));
			devMode.dmSize = sizeof(devMode);
			devMode.dmFields = DM_POSITION/* | DM_PELSWIDTH | DM_PELSHEIGHT*/;
			devMode.dmPosition.x = 1920;
			devMode.dmPosition.y = 1080 * 2;

			//The setting twice takes effect.
			result = ChangeDisplaySettingsEx(displayDevice.DeviceName, &devMode, NULL, CDS_UPDATEREGISTRY | CDS_NORESET, NULL);
			ChangeDisplaySettingsEx(NULL, NULL, NULL, NULL, NULL);
		}
	}
}

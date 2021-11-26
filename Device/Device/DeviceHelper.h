#pragma once

#include<Windows.h>

namespace Device
{
	namespace Driver
	{
		void* OpenDevice(GUID *pGuid, bool bOverLapped = false);

		unsigned int RescanDeviceList();
		unsigned int EnumerateDevices(GUID *pGuid, wchar_t *pDevicePath, int devicePathLength);
		unsigned int DeviceStateChange(LPCTSTR lpTexts,bool isEnable);
	}
}
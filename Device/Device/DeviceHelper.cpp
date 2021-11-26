#include "stdafx.h"
#include "DeviceHelper.h"
#include <setupapi.h>
#include <cfgmgr32.h>

#pragma comment (lib, "SetupAPI.lib")

void * Device::Driver::OpenDevice(GUID * pGuid, bool bOverLapped)
{
	wchar_t DevicePath[256]; // 设备的路径名, 暂定最大长度为256
	void*  hDev = INVALID_HANDLE_VALUE;

	if (ERROR_SUCCESS != EnumerateDevices(pGuid, DevicePath, sizeof(DevicePath) / sizeof(DevicePath[0])))
	{
		// 没能找到设备
		return INVALID_HANDLE_VALUE;
	}

	// 找到设备
	// 不允许重叠操作
	if (false == bOverLapped)
	{
		hDev = CreateFile(
			DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			0
		);
	}
	// 允许重叠操作
	else
	{
		// 创建文件（打开文件）并返回文件句柄
		hDev = CreateFile(
			DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,									// 文件必须已经存在
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,	// 文件属性为默认属性，并且允许对文件进行重叠操作
			0
		);
	}

	if (hDev == INVALID_HANDLE_VALUE)
	{
		return INVALID_HANDLE_VALUE;
	}

	return hDev;
}

unsigned int Device::Driver::RescanDeviceList()
{
	DEVINST     devInst;
	CONFIGRET   status;

	status = CM_Locate_DevNode(&devInst, NULL, CM_LOCATE_DEVNODE_NORMAL);

	if (status != CR_SUCCESS) {
		return status;
	}

	status = CM_Reenumerate_DevNode(devInst, 0);

	return status;
}

unsigned int Device::Driver::EnumerateDevices(GUID * pGuid, wchar_t * pDevicePath, int devicePathLength)
{
	unsigned int                       lastError;
	HDEVINFO                           hDeviceInfo;
	unsigned int                       bufferSize;
	SP_DEVICE_INTERFACE_DATA           interfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA   deviceDetail;
	int i = 0;
	// 判断参数有效性
	if (nullptr == pGuid || nullptr == pDevicePath)
	{
		return ERROR_INVALID_PARAMETER;
	}

	// Find all devices that have our interface
	hDeviceInfo = SetupDiGetClassDevs(
		pGuid,
		nullptr,
		nullptr,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
	);
	if (hDeviceInfo == INVALID_HANDLE_VALUE)
	{
		lastError = GetLastError();
		return lastError;
	}

	// Setup the interface data struct
	interfaceData.cbSize = sizeof(interfaceData);

	for ( i = 0;
		SetupDiEnumDeviceInterfaces(
			hDeviceInfo,
			nullptr,
			pGuid,
			i,
			&interfaceData);
		++i)
	{
		// Found our device instance
		if (!SetupDiGetDeviceInterfaceDetail(
			hDeviceInfo,
			&interfaceData,
			nullptr,
			0,
			(PDWORD)&bufferSize,
			nullptr))
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			{
				continue;
			}
		}

		// Allocate a big enough buffer to get detail data
		deviceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(bufferSize);
		if (deviceDetail == nullptr)
		{
			continue;
		}

		// Setup the device interface struct
		deviceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		// Try again to get the device interface detail info
		if (!SetupDiGetDeviceInterfaceDetail(
			hDeviceInfo,
			&interfaceData,
			deviceDetail,
			bufferSize,
			nullptr,
			nullptr))
		{
			free(deviceDetail);
			continue;
		}

		// copy device instance info
		wcscpy_s(pDevicePath, devicePathLength, deviceDetail->DevicePath);

		// Free our allocated buffer
		free(deviceDetail);
	}

	// 销毁一个设备信息集合，并释放所有关联的内容
	SetupDiDestroyDeviceInfoList(hDeviceInfo);

	if (i == 0)
	{
		// No devices found
		return ERROR_OPEN_FAILED;
	}

	return ERROR_SUCCESS;
}

unsigned int Device::Driver::DeviceStateChange(LPCTSTR lpTexts, bool isEnable)
{
	SP_DEVINFO_DATA m_DeviceInfoData;
	HDEVINFO  hDeviceInfo = SetupDiGetClassDevs(nullptr, 0, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);

	m_DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	DWORD nDevice = 0;

	bool isFinded = false;
	// search device
	for (int i = 0; SetupDiEnumDeviceInfo(hDeviceInfo, i, &m_DeviceInfoData); i++)
	{
		DWORD DataT;
		LPTSTR buffer = NULL;
		DWORD buffersize = 0;

		while (!SetupDiGetDeviceRegistryProperty
		(
			hDeviceInfo,
			&m_DeviceInfoData,
			SPDRP_DEVICEDESC,
			&DataT,
			(PBYTE)buffer,
			buffersize,
			&buffersize
		))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				// Change the buffer size.
				if (buffer)
				{
					LocalFree(buffer);
				}
				buffer = (LPTSTR)LocalAlloc(LPTR, buffersize);
			}
			else
			{
				break;
			}
		}

		if ((buffer != NULL) && wcscmp(lpTexts, buffer) == 0)
		{
			// finded it.

			if (buffer)
			{
				LocalFree(buffer);
			}
			isFinded = true;
			break;
		}

		if (buffer)
		{
			LocalFree(buffer);
		}
	}

	if(!isFinded)
		return ERROR_OPEN_FAILED;

	SP_PROPCHANGE_PARAMS propChange = { sizeof(SP_CLASSINSTALL_HEADER) };
	propChange.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
	propChange.Scope = DICS_FLAG_GLOBAL;
	propChange.StateChange = isEnable ? DICS_ENABLE : DICS_DISABLE;   // Enable Or Disable Flag

	bool rlt;
	if (m_DeviceInfoData.DevInst != NULL)
	{
		rlt = SetupDiSetClassInstallParams
		(
			hDeviceInfo,
			&m_DeviceInfoData,
			(SP_CLASSINSTALL_HEADER *)&propChange,
			sizeof(propChange)
		);
	}

	if (rlt)
	{
		rlt = SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDeviceInfo, &m_DeviceInfoData);
	}
	else
	{
		return GetLastError();
	}

	// destroy
	SetupDiDestroyDeviceInfoList(hDeviceInfo);

	return 0;
}

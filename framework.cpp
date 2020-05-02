#include "pch.h"
#include "framework.h"
#include "DpiHelper.h"
#include <memory>
#include <cassert>
#include <string>
#include <map>

bool DpiHelper::GetPathsAndModes(std::vector<DISPLAYCONFIG_PATH_INFO>& pathsV, std::vector<DISPLAYCONFIG_MODE_INFO>& modesV, int flags)
{
	UINT32 numPaths = 0, numModes = 0;
	auto status = GetDisplayConfigBufferSizes(flags, &numPaths, &numModes);
	if (ERROR_SUCCESS != status)
	{
		return false;
	}

	std::unique_ptr<DISPLAYCONFIG_PATH_INFO[]> paths(new DISPLAYCONFIG_PATH_INFO[numPaths]);
	std::unique_ptr<DISPLAYCONFIG_MODE_INFO[]> modes(new DISPLAYCONFIG_MODE_INFO[numModes]);
	status = QueryDisplayConfig(flags, &numPaths, paths.get(), &numModes, modes.get(), nullptr);
	if (ERROR_SUCCESS != status)
	{
		return false;
	}

	for (unsigned int i = 0; i < numPaths; i++)
	{
		pathsV.push_back(paths[i]);
	}

	for (unsigned int i = 0; i < numModes; i++)
	{
		modesV.push_back(modes[i]);
	}

	return true;
}


DpiHelper::DpiHelper()
{
}


DpiHelper::~DpiHelper()
{
}


DpiHelper::DPIScalingInfo DpiHelper::GetDPIScalingInfo(LUID adapterID, UINT32 sourceID)
{
	DPIScalingInfo dpiInfo = {};

	DpiHelper::DISPLAYCONFIG_SOURCE_DPI_SCALE_GET requestPacket = {};
	requestPacket.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)DpiHelper::DISPLAYCONFIG_DEVICE_INFO_TYPE_CUSTOM::DISPLAYCONFIG_DEVICE_INFO_GET_DPI_SCALE;
	requestPacket.header.size = sizeof(requestPacket);
	assert(0x20 == sizeof(requestPacket));//if this fails => OS has changed somthing, and our reverse enginnering knowledge about the API is outdated
	requestPacket.header.adapterId = adapterID;
	requestPacket.header.id = sourceID;

	auto res = ::DisplayConfigGetDeviceInfo(&requestPacket.header);
	if (ERROR_SUCCESS == res)
	{//success
		if (requestPacket.curScaleRel < requestPacket.minScaleRel)
		{
			requestPacket.curScaleRel = requestPacket.minScaleRel;
		}
		else if (requestPacket.curScaleRel > requestPacket.maxScaleRel)
		{
			requestPacket.curScaleRel = requestPacket.maxScaleRel;
		}

		std::int32_t minAbs = abs((int)requestPacket.minScaleRel);
		if (DpiHelper::CountOf(DpiVals) >= (size_t)(minAbs + requestPacket.maxScaleRel + 1))
		{//all ok
			dpiInfo.current = DpiVals[minAbs + requestPacket.curScaleRel];
			dpiInfo.recommended = DpiVals[minAbs];
			dpiInfo.maximum = DpiVals[minAbs + requestPacket.maxScaleRel];
			dpiInfo.bInitDone = true;
		}
		else
		{
			//Error! Probably DpiVals array is outdated
			return dpiInfo;
		}
	}
	else
	{
		//DisplayConfigGetDeviceInfo() failed
		return dpiInfo;
	}

	return dpiInfo;
}

std::wstring GetTargetName(LUID adapterLUID, UINT32 sourceId)
{
	std::vector<DISPLAYCONFIG_PATH_INFO> pathsV;
	std::vector<DISPLAYCONFIG_MODE_INFO> modesV;
	int flags = QDC_ONLY_ACTIVE_PATHS;
	if (false == DpiHelper::GetPathsAndModes(pathsV, modesV, flags))
	{
		wprintf(L"DpiHelper::GetPathsAndModes() failed\r\n");
	}

	for (const auto& path : pathsV)
	{

		if (adapterLUID.LowPart == path.targetInfo.adapterId.LowPart
			&& adapterLUID.HighPart == path.targetInfo.adapterId.HighPart
			&& sourceId == path.sourceInfo.id)
		{
			DISPLAYCONFIG_TARGET_DEVICE_NAME deviceName;
			deviceName.header.size = sizeof(deviceName);
			deviceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
			deviceName.header.adapterId = adapterLUID;
			deviceName.header.id = path.targetInfo.id;
			if (ERROR_SUCCESS != DisplayConfigGetDeviceInfo(&deviceName.header))
			{
				wprintf(L"DisplayConfigGetDeviceInfo() failed\r\n");
			}
			else
			{

				std::wstring nameString = deviceName.monitorFriendlyDeviceName;
				if (DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL == deviceName.outputTechnology)
				{
					nameString += L"(internal display)";
				}
				return nameString;
			}
		}

	}
	return L"N/A";

}



void printOne(LUID adapterLUID, UINT32 sourceID) {
	wprintf(L"GPU=%ld.%u,Desktop_Index_In_GPU=%d,Monitor=%ls\r\n"
		, adapterLUID.HighPart
		, adapterLUID.LowPart
		, sourceID
		, GetTargetName(adapterLUID, sourceID).data());
}



bool DpiHelper::SetDPIScaling(LUID adapterID, UINT32 sourceID, UINT32 dpiPercentToSet)
{

	wprintf(L"setting dpi scale to %d: ", dpiPercentToSet);
	printOne(adapterID, sourceID);
	DPIScalingInfo dPIScalingInfo = GetDPIScalingInfo(adapterID, sourceID);

	if (dpiPercentToSet == dPIScalingInfo.current)
	{
		return true;
	}

	if (dpiPercentToSet < dPIScalingInfo.mininum)
	{
		dpiPercentToSet = dPIScalingInfo.mininum;
	}
	else if (dpiPercentToSet > dPIScalingInfo.maximum)
	{
		dpiPercentToSet = dPIScalingInfo.maximum;
	}

	int idx1 = -1, idx2 = -1;

	int i = 0;
	for (const auto& val : DpiVals)
	{
		if (val == dpiPercentToSet)
		{
			idx1 = i;
		}

		if (val == dPIScalingInfo.recommended)
		{
			idx2 = i;
		}
		i++;
	}

	if ((idx1 == -1) || (idx2 == -1))
	{
		//Error cannot find dpi value
		return false;
	}

	int dpiRelativeVal = idx1 - idx2;

	DpiHelper::DISPLAYCONFIG_SOURCE_DPI_SCALE_SET setPacket = {};
	setPacket.header.adapterId = adapterID;
	setPacket.header.id = sourceID;
	setPacket.header.size = sizeof(setPacket);
	assert(0x18 == sizeof(setPacket));//if this fails => OS has changed somthing, and our reverse enginnering knowledge about the API is outdated
	setPacket.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)DpiHelper::DISPLAYCONFIG_DEVICE_INFO_TYPE_CUSTOM::DISPLAYCONFIG_DEVICE_INFO_SET_DPI_SCALE;
	setPacket.scaleRel = (UINT32)dpiRelativeVal;

	auto res = ::DisplayConfigSetDeviceInfo(&setPacket.header);
	if (ERROR_SUCCESS == res)
	{
		return true;
	}
	else
	{
		return false;
	}
	return true;
}

#define MAX_ID  10
LUID GpuId[MAX_ID];
UINT32 DesktopIndexInGpu[MAX_ID];
UINT32 oldDPI[MAX_ID];

INT32 DpiHelper::GetIDHigh(INT32 index) {
	auto map = RefreshDpiInfo();
	return map[index].m_adapterId.HighPart;
}

INT32 DpiHelper::GetIDLow(INT32 index) {
	auto map = RefreshDpiInfo();
	return map[index].m_adapterId.LowPart;
}

INT32 DpiHelper::GetIDSource(INT32 index) {
	auto map = RefreshDpiInfo();
	return map[index].m_sourceID;
}

INT32 DpiHelper::GetIndexCount() {
	auto map = RefreshDpiInfo();
	return map.size();
}

std::map<int, DpiHelper::DisplayData> DpiHelper::RefreshDpiInfo()
{
	std::map<int, DpiHelper::DisplayData> m_displayDataCache = {};
	m_displayDataCache.clear();
	std::vector<DISPLAYCONFIG_PATH_INFO> pathsV;
	std::vector<DISPLAYCONFIG_MODE_INFO> modesV;
	int flags = QDC_ONLY_ACTIVE_PATHS;
	if (false == DpiHelper::GetPathsAndModes(pathsV, modesV, flags))
	{
		wprintf(L"DpiHelper::GetPathsAndModes() failed");
	}
	else
	{
		wprintf(L"DpiHelper::GetPathsAndModes() successful");
	}

	int iIndex = 0;
	for (const auto& path : pathsV)
	{
		//get display name
		auto adapterLUID = path.targetInfo.adapterId;
		auto targetID = path.targetInfo.id;
		auto sourceID = path.sourceInfo.id;

		DISPLAYCONFIG_TARGET_DEVICE_NAME deviceName;
		deviceName.header.size = sizeof(deviceName);
		deviceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
		deviceName.header.adapterId = adapterLUID;
		deviceName.header.id = targetID;
		if (ERROR_SUCCESS != DisplayConfigGetDeviceInfo(&deviceName.header))
		{
			wprintf(L"DisplayConfigGetDeviceInfo() failed");
		}
		else
		{

			std::wstring monitor_name = GetTargetName(adapterLUID, sourceID);
			wprintf(L"DisplayConfigGetDeviceInfo()from TargetName: %s", monitor_name);
			
			std::wstring nameString = std::to_wstring(iIndex) + std::wstring(L". ") + deviceName.monitorFriendlyDeviceName;
			if (DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL == deviceName.outputTechnology)
			{
				nameString += L"(internal display)";
			}
			//m_displayList.AddString(nameString.c_str());
			DisplayData dd = {};
			dd.m_adapterId = adapterLUID;
			dd.m_sourceID = sourceID;
			dd.m_targetID = targetID;

			m_displayDataCache[iIndex] = dd;
			iIndex++;
		}
	}

	wprintf(L"DpiHelper::RefreshDpiInfo() index size %d", iIndex);

	return m_displayDataCache;
}

//
//void PrintDpiInfo() {
//
//	std::vector<DISPLAYCONFIG_PATH_INFO> pathsV;
//	std::vector<DISPLAYCONFIG_MODE_INFO> modesV;
//	int flags = QDC_ONLY_ACTIVE_PATHS;
//	if (false == DpiHelper::GetPathsAndModes(pathsV, modesV, flags))
//	{
//		wprintf(L"DpiHelper::GetPathsAndModes() failed");
//	}
//
//	int i = 0;
//	for (const auto& path : pathsV)
//	{
//		//get display name
//		auto adapterLUID = path.targetInfo.adapterId;
//		auto sourceID = path.sourceInfo.id;
//		std::wstring monitor_name = GetTargetName(adapterLUID, sourceID);
//		printOne(adapterLUID, sourceID);
//
//		DpiHelper::DPIScalingInfo dpiInfo = DpiHelper::GetDPIScalingInfo(adapterLUID, sourceID);
//
//		GpuId[i] = adapterLUID;
//		DesktopIndexInGpu[i] = sourceID;
//		oldDPI[i] = dpiInfo.current;
//
//
//		wprintf(L"Available DPI:\r\n");
//		int curdpi = 0;
//		for (const auto& dpi : DpiVals)
//		{
//			if ((dpi >= dpiInfo.mininum) && (dpi <= dpiInfo.maximum))
//				wprintf(L"    %d\r\n", dpi);
//		}
//		wprintf(L"    current DPI: %d\r\n", dpiInfo.current);
//
//		i++;
//		if (i >= MAX_ID) {
//			wprintf(L"To many desktops\r\n");
//			break;
//		}
//	}
//
//}

void SetDPIScaling(INT32 adapterIDHigh, UINT32 adapterIDlow, UINT32 sourceID, UINT32 dpiPercentToSet) {
	LUID adapterId;
	adapterId.HighPart = adapterIDHigh;
	adapterId.LowPart = adapterIDlow;
	DpiHelper::SetDPIScaling(adapterId, sourceID, dpiPercentToSet);
}

void RestoreDPIScaling()
{
	wprintf(L"Now restore DPI settings...\r\n");
	for (int i = 0; i < MAX_ID; i++) {
		if (GpuId[i].LowPart == 0 && GpuId[i].HighPart == 0) break;
		DpiHelper::SetDPIScaling(GpuId[i], DesktopIndexInGpu[i], oldDPI[i]);
	}

}

INT32 GetIDHigh(INT32 index) {
	return DpiHelper::GetIDHigh(index);
}

INT32 GetIDLow(INT32 index) {
	return DpiHelper::GetIDLow(index);
}

INT32 GetIDSource(INT32 index) {
	return DpiHelper::GetIDSource(index);
}

INT32 GetIndexCount() {
	return DpiHelper::GetIndexCount();
}
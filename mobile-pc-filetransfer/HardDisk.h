#pragma once

#include <atlstr.h>
#include <WinIoCtl.h>
#include <vector>
#include <string>

#define DRVUNKNOWN		0
#define DRVFIXED		1
#define DRVREMOTE		2
#define DRVRAM			3
#define DRVCD			4
#define DRVREMOVE		5

#define IOCTL_STORAGE_QUERY_PROPERTY   CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef enum CP_STORAGE_BUS_TYPE {
	CP_BusTypeUnknown = 0x00,
	CP_BusTypeScsi,
	CP_BusTypeAtapi,
	CP_BusTypeAta,
	CP_BusType1394,
	CP_BusTypeSsa,
	CP_BusTypeFibre,
	CP_BusTypeUsb,
	CP_BusTypeRAID,
	CP_BusTypeMaxReserved = 0x7F
} CP_STORAGE_BUS_TYPE, *CP_PSTORAGE_BUS_TYPE;


std::vector<std::string> GetHardDiskList();

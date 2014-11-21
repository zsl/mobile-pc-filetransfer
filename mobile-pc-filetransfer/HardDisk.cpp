#include "HardDisk.h"

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <functional>
#include <vector>

using namespace std;

namespace {

BOOL GetDisksProperty(HANDLE hDevice, PSTORAGE_DEVICE_DESCRIPTOR pDevDesc);

char chFirstDriveFromMask (ULONG unitmask)
{
	char i;
	for (i = 0; i < 26; ++i)  
	{
		if (unitmask & 0x1) 
			break;
		unitmask = unitmask >> 1;
	}
	return (i + 'A');
}

void ReInitUSB_Disk_Letter(char * szMoveDiskName, int len)
{
	int k = 0;
	DWORD			MaxDriveSet, CurDriveSet;
	DWORD			drive, drivetype;
	CHAR			szBuf[300];
	HANDLE			hDevice;
	PSTORAGE_DEVICE_DESCRIPTOR pDevDesc;

	for(k=0; k<len; k++)
		szMoveDiskName[k] = '\0';	
	k = 0;		
	// Get available drives we can monitor
	MaxDriveSet = CurDriveSet = 0;

	MaxDriveSet = GetLogicalDrives();
	CurDriveSet = MaxDriveSet;
	char szDrvName[33];
	for ( drive = 0; drive < 32 && k<len-1; ++drive )  
	{
		if ( MaxDriveSet & (1 << drive) )  
		{
			DWORD temp = 1<<drive;
			sprintf_s( szDrvName, sizeof(szDrvName),  "%c:\\", 'A'+drive );
			switch ( GetDriveTypeA( szDrvName ) )  
			{
			case 0:					// The drive type cannot be determined.
			case 1:					// The root directory does not exist.
				drivetype = DRVUNKNOWN;
				break;
			case DRIVE_REMOVABLE:	// The drive can be removed from the drive.
				drivetype = DRVREMOVE;
				szMoveDiskName[k] = chFirstDriveFromMask(temp);
				k++;
				break;
			case DRIVE_CDROM:		// The drive is a CD-ROM drive.
				break;
			case DRIVE_FIXED:		// The disk cannot be removed from the drive.
				drivetype = DRVFIXED;
				sprintf_s(szBuf, sizeof(szBuf), "\\\\?\\%c:", 'A'+drive);
				szBuf[299] = '\0';
				hDevice = CreateFileA(szBuf, GENERIC_READ,
					FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);

				if (hDevice != INVALID_HANDLE_VALUE)
				{
					pDevDesc = (PSTORAGE_DEVICE_DESCRIPTOR)new BYTE[sizeof(STORAGE_DEVICE_DESCRIPTOR) + 512 - 1];
					pDevDesc->Size = sizeof(STORAGE_DEVICE_DESCRIPTOR) + 512 - 1;
					if(GetDisksProperty(hDevice, pDevDesc))
					{
						if(pDevDesc->BusType == CP_BusTypeUsb)
						{
							szMoveDiskName[k] = chFirstDriveFromMask(temp);
							k++;
						}
					}

					delete pDevDesc;
					pDevDesc = NULL;
					CloseHandle(hDevice);
				}

				break;
			case DRIVE_REMOTE:		// The drive is a remote (network) drive.
				drivetype = DRVREMOTE;
				szMoveDiskName[k] = chFirstDriveFromMask(temp);
				k++;
				break;
			case DRIVE_RAMDISK:		// The drive is a RAM disk.
				drivetype = DRVRAM;
				break;
			}
		}
	}
}

BOOL GetDisksProperty(HANDLE hDevice, PSTORAGE_DEVICE_DESCRIPTOR pDevDesc)
{
	STORAGE_PROPERTY_QUERY	Query;	// input param for query
	DWORD dwOutBytes;				// IOCTL output length
	BOOL bResult;					// IOCTL return val

	// specify the query type
	Query.PropertyId = StorageDeviceProperty;
	Query.QueryType = PropertyStandardQuery;

	// Query using IOCTL_STORAGE_QUERY_PROPERTY 
	bResult = ::DeviceIoControl(hDevice,			// device handle
		IOCTL_STORAGE_QUERY_PROPERTY,			// info of device property
		&Query, sizeof(STORAGE_PROPERTY_QUERY),	// input data buffer
		pDevDesc, pDevDesc->Size,				// output data buffer
		&dwOutBytes,							// out's length
		(LPOVERLAPPED)NULL);					

	return bResult;
}

}

std::vector<std::string> GetHardDiskList()
{
	vector<string> hardDiskList;

	////得到所有的非本地硬盘盘符
	char szRDNamesBuf[27];
	ReInitUSB_Disk_Letter(szRDNamesBuf,27);
	string strRemoveableDriverNames = szRDNamesBuf;

	const DWORD alldriveLen = GetLogicalDriveStringsA(0, nullptr);  //取得表示当前所有磁盘设备字符串（盘符）的字符数组长度
	std::vector<char> tmpData(alldriveLen);         //保存当前所有磁盘设备字符串的字符数组
    char* driverstr = tmpData.data();
	GetLogicalDriveStringsA(alldriveLen, driverstr);    //获得表示当前所有磁盘设备字符串的字符数组

    char* driver = driverstr;
	while (*driver)		//diverstr的结构为每4个字节表示一个磁盘，比如“c:\nulld:\null...”
	{                                               //所以根据该结构获得我们需要的信息
		if(GetDriveTypeA(driver) == DRIVE_FIXED)	//如果该磁盘为磁盘则进行处理
		{
			if( string::npos == strRemoveableDriverNames.find(driver[0]))
			{
				hardDiskList.push_back(boost::to_lower_copy(string(driver)));
			}			
		}

        driver += strlen(driver) + 1;
	}

	std::sort(hardDiskList.begin(), hardDiskList.end(), std::less<string>());

	return hardDiskList;
}	


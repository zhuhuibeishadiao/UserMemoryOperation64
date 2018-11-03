#include <Windows.h>
#include <stdio.h>
#include <Psapi.h>  
#include <TlHelp32.h>
#include <iostream>
#pragma comment (lib,"Psapi.lib")  
#include "../UserMemoryOperation64/ExportHandler.h"


DWORD GetProcessidFromName(WCHAR* name)
{
    PROCESSENTRY32 pe;

    DWORD id = 0;

    HANDLE hSnapshot = (CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);

    pe.dwSize = sizeof(PROCESSENTRY32);

    if (!(Process32First)(hSnapshot, &pe))

        return 0;

    while (1)
    {
        pe.dwSize = sizeof(PROCESSENTRY32);

        if ((Process32Next)(hSnapshot, &pe) == FALSE)
            break;
        if ((wcscmp)(pe.szExeFile, name) == 0)
        {
            id = pe.th32ProcessID;
            break;
        }

    }

    (CloseHandle)(hSnapshot);

    return id;
}

void main()
{
    LONG status = Initialization();

    if (status)
    {
        printf("status:%d\n", status);
        system("pause");
        return;
    }

    ULONG64 pid = GetProcessidFromName(L"explorer.exe");

    if (pid == 0)
    {
        printf("not find target pid.\n");
        system("pause");
        return;
    }

    ULONG64 GameBase = GetTargetBase(pid);

    printf("base: 0x%p\n", GameBase);

    ULONG64 data = 0;
    ReadByte((LPVOID)GameBase, &data, sizeof(ULONG64));

    printf("read data: 0x%llx\n", data);

    DWORD tick = GetTickCount();

    for (size_t i = 0; i < 500000; i++)
    {
        ReadByte((LPVOID)GameBase, &data, sizeof(ULONG64));
    }
    tick = GetTickCount() - tick;

    printf("ms:%d\n", tick);

    system("pause");
}

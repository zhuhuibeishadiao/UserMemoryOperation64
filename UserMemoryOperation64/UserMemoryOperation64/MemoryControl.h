#pragma once
#include "pch.h"
#include "KernelHelper.h"

#define OBJ_KERNEL_HANDLE                   0x00000200L
#define OBJ_CASE_INSENSITIVE                0x00000040L
//#define NtCurrentPeb()  ( (PPEB) __readgsqword(0x60) )


#define SETBIT(X,Y)     X|=(1ULL<<(Y))
#define UNSETBIT(X,Y)   X&=(~(1ULL<<(Y)))

typedef struct _OBJECT_ATTRIBUTES
{
    ULONG           Length;
    HANDLE          RootDirectory;
    PVOID		    ObjectName;
    ULONG           Attributes;
    PVOID           SecurityDescriptor;
    PVOID           SecurityQualityOfService;
}  OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

typedef struct _PHYSICAL_MEMORY_RANGE
{
    PHYSICAL_ADDRESS BaseAddress;
    LARGE_INTEGER NumberOfBytes;
} PHYSICAL_MEMORY_RANGE, *PPHYSICAL_MEMORY_RANGE;

int64_t GNameAddr = 0;

struct MemoryController
{
    template<typename T>
    T& ReadPhysicalUnsafe(uint64_t Pa)
    {
        return *(T*)(PhysicalMemoryBegin + Pa);
    }

    PUCHAR PhysicalMemoryBegin;
    SIZE_T PhysicalMemorySize;

    uint64_t TargetDirectoryBase;

    uint64_t CurrentDirectoryBase;
    uint64_t CurrentEProcess;

    uint64_t UniqueProcessIdOffset;
    uint64_t DirectoryTableBaseOffset;
    uint64_t ActiveProcessLinksOffset;

    uint64_t CurrentProcessBase;
    uint64_t ProcessBaseOffset;

    uint64_t TargetProcessBase;

    NTSTATUS CreationStatus;

    uint64_t FindEProcess(uint64_t Pid)
    {
        uint64_t EProcess = this->CurrentEProcess;

        do
        {
            if (this->Read<uint64_t>(EProcess + this->UniqueProcessIdOffset) == Pid)
                return EProcess;

            LIST_ENTRY Le = this->Read<LIST_ENTRY>(EProcess + this->ActiveProcessLinksOffset);
            EProcess = (uint64_t)Le.Flink - this->ActiveProcessLinksOffset;
        } while (EProcess != this->CurrentEProcess);

        return 0;
    }

    void AttachTo(uint64_t EProcess)
    {
        this->TargetDirectoryBase = this->Read<uint64_t>(EProcess + this->DirectoryTableBaseOffset);
    }

    void Detach()
    {
        this->TargetDirectoryBase = this->CurrentDirectoryBase;
    }

    uint64_t ReadProcessBase(uint64_t EProcess)
    {
        this->TargetProcessBase = this->Read<uint64_t>(EProcess + this->ProcessBaseOffset);

        return this->TargetProcessBase;
    }

    uint64_t GetProcessBase()
    {
        return this->TargetProcessBase;
    }

    struct PageTableInfo
    {
        PML4E* Pml4e;
        PDPTE* Pdpte;
        PDE* Pde;
        PTE* Pte;
    };

    PageTableInfo QueryPageTableInfo(PVOID Va)
    {
        PageTableInfo Pi = { 0,0,0,0 };

        VIRT_ADDR Addr = { (uint64_t)Va };
        PTE_CR3 Cr3 = { TargetDirectoryBase };

        {
            uint64_t a = PFN_TO_PAGE(Cr3.pml4_p) + sizeof(PML4E) * Addr.pml4_index;
            if (a > this->PhysicalMemorySize)
                return Pi;
            PML4E& e = ReadPhysicalUnsafe<PML4E>(a);
            if (!e.present)
                return Pi;
            Pi.Pml4e = &e;
        }
        {
            uint64_t a = PFN_TO_PAGE(Pi.Pml4e->pdpt_p) + sizeof(PDPTE) * Addr.pdpt_index;
            if (a > this->PhysicalMemorySize)
                return Pi;
            PDPTE& e = ReadPhysicalUnsafe<PDPTE>(a);
            if (!e.present)
                return Pi;
            Pi.Pdpte = &e;
        }
        {
            uint64_t a = PFN_TO_PAGE(Pi.Pdpte->pd_p) + sizeof(PDE) * Addr.pd_index;
            if (a > this->PhysicalMemorySize)
                return Pi;
            PDE& e = ReadPhysicalUnsafe<PDE>(a);
            if (!e.present)
                return Pi;
            Pi.Pde = &e;
            if (Pi.Pde->page_size)
                return Pi;
        }
        {
            uint64_t a = PFN_TO_PAGE(Pi.Pde->pt_p) + sizeof(PTE) * Addr.pt_index;
            if (a > this->PhysicalMemorySize)
                return Pi;
            PTE& e = ReadPhysicalUnsafe<PTE>(a);
            if (!e.present)
                return Pi;
            Pi.Pte = &e;
        }
        return Pi;
    }

    uint64_t VirtToPhys(PVOID Va)
    {
        auto Info = QueryPageTableInfo(Va);

        if (!Info.Pde)
            return 0;

        uint64_t Pa = 0;

        if (Info.Pde->page_size)
        {
            Pa = PFN_TO_PAGE(Info.Pde->pt_p);
            Pa += (uint64_t)Va & (0x200000 - 1);
        }
        else
        {
            if (!Info.Pte)
                return 0;
            Pa = PFN_TO_PAGE(Info.Pte->page_frame);
            Pa += (uint64_t)Va & (0x1000 - 1);
        }
        return Pa;
    }

    void IterPhysRegion(PVOID StartVa, SIZE_T Size, std::function<void(PVOID Va, uint64_t, SIZE_T)> Fn)
    {
        PUCHAR It = (PUCHAR)StartVa;
        PUCHAR End = It + Size;

        while (It < End)
        {
            SIZE_T Size = (PUCHAR)(((uint64_t)It + 0x1000) & (~0xFFF)) - It;

            if ((It + Size) > End)
                Size = End - It;

            uint64_t Pa = VirtToPhys(It);

            Fn(It, Pa, Size);

            It += Size;
        }
    }

    void AttachIfCanRead(uint64_t EProcess, PVOID Adr)
    {
        this->AttachTo(EProcess);
        if (!this->VirtToPhys(Adr))
            this->Detach();
    }

    SIZE_T ReadVirtual(PVOID Src, PVOID Dst, SIZE_T Size)
    {
        PUCHAR It = (PUCHAR)Dst;
        SIZE_T BytesRead = 0;

        this->IterPhysRegion(Src, Size, [&](PVOID Va, uint64_t Pa, SIZE_T Sz)
        {
            if (Pa)
            {
                BytesRead += Sz;
                memcpy(It, PhysicalMemoryBegin + Pa, Sz);
                It += Sz;
            }
        });

        return BytesRead;
    }

    SIZE_T WriteVirtual(PVOID Src, PVOID Dst, SIZE_T Size)
    {
        PUCHAR It = (PUCHAR)Src;
        SIZE_T BytesRead = 0;

        this->IterPhysRegion(Dst, Size, [&](PVOID Va, uint64_t Pa, SIZE_T Sz)
        {
            if (Pa)
            {
                BytesRead += Sz;
                memcpy(PhysicalMemoryBegin + Pa, It, Sz);
                It += Sz;
            }
        });

        return BytesRead;
    }

    template<typename T>
    T Read(int64_t From)
    {
        char Buffer[sizeof(T)];
        this->ReadVirtual((PVOID)From, Buffer, sizeof(T));
        return *(T*)(Buffer);
    }

    template<typename T>
    void Write(int64_t To, const T & Data)
    {
        this->WriteVirtual((PVOID)&Data, (PVOID)To, sizeof(T));
    }


};

void DecStrW(wchar_t *adr)
{
    int len = wcslen(adr);
    for (unsigned i = 0; i < len; i++)
    {
        adr[i] ^= 50;
        adr[i] ^= 72;
    }
}

void EncStrW(wchar_t *adr)
{
    int len = wcslen(adr);
    for (unsigned i = 0; i < len; i++)
    {
        adr[i] ^= 72;
        adr[i] ^= 50;
    }
}

NON_PAGED_DATA static wchar_t PhysicalMemoryName[23] = { 38, 62, 31, 12, 19, 25, 31, 38, 42, 18, 3, 9, 19, 25, 27, 22, 55, 31, 23, 21, 8, 3, 0 };

static MemoryController Mc_InitContext(CapcomContext** CpCtxReuse = 0, KernelContext** KrCtxReuse = 0)
{
    //VMProtectBeginUltra("Mc_InitContext");
    assert(Np_LockSections());

    KernelContext* KrCtx = Kr_InitContext();

    CapcomContext* CpCtx = Cl_InitContext();


    assert(CpCtx);
    assert(KrCtx);

    Khu_Init(CpCtx, KrCtx);

    //printf( "[+] Mapping physical memory to user-mode!\n" );

    //system("pause");

    NON_PAGED_DATA static MemoryController Controller = { 0 };

    Controller.CurrentProcessBase = (uint64_t)GetModuleHandle(NULL);

    NON_PAGED_DATA static auto k_ZwOpenSection = KrCtx->GetProcAddress<>(VMProtectDecryptStringA("ZwOpenSection"));
    NON_PAGED_DATA static auto k_ZwMapViewOfSection = KrCtx->GetProcAddress<>(VMProtectDecryptStringA("ZwMapViewOfSection"));
    NON_PAGED_DATA static auto k_ZwClose = KrCtx->GetProcAddress<>(VMProtectDecryptStringA("ZwClose"));
    NON_PAGED_DATA static auto k_PsGetCurrentProcess = KrCtx->GetProcAddress<>(VMProtectDecryptStringA("PsGetCurrentProcess"));
    NON_PAGED_DATA static auto k_PsGetCurrentProcessId = KrCtx->GetProcAddress<>(VMProtectDecryptStringA("PsGetCurrentProcessId"));
    NON_PAGED_DATA static auto k_PsGetProcessId = KrCtx->GetProcAddress<>(VMProtectDecryptStringA("PsGetProcessId"));

    NON_PAGED_DATA static auto k_MmGetPhysicalMemoryRanges = KrCtx->GetProcAddress<PPHYSICAL_MEMORY_RANGE(*)()>(VMProtectDecryptStringA("MmGetPhysicalMemoryRanges"));

    NON_PAGED_DATA static auto k_MmGetSystemRoutineAddress = KrCtx->GetProcAddress<>(VMProtectDecryptStringA("MmGetSystemRoutineAddress"));

    DecStrW(PhysicalMemoryName);
    //NON_PAGED_DATA static wchar_t PhysicalMemoryName[] = L"\\Device\\PhysicalMemory";

    NON_PAGED_DATA static OBJECT_ATTRIBUTES PhysicalMemoryAttributes;
    NON_PAGED_DATA static UNICODE_STRING PhysicalMemoryNameUnicode;

    PhysicalMemoryNameUnicode.Buffer = PhysicalMemoryName;
    PhysicalMemoryNameUnicode.Length = sizeof(PhysicalMemoryName) - 2;
    PhysicalMemoryNameUnicode.MaximumLength = sizeof(PhysicalMemoryName);

    PhysicalMemoryAttributes.Length = sizeof(PhysicalMemoryAttributes);
    PhysicalMemoryAttributes.Attributes = OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE;
    PhysicalMemoryAttributes.ObjectName = &PhysicalMemoryNameUnicode;
    PhysicalMemoryAttributes.RootDirectory = 0;
    PhysicalMemoryAttributes.SecurityDescriptor = 0;
    PhysicalMemoryAttributes.SecurityQualityOfService = 0;

    CpCtx->ExecuteInKernel(NON_PAGED_LAMBDA()
    {
        auto Range = k_MmGetPhysicalMemoryRanges();

        while (Range->NumberOfBytes.QuadPart)
        {
            Controller.PhysicalMemorySize = max(Controller.PhysicalMemorySize, Range->BaseAddress.QuadPart + Range->NumberOfBytes.QuadPart);
            Range++;
        }

        HANDLE PhysicalMemoryHandle = 0;
        Controller.CreationStatus = Khk_CallPassive(k_ZwOpenSection, &PhysicalMemoryHandle, uint64_t(SECTION_ALL_ACCESS), &PhysicalMemoryAttributes);

        if (!Controller.CreationStatus)
        {
            Controller.CreationStatus = Khk_CallPassive
            (
                k_ZwMapViewOfSection,
                PhysicalMemoryHandle,
                NtCurrentProcess(),
                &Controller.PhysicalMemoryBegin,
                0ull,
                0ull,
                0ull,
                &Controller.PhysicalMemorySize,
                1ull,
                0,
                PAGE_READWRITE
            );

            if (!Controller.CreationStatus)
            {
                Controller.CurrentEProcess = k_PsGetCurrentProcess();
                Controller.CurrentDirectoryBase = __readcr3();
                uint64_t Pid = k_PsGetProcessId(Controller.CurrentEProcess);


                uint32_t PidOffset = *(uint32_t*)((PUCHAR)k_PsGetProcessId + 3);
                if (PidOffset < 0x400 && *(uint64_t*)(Controller.CurrentEProcess + PidOffset) == Pid)
                {
                    Controller.UniqueProcessIdOffset = PidOffset;
                    Controller.ActiveProcessLinksOffset = Controller.UniqueProcessIdOffset + 0x8;
                }

                for (int i = 0; i < 0x400; i += 0x8)
                {
                    uint64_t* Ptr = (uint64_t*)(Controller.CurrentEProcess + i);
                    if (!Controller.UniqueProcessIdOffset && Ptr[0] & 0xFFFFFFFF == Pid && (Ptr[1] > 0xffff800000000000) && (Ptr[2] > 0xffff800000000000) && ((Ptr[1] & 0xF) == (Ptr[2] & 0xF)))
                    {
                        Controller.UniqueProcessIdOffset = i;
                        Controller.ActiveProcessLinksOffset = Controller.UniqueProcessIdOffset + 0x8;
                    }
                    else if (!Controller.DirectoryTableBaseOffset && Ptr[0] == __readcr3())
                    {
                        Controller.DirectoryTableBaseOffset = i;
                    }

                    if (*(uint64_t*)(Controller.CurrentEProcess + i) == Controller.CurrentProcessBase)
                    {
                        Controller.ProcessBaseOffset = i;
                    }
                }

                /*
                ULONG64 i = 0;
                PULONG64 pAddrOfFnc = 0;
                ULONG64 fncAddr = (ULONG64)k_MmGetSystemRoutineAddress(&PsSetLoadImageNotifyRoutineUnicode);
                if (fncAddr)
                {
                fncAddr += 0x50;
                for (i = fncAddr; i < fncAddr + 0x15; i++)
                {
                if (*(UCHAR*)i == 0x8B && *(UCHAR*)(i + 1) == 0x05)
                {
                LONG OffsetAddr = 0;
                memcpy(&OffsetAddr, (UCHAR*)(i + 2), 4);

                pAddrOfFnc = (ULONG64*)(OffsetAddr + i + 0x6);
                break;
                }
                }
                }

                BOOLEAN enableThread = false;
                BOOLEAN enableImage = false;

                ULONG64 varaddress = (ULONG64)pAddrOfFnc;
                if (varaddress)
                {
                ULONG val = *(ULONG*)(varaddress);
                if (!enableThread)
                {
                UNSETBIT(val, 3);
                UNSETBIT(val, 4);
                }
                else
                {
                SETBIT(val, 3);
                SETBIT(val, 4);
                }
                if (!enableImage)
                {
                UNSETBIT(val, 0);
                }
                else
                {
                SETBIT(val, 0);
                }
                *(ULONG*)(varaddress) = val;
                }
                }*/
            }
            k_ZwClose(PhysicalMemoryHandle);
        }

    });

    if (!Controller.UniqueProcessIdOffset)
        Controller.CreationStatus = 1;
    if (!Controller.DirectoryTableBaseOffset)
        Controller.CreationStatus = 2;
    if (!Controller.DirectoryTableBaseOffset)
        Controller.CreationStatus = 3;

#ifndef VMP_VERSION
    printf( "[+] PhysicalMemoryBegin: %16llx\n", Controller.PhysicalMemoryBegin );
    printf( "[+] PhysicalMemorySize:  %16llx\n", Controller.PhysicalMemorySize );

    printf( "[+] CurrentProcessBase:  %16llx\n", Controller.CurrentProcessBase);
    printf( "[+] CurrentProcessCr3:   %16llx\n", Controller.CurrentDirectoryBase );
    printf( "[+] CurrentEProcess:     %16llx\n", Controller.CurrentEProcess );

    printf( "[+] DirectoryTableBase@  %16llx\n", Controller.DirectoryTableBaseOffset );
    printf( "[+] UniqueProcessId@     %16llx\n", Controller.UniqueProcessIdOffset );
    printf( "[+] ActiveProcessLinks@  %16llx\n", Controller.ActiveProcessLinksOffset );
    printf( "[+] ProcessBaseOffset@   %16llx\n", Controller.ProcessBaseOffset);
    printf( "[+] Status:              %16llx\n", Controller.CreationStatus );
#endif
    

    Controller.TargetDirectoryBase = Controller.CurrentDirectoryBase;

    if (!CpCtxReuse)
        Cl_FreeContext(CpCtx);
    else
        *CpCtxReuse = CpCtx;

    if (!KrCtxReuse)
        Kr_FreeContext(KrCtx);
    else
        *KrCtxReuse = KrCtx;

    EncStrW(PhysicalMemoryName);

    //VMProtectEnd();
    return Controller;
}




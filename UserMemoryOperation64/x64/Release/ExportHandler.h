#pragma once

#ifdef USERMEMORYOPERATION64_EXPORTS
#define USERMEMORYOPERATION64_API extern "C" __declspec(dllexport)
#else
#define USERMEMORYOPERATION64_API extern "C" __declspec(dllimport)
#endif

USERMEMORYOPERATION64_API LONG Initialization();

USERMEMORYOPERATION64_API uint64_t GetTargetBase(uint64_t pid);

USERMEMORYOPERATION64_API void WriteByte(LPVOID Dst, PVOID  Src, SIZE_T Size);

USERMEMORYOPERATION64_API void ReadByte(LPVOID Dst, PVOID  Src, SIZE_T Size);
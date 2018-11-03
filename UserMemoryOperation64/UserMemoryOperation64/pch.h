#pragma once
#include <Windows.h>
#include <intrin.h>
#include <iostream>
#include <stdio.h>
#include <intrin.h>
#include <inttypes.h>
#include <functional>
#include <assert.h>
#include "ExportHandler.h"

// øÿ÷∆ «∑ÒVMP∞Ê±æ
#define VMP_VERSION

#include "VMProtectSDK.h"

#ifdef VMP_VERSION
#define PRINT(format, ...)
#else
#define PRINT(format, ...) printf(format, __VA_ARGS__)
#endif // VMP_VERSION



#ifndef PFN_TO_PAGE
#define PFN_TO_PAGE(pfn) ( pfn << 12 )
#pragma pack(push, 1)
typedef union CR3_
{
    uint64_t value;
    struct
    {
        uint64_t ignored_1 : 3;
        uint64_t write_through : 1;
        uint64_t cache_disable : 1;
        uint64_t ignored_2 : 7;
        uint64_t pml4_p : 40;
        uint64_t reserved : 12;
    };
} PTE_CR3;

typedef union VIRT_ADDR_
{
    uint64_t value;
    void *pointer;
    struct
    {
        uint64_t offset : 12;
        uint64_t pt_index : 9;
        uint64_t pd_index : 9;
        uint64_t pdpt_index : 9;
        uint64_t pml4_index : 9;
        uint64_t reserved : 16;
    };
} VIRT_ADDR;

typedef uint64_t PHYS_ADDR;

typedef union PML4E_
{
    uint64_t value;
    struct
    {
        uint64_t present : 1;
        uint64_t rw : 1;
        uint64_t user : 1;
        uint64_t write_through : 1;
        uint64_t cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t ignored_1 : 1;
        uint64_t reserved_1 : 1;
        uint64_t ignored_2 : 4;
        uint64_t pdpt_p : 40;
        uint64_t ignored_3 : 11;
        uint64_t xd : 1;
    };
} PML4E;

typedef union PDPTE_
{
    uint64_t value;
    struct
    {
        uint64_t present : 1;
        uint64_t rw : 1;
        uint64_t user : 1;
        uint64_t write_through : 1;
        uint64_t cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t page_size : 1;
        uint64_t ignored_2 : 4;
        uint64_t pd_p : 40;
        uint64_t ignored_3 : 11;
        uint64_t xd : 1;
    };
} PDPTE;

typedef union PDE_
{
    uint64_t value;
    struct
    {
        uint64_t present : 1;
        uint64_t rw : 1;
        uint64_t user : 1;
        uint64_t write_through : 1;
        uint64_t cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t page_size : 1;
        uint64_t ignored_2 : 4;
        uint64_t pt_p : 40;
        uint64_t ignored_3 : 11;
        uint64_t xd : 1;
    };
} PDE;

typedef union PTE_
{
    uint64_t value;
    VIRT_ADDR vaddr;
    struct
    {
        uint64_t present : 1;
        uint64_t rw : 1;
        uint64_t user : 1;
        uint64_t write_through : 1;
        uint64_t cache_disable : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t pat : 1;
        uint64_t global : 1;
        uint64_t ignored_1 : 3;
        uint64_t page_frame : 40;
        uint64_t ignored_3 : 11;
        uint64_t xd : 1;
    };
} PTE;
#pragma pack(pop)
#endif

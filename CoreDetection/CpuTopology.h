//-------------------------------------------------------------------------------------
// CpuTopology.h
// 
// CpuToplogy class declaration.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//-------------------------------------------------------------------------------------
#pragma once

#include <Windows.h>

class ICpuTopology;

//---------------------------------------------------------------------------------
// Name: CpuToplogy
// Desc: This class constructs a supported cpu topology implementation object on
//       initialization and forwards calls to it.  This is the Abstraction class
//       in the traditional Bridge Pattern.
//---------------------------------------------------------------------------------
class CpuTopology
{
public:
                CpuTopology( bool bForceCpuid = false );
                ~CpuTopology();

    bool        IsDefaultImpl() const noexcept;
    DWORD       NumberOfProcessCores() const noexcept;
    DWORD       NumberOfSystemCores() const noexcept;
    DWORD_PTR   CoreAffinityMask( DWORD coreIdx ) const noexcept;

    void        ForceCpuid( bool bForce );
private:
    void        Destroy_();

    ICpuTopology* m_pImpl;
};

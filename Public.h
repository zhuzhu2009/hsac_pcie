/*++
    Copyright (c) ESSS.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

#include <initguid.h> 
//
// The following value is arbitrarily chosen from the space defined 
// by Microsoft as being "for non-Microsoft use"
//
// NOTE: we use OSR's GUID_OSR_HSAC_INTERFACE GUID value so that we 
// can use OSR's HSACTest program      :-)
//
// {08EDA5A0-DA4F-42A8-A820-FBA2FA235AF7}
DEFINE_GUID(GUID_HSAC_INTERFACE, 
	0x8eda5a0, 0xda4f, 0x42a8, 0xa8, 0x20, 0xfb, 0xa2, 0xfa, 0x23, 0x5a, 0xf7);

//
// GUID definition are required to be outside of header inclusion pragma to avoid
// error during precompiled headers.
//

#ifndef PUBLIC_H
#define PUBLIC_H

#define IOCTL_GET_VERSION				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED,   FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_RESET						CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_GET_REGISTER				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_OUT_DIRECT,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_GET_SINGLE_REGISTER		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_OUT_DIRECT,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_SET_REGISTER				CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_GET_SRAM					CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_OUT_DIRECT,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_SET_SRAM					CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_MAP_DMA_BUF_ADDR			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_OUT_DIRECT, FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_UNMAP_DMA_BUF_ADDR		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_OUT_DIRECT, FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_SET_DMA_PROFILE			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_DIRECT_DMA_READ			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x810, METHOD_BUFFERED,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)
#define IOCTL_DIRECT_DMA_WRITE			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED,	FILE_READ_ACCESS|FILE_WRITE_ACCESS)

#endif


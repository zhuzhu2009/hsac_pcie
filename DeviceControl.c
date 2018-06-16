/*++

Copyright (c) ESSS.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Read.c

Abstract:


Environment:

    Kernel mode

--*/

#include "precomp.h"

#include "DeviceControl.tmh"

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
VOID
HSACEvtIoDeviceControl(
    __in WDFQUEUE      Queue,
    __in WDFREQUEST    Request,
    __in size_t         OutputBufferLength,
    __in size_t         InputBufferLength,
    __in ULONG         IoControlCode
    )
/*++
Routine Description:

    This event is called when the framework receives IRP_MJ_DEVICE_CONTROL
    requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.
Return Value:

    VOID

--*/
{
    PDEVICE_EXTENSION		devExt;
    size_t                  length = 0;
    NTSTATUS                status = STATUS_SUCCESS;
    PVOID                   pInputBuffer = NULL;
	PVOID					pOutputBuffer = NULL;

	ULONG a1 = 0,a2 = 0;
	ULONG readIndex = 0, readNum = 0;

    UNREFERENCED_PARAMETER( InputBufferLength  );
    UNREFERENCED_PARAMETER( OutputBufferLength  );

    //
    // Get the device extension.
    //
    devExt = HSACGetDeviceContext(WdfIoQueueGetDevice( Queue ));
#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
		"HSACEvtIoDeviceControl InputBufferLength: 0x%x, OutputBufferLength: 0x%x\n", 
		InputBufferLength, OutputBufferLength);
#endif
    //
    // Handle this request specific code.
    //
    switch (IoControlCode) {
	case IOCTL_GET_REGISTER:
		{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
				"IOCTL_GET_REGISTER-->require length: 0x%x\n", sizeof(ULONG));
#endif

			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			status = WdfRequestRetrieveOutputBuffer(Request, 0, &pOutputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			readIndex = *(PULONG)pInputBuffer;
			readNum = *((PULONG)pInputBuffer + 1);
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
				"readIndex: %d, readNum: %d\n", readIndex, readNum);
#endif
			if (readNum == 0)
			{
				length = 0;
				break;
			}
			//if (readNum > 1)
			//{
				if (readIndex + readNum > (sizeof(HSAC_REGS)/sizeof(unsigned int) ))
				{
					readNum = (sizeof(HSAC_REGS)/sizeof(unsigned int) ) - readIndex;
				}
				WdfInterruptAcquireLock( devExt->Interrupt );
				READ_REGISTER_BUFFER_ULONG(((PULONG) devExt->Regs) + readIndex, (PULONG)pOutputBuffer, readNum);
				WdfInterruptReleaseLock( devExt->Interrupt );
			//}
			//else
			//{
			//	*(PULONG)pOutputBuffer = READ_REGISTER_ULONG( ((PULONG) devExt->Regs) + readIndex );
			//}
			
			length = readNum;
			break;
		}
	case IOCTL_GET_SINGLE_REGISTER:
		{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
				"IOCTL_GET_REGISTER-->require length: 0x%x\n", sizeof(ULONG));
#endif

			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			status = WdfRequestRetrieveOutputBuffer(Request, 0, &pOutputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			readIndex = *(PULONG)pInputBuffer;
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
				"readIndex: %d\n", readIndex);
#endif
			WdfInterruptAcquireLock( devExt->Interrupt );
			*(PULONG)pOutputBuffer = READ_REGISTER_ULONG( ((PULONG) devExt->Regs) + readIndex );
			WdfInterruptReleaseLock( devExt->Interrupt );

			length = sizeof(ULONG);
			break;
		}
	case IOCTL_SET_REGISTER:
		{
			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
#endif
				break;
			}
			WdfInterruptAcquireLock( devExt->Interrupt );
			WRITE_REGISTER_ULONG( ((PULONG) devExt->Regs) + (*(PULONG)pInputBuffer), *((PULONG)pInputBuffer + 1) );
			WdfInterruptReleaseLock( devExt->Interrupt );

//			//a1 = *(PULONG)pInputBuffer;
//			//a2 = *((PULONG)pInputBuffer + 1);
//
//#if (DBG != 0)
//			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
//				"IOCTL_SET_REGISTER-->offset:0x%x, value:0x%x\n", a1, a2);
//#endif
//
//			//a2 = READ_REGISTER_ULONG( ((PULONG) devExt->Regs) + (*(PULONG)pInputBuffer) );
//
//#if (DBG != 0)
//			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
//				"IOCTL_SET_REGISTER-->offset:0x%x, value:0x%x\n", a1, a2);
//#endif

			length = 0;
			break;
		}
	case IOCTL_GET_SRAM:
		{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
				"IOCTL_GET_SRAM-->require length: 0x%x\n", sizeof(ULONG));
#endif

			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			status = WdfRequestRetrieveOutputBuffer(Request, 0, &pOutputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			readIndex = *(PULONG)pInputBuffer;
			readNum = *((PULONG)pInputBuffer + 1);

			if (readNum == 0)
			{
				length = 0;
				break;
			}
			//if (readNum > 1)
			//{
			if (readIndex + readNum > (devExt->SRAMLength/sizeof(ULONG) ))
			{
				readNum = (devExt->SRAMLength/sizeof(ULONG) ) - readIndex;
			}

#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
				"readIndex: %d, readNum: %d\n", readIndex, readNum);
#endif

			WdfInterruptAcquireLock( devExt->Interrupt );
			READ_REGISTER_BUFFER_ULONG(((PULONG) devExt->SRAMBase) + readIndex, (PULONG)pOutputBuffer, readNum);
			WdfInterruptReleaseLock( devExt->Interrupt );
			//}
			//else
			//{
			//	*(PULONG)pOutputBuffer = READ_REGISTER_ULONG( ((PULONG) devExt->Regs) + readIndex );
			//}

			length = readNum;
			break;
		}
	case IOCTL_SET_SRAM:
		{
			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			readIndex = *(PULONG)pInputBuffer;
			readNum = *((PULONG)pInputBuffer + 1);

			if (readNum == 0)
			{
				length = 0;
				break;
			}
			//if (readNum > 1)
			//{
			if (readIndex + readNum > (devExt->SRAMLength/sizeof(ULONG) ))
			{
				readNum = (devExt->SRAMLength/sizeof(ULONG) ) - readIndex;
			}

#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
				"readIndex: %d, readNum: %d\n", readIndex, readNum);
#endif

			WdfInterruptAcquireLock( devExt->Interrupt );
			WRITE_REGISTER_BUFFER_ULONG( ((PULONG) devExt->SRAMBase) + readIndex, (PULONG)pInputBuffer + 2, readNum);
			WdfInterruptReleaseLock( devExt->Interrupt );
			//			//a1 = *(PULONG)pInputBuffer;
			//			//a2 = *((PULONG)pInputBuffer + 1);
			//
			//#if (DBG != 0)
			//			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
			//				"IOCTL_SET_REGISTER-->offset:0x%x, value:0x%x\n", a1, a2);
			//#endif
			//
			//			//a2 = READ_REGISTER_ULONG( ((PULONG) devExt->Regs) + (*(PULONG)pInputBuffer) );
			//
			//#if (DBG != 0)
			//			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS,
			//				"IOCTL_SET_REGISTER-->offset:0x%x, value:0x%x\n", a1, a2);
			//#endif

			length = readNum;
			break;
		}
	case IOCTL_MAP_DMA_BUF_ADDR:
		{
			ULONG_PTR address = 0;

			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
#endif
				break;
			}
			status = WdfRequestRetrieveOutputBuffer(Request, 0, &pOutputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			if (devExt->MapFlag == 1)
			{
				HSACUnmapUserAddress(devExt);
				devExt->MapFlag = 0;
			}
			HSACMapUserAddress( devExt );
			devExt->MapFlag = 1;

#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
			if (*(PULONG)pInputBuffer == 0)
			{
				length = devExt->readCommonBufferNum * sizeof(PVOID);
				RtlCopyMemory(pOutputBuffer, devExt->pReadUserAddress, length);
			} 
			else if (*(PULONG)pInputBuffer == 1)
			{
				length = devExt->writeCommonBufferNum * sizeof(PVOID);
				RtlCopyMemory(pOutputBuffer, devExt->pWriteUserAddress, length);
			}
			else
			{
				status = STATUS_INVALID_DEVICE_REQUEST;
			}
#else
			if (*(PULONG)pInputBuffer == 0)
			{
				address = (ULONG_PTR)devExt->ReadUserAddress0;
			} 
			else if (*(PULONG)pInputBuffer == 1)
			{
				address = (ULONG_PTR)devExt->WriteUserAddress0;
			}
			else
			{
				status = STATUS_INVALID_DEVICE_REQUEST;
			}

#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
				"UserAddress: 0x%I64x", address);
#endif

			RtlCopyMemory(pOutputBuffer, &address, sizeof(PVOID));
#endif
			break;
		}
	case IOCTL_UNMAP_DMA_BUF_ADDR:
		{
			if (devExt->MapFlag == 1)
			{
				HSACUnmapUserAddress(devExt);
				devExt->MapFlag = 0;
			}
			break;
		}
	case IOCTL_SET_DMA_PROFILE:
		{
			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
					"WdfRequestRetrieveInputBuffer failed 0x%x\n", status);
#endif
				break;
			}

			devExt->dmaProfile = (WDF_DMA_PROFILE)(*(PULONG)pInputBuffer);

#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
				"dmaProfile: %d\n", devExt->dmaProfile);
#endif
			length = 0;
			break;
		}
	case IOCTL_DIRECT_DMA_READ:
		{
//			ULONG dma1Ctl = 0;
//			status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
//			devExt->ReadSize = *(PULONG)pInputBuffer;
//
//#if (DBG != 0)
//			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
//				"devExt->ReadSize: %d\n", devExt->ReadSize);
//#endif
//
//			WdfInterruptAcquireLock( devExt->Interrupt );
//
//			WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR32,
//				devExt->ReadCommonBufferBaseLA.LowPart);
//			WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR64,
//				devExt->ReadCommonBufferBaseLA.HighPart );
//
//			WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_SIZE, devExt->ReadSize);
//			devExt->ReadSize = 0;
//
//			dma1Ctl = DMA_CTRL_START;
//			//WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_CTRL, dma1Ctl);
//
//			WdfInterruptReleaseLock( devExt->Interrupt );
			break;
		}
	case IOCTL_DIRECT_DMA_WRITE:
		{
			//ULONG dma0Ctl = 0;
			//status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &length);
			//devExt->WriteSize = *(PULONG)pInputBuffer;

			//WdfInterruptAcquireLock( devExt->Interrupt );

			//WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR32,
			//	devExt->WriteCommonBufferBaseLA.LowPart );
			//WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR64,
			//	devExt->WriteCommonBufferBaseLA.HighPart );

			//WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_SIZE, devExt->WriteSize);

			//dma0Ctl = DMA_CTRL_START;
			//WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_CTRL, dma0Ctl);

			//WdfInterruptReleaseLock( devExt->Interrupt );
			break;
		}
	case IOCTL_RESET: // code == 0x801
		{
			status = WdfRequestRetrieveOutputBuffer(Request, 0, &pOutputBuffer, &length);
			if( !NT_SUCCESS(status)) {
#if (DBG != 0)
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS,
								"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
#endif
				break;
			}
			//*(PULONG) buffer = 0x0004000A;

			//status = STATUS_SUCCESS;
			length = sizeof(ULONG);
			break;
		}

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
    }

    WdfRequestCompleteWithInformation(Request, status, length);
}

/*++

Copyright (c) ESSS.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Init.c

Abstract:

    Contains most of initialization functions

Environment:

    Kernel mode

--*/

#include "precomp.h"

#include "Init.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, HSACInitializeDeviceExtension)
#pragma alloc_text (PAGE, HSACPrepareHardware)
#pragma alloc_text (PAGE, HSACInitializeDMA)
#endif

NTSTATUS
HSACInitializeDeviceExtension(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    This routine is called by EvtDeviceAdd. Here the device context is
    initialized and all the software resources required by the device is
    allocated.

Arguments:

    DevExt     Pointer to the Device Extension

Return Value:

     NTSTATUS

--*/
{
    NTSTATUS    status;
    ULONG       dteCount;
    WDF_IO_QUEUE_CONFIG  queueConfig;

    PAGED_CODE();
	DevExt->RegsBase = NULL;
	DevExt->RegsLength = 0;
	DevExt->SRAMBase = NULL;
	DevExt->SRAMLength = 0;

    //
    // Set Maximum Transfer Length (which must be less than the SRAM size).
    //
    DevExt->MaximumTransferLength = HSAC_MAXIMUM_TRANSFER_LENGTH;
    if(DevExt->MaximumTransferLength > HSAC_PHY_BUF_SIZE) {
        DevExt->MaximumTransferLength = HSAC_PHY_BUF_SIZE;
    }

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "MaximumTransferLength %d", DevExt->MaximumTransferLength);
#endif
    //
    // Calculate the number of DMA_TRANSFER_ELEMENTS + 1 needed to
    // support the MaximumTransferLength.
    //
    dteCount = BYTES_TO_PAGES((ULONG) ROUND_TO_PAGES(
        DevExt->MaximumTransferLength) + PAGE_SIZE);

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Number of DTEs %d", dteCount);
#endif
    //
    // Set the number of DMA_TRANSFER_ELEMENTs (DTE) to be available.
    //
    DevExt->WriteTransferElements = dteCount;
    DevExt->ReadTransferElements  = dteCount;

	DevExt->MapFlag = 0;

#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	DevExt->readCommonBufferNum = 0;
	DevExt->writeCommonBufferNum = 0;
#endif

    //
    // The HSAC has two DMA Channels. This driver will use DMA Channel 0
    // as the "ToDevice" channel (Writes) and DMA Channel 1 as the
    // "From Device" channel (Reads).
    //
    // In order to support "duplex" DMA operation (the ability to have
    // concurrent reads and writes) two Dispatch Queues are created:
    // one for the Write (ToDevice) requests and another for the Read
    // (FromDevice) requests.  While each Dispatch Queue will operate
    // independently for each other, the requests within a given Dispatch
    // Queue will be serialized. This is hardware can only process one request
    // per DMA Channel at a time.
    //


    //
    // Setup a queue to handle only IRP_MJ_WRITE requests in Sequential
    // dispatch mode. This mode ensures there is only one write request
    // outstanding in the driver at any time. Framework will present the next
    // request only if the current request is completed.
    // Since we have configured the queue to dispatch all the specific requests
    // we care about, we don't need a default queue.  A default queue is
    // used to receive requests that are not predefined to goto
    // a specific queue.
    //
    WDF_IO_QUEUE_CONFIG_INIT ( &queueConfig,
                              WdfIoQueueDispatchSequential);

    queueConfig.EvtIoWrite = HSACEvtIoWrite;

    status = WdfIoQueueCreate( DevExt->Device,
                                           &queueConfig,
                                           WDF_NO_OBJECT_ATTRIBUTES,
                                           &DevExt->WriteQueue );

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfIoQueueCreate failed: %!STATUS!", status);
#endif
        return status;
    }

    //
    // Set the Write Queue forwarding for IRP_MJ_WRITE requests.
    //
    status = WdfDeviceConfigureRequestDispatching( DevExt->Device,
                                       DevExt->WriteQueue,
                                       WdfRequestTypeWrite);

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "DeviceConfigureRequestDispatching failed: %!STATUS!", status);
#endif
        return status;
    }


    //
    // Create a new IO Queue for IRP_MJ_READ requests in sequential mode.
    //
    WDF_IO_QUEUE_CONFIG_INIT( &queueConfig,
                              WdfIoQueueDispatchSequential);

    queueConfig.EvtIoRead = HSACEvtIoRead;

    status = WdfIoQueueCreate( DevExt->Device,
                               &queueConfig,
                               WDF_NO_OBJECT_ATTRIBUTES,
                               &DevExt->ReadQueue );

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfIoQueueCreate failed: %!STATUS!", status);
#endif
        return status;
    }

    //
    // Set the Read Queue forwarding for IRP_MJ_READ requests.
    //
    status = WdfDeviceConfigureRequestDispatching( DevExt->Device,
                                       DevExt->ReadQueue,
                                       WdfRequestTypeRead);

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "DeviceConfigureRequestDispatching failed: %!STATUS!", status);
#endif
        return status;
    }
	
	//
	// Create a new IO Queue for IRP_MJ_DEVICE_CONTROL requests in sequential mode.
	//
	WDF_IO_QUEUE_CONFIG_INIT( &queueConfig,
							  WdfIoQueueDispatchSequential);

	queueConfig.EvtIoDeviceControl = HSACEvtIoDeviceControl;

	status = WdfIoQueueCreate( DevExt->Device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&DevExt->DeviceControlQueue );

	if (!NT_SUCCESS (status)) {
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
			"WdfIoQueueCreate failed 0x%X\n", status);
#endif
		return status;
	}

	//
	// Set the Device Queue forwarding for IRP_MJ_DEVICE_CONTROL requests.
	//
	status = WdfDeviceConfigureRequestDispatching( DevExt->Device,
		DevExt->DeviceControlQueue,
		WdfRequestTypeDeviceControl);

	if(!NT_SUCCESS(status)) {
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
			"DeviceConfigureRequestDispatching failed: %!STATUS!", status);
#endif
		return status;
	}


    //
    // Create a WDFINTERRUPT object.
    //
    status = HSACInterruptCreate(DevExt);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = HSACInitializeDMA( DevExt );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    return status;
}

NTSTATUS
HSACPrepareHardware(
    IN PDEVICE_EXTENSION DevExt,
    IN WDFCMRESLIST     ResourcesTranslated
    )
/*++
Routine Description:

    Gets the HW resources assigned by the bus driver from the start-irp
    and maps it to system address space.

Arguments:

    DevExt      Pointer to our DEVICE_EXTENSION

Return Value:

     None

--*/
{
    ULONG               i;
    NTSTATUS            status = STATUS_SUCCESS;
    CHAR              * bar = NULL;

    BOOLEAN             foundRegs      = FALSE;
    PHYSICAL_ADDRESS    regsBasePA     = {0};
    ULONG               regsLength     = 0;

    BOOLEAN             foundSRAM      = FALSE;
    PHYSICAL_ADDRESS    SRAMBasePA     = {0};
    ULONG               SRAMLength     = 0;

    PCM_PARTIAL_RESOURCE_DESCRIPTOR  desc;

    PAGED_CODE();

    //
    // Parse the resource list and save the resource information.
    //
    for (i=0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {

        desc = WdfCmResourceListGetDescriptor( ResourcesTranslated, i );

        if(!desc) {
#if (DBG != 0)
            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                        "WdfResourceCmGetDescriptor failed");
#endif
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }

        switch (desc->Type) {

            case CmResourceTypeMemory:

                bar = NULL;


				if (foundRegs && !foundSRAM/* &&
					desc->u.Memory.Length == 0x1000*/) {

					SRAMBasePA = desc->u.Memory.Start;
					SRAMLength = desc->u.Memory.Length;
					foundSRAM = TRUE;
					bar = "BAR2";
#if (DBG != 0)
					TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
						" get BAR2 ");
#endif
				}

                if (!foundRegs/* &&
                    desc->u.Memory.Length == 0x1000*/) {

                    regsBasePA = desc->u.Memory.Start;
                    regsLength = desc->u.Memory.Length;
                    foundRegs = TRUE;
                    bar = "BAR0";
#if (DBG != 0)
					TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
						" get BAR0 ");
#endif
                }
#if (DBG != 0)
                TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                            " - Memory Resource [%I64X-%I64X] %s",
                            desc->u.Memory.Start.QuadPart,
                            desc->u.Memory.Start.QuadPart +
                            desc->u.Memory.Length,
                            (bar) ? bar : "<unrecognized>" );
#endif
                break;

            case CmResourceTypePort:
#if (DBG != 0)
                TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                            " - Port   Resource [%08I64X-%08I64X] %s",
                            desc->u.Port.Start.QuadPart,
                            desc->u.Port.Start.QuadPart +
                            desc->u.Port.Length,
                            (bar) ? bar : "<unrecognized>" );
#endif
                break;

            default:
                //
                // Ignore all other descriptors
                //
                break;
        }
    }

    if (!(foundRegs/* && foundSRAM*/)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "HSACMapResources: Missing resources");
#endif
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    //
    // Map in the Registers Memory resource: BAR0
    //
    DevExt->RegsBase = (PUCHAR) MmMapIoSpace( regsBasePA,
                                              regsLength,
                                              MmNonCached );

    if (!DevExt->RegsBase) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    " - Unable to map Registers memory %08I64X, length %d",
                    regsBasePA.QuadPart, regsLength);
#endif
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    DevExt->RegsLength = regsLength;
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                " - Registers %p, length %d",
                DevExt->RegsBase, DevExt->RegsLength );
#endif
    //
    // Set separated pointer to HSAC_REGS structure.
    //
    DevExt->Regs = (PHSAC_REGS) DevExt->RegsBase;

	if (foundSRAM)
	{
		//
		// Map in the SRAM Memory Space resource: BAR2
		//
		DevExt->SRAMBase = (PUCHAR) MmMapIoSpace( SRAMBasePA,
			SRAMLength,
			MmNonCached );

		if (!DevExt->SRAMBase) {
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
				" - Unable to map SRAM memory %08I64X, length %d",
				SRAMBasePA.QuadPart,  SRAMLength);
#endif
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		DevExt->SRAMLength = SRAMLength;

#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			" - SRAM      %p, length %d",
			DevExt->SRAMBase, DevExt->SRAMLength );
#endif
	}


    return status;
}

NTSTATUS
HSACInitializeDMA(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Initializes the DMA adapter.

Arguments:

    DevExt      Pointer to our DEVICE_EXTENSION

Return Value:

     None

--*/
{
    NTSTATUS    status;
    WDF_OBJECT_ATTRIBUTES attributes;
	ULONG i = 0;

    PAGED_CODE();

	DevExt->dmaProfile = WdfDmaProfileScatterGather64Duplex;
    //
    // HSAC DMA_TRANSFER_ELEMENTS must be 16-byte aligned
    //
    WdfDeviceSetAlignmentRequirement( DevExt->Device,
                                      HSAC_DTE_ALIGNMENT_16 );

    //
    // Create a new DMA Enabler instance.
    // Use Scatter/Gather, 64-bit Addresses, Duplex-type profile.
    //
    {
        WDF_DMA_ENABLER_CONFIG   dmaConfig;

        WDF_DMA_ENABLER_CONFIG_INIT( &dmaConfig,
                                     WdfDmaProfileScatterGather64Duplex,
                                     DevExt->MaximumTransferLength );
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                    " - The DMA Profile is WdfDmaProfileScatterGather64Duplex");
#endif

        status = WdfDmaEnablerCreate( DevExt->Device,
                                      &dmaConfig,
                                      WDF_NO_OBJECT_ATTRIBUTES,
                                      &DevExt->DmaEnabler );

        if (!NT_SUCCESS (status)) {
#if (DBG != 0)
            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                        "WdfDmaEnablerCreate failed: %!STATUS!", status);
#endif
            return status;
        }
    }

    //
    // Allocate common buffer for building writes
    //
    // NOTE: This common buffer will not be cached.
    //       Perhaps in some future revision, cached option could
    //       be used. This would have faster access, but requires
    //       flushing before starting the DMA in HSACStartWriteDma.
    //
#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	DevExt->writeCommonBufferNum = HSAC_TRANSFER_BUFFER_NUM;
	DevExt->WriteCommonBufferSize = DevExt->MaximumTransferLength;
	//sizeof(DMA_TRANSFER_ELEMENT) * DevExt->WriteTransferElements;

	for (i = 0; i < DevExt->writeCommonBufferNum; i++)
	{
		status = WdfCommonBufferCreate( DevExt->DmaEnabler,
			DevExt->WriteCommonBufferSize,
			WDF_NO_OBJECT_ATTRIBUTES,
			&DevExt->pWriteCommonBuffer[i] );

		if (!NT_SUCCESS(status)) {
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
				"WdfCommonBufferCreate (write) failed: %!STATUS!", status);
#endif
			return status;
		}

		DevExt->pWriteCommonBufferBase[i] =
			WdfCommonBufferGetAlignedVirtualAddress(DevExt->pWriteCommonBuffer[i]);

		DevExt->pWriteCommonBufferBaseLA[i] =
			WdfCommonBufferGetAlignedLogicalAddress(DevExt->pWriteCommonBuffer[i]);

		RtlZeroMemory( DevExt->pWriteCommonBufferBase[i],
			DevExt->WriteCommonBufferSize);
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			"WriteCommonBuffer[%d] 0x%p  (#0x%I64X), length %I64d",
			i,
			DevExt->pWriteCommonBufferBase[i],
			DevExt->pWriteCommonBufferBaseLA[i].QuadPart,
			WdfCommonBufferGetLength(DevExt->pWriteCommonBuffer[i]) );
#endif
	}


#else 
#if (CACHE_MODE == PING_PANG)
    DevExt->WriteCommonBufferSize = DevExt->MaximumTransferLength * HSAC_TRANSFER_BUFFER_NUM;
        //sizeof(DMA_TRANSFER_ELEMENT) * DevExt->WriteTransferElements;
#else
	DevExt->WriteCommonBufferSize = DevExt->MaximumTransferLength;
#endif
    status = WdfCommonBufferCreate( DevExt->DmaEnabler,
                                    DevExt->WriteCommonBufferSize,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    &DevExt->WriteCommonBuffer );

    if (!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfCommonBufferCreate (write) failed: %!STATUS!", status);
#endif
        return status;
    }

    DevExt->WriteCommonBufferBase =
        WdfCommonBufferGetAlignedVirtualAddress(DevExt->WriteCommonBuffer);

    DevExt->WriteCommonBufferBaseLA =
        WdfCommonBufferGetAlignedLogicalAddress(DevExt->WriteCommonBuffer);

    RtlZeroMemory( DevExt->WriteCommonBufferBase, DevExt->WriteCommonBufferSize);

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "WriteCommonBuffer 0x%p  (#0x%I64X), length %I64d",
                DevExt->WriteCommonBufferBase,
                DevExt->WriteCommonBufferBaseLA.QuadPart,
                WdfCommonBufferGetLength(DevExt->WriteCommonBuffer) );
#endif

#endif
    //
    // Allocate common buffer for building reads
    //
    // NOTE: This common buffer will not be cached.
    //       Perhaps in some future revision, cached option could
    //       be used. This would have faster access, but requires
    //       flushing before starting the DMA in HSACStartReadDma.
    //
#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	DevExt->readCommonBufferNum = HSAC_TRANSFER_BUFFER_NUM;
	DevExt->ReadCommonBufferSize = DevExt->MaximumTransferLength;
	//sizeof(DMA_TRANSFER_ELEMENT) * DevExt->ReadTransferElements;

	for (i = 0; i < DevExt->readCommonBufferNum; i++)
	{
		status = WdfCommonBufferCreate( DevExt->DmaEnabler,
			DevExt->ReadCommonBufferSize,
			WDF_NO_OBJECT_ATTRIBUTES,
			&DevExt->pReadCommonBuffer[i] );

		if (!NT_SUCCESS(status)) {
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
				"WdfCommonBufferCreate (read) failed %!STATUS!", status);
#endif
			return status;
		}

		DevExt->pReadCommonBufferBase[i] =
			WdfCommonBufferGetAlignedVirtualAddress(DevExt->pReadCommonBuffer[i]);

		DevExt->pReadCommonBufferBaseLA[i] =
			WdfCommonBufferGetAlignedLogicalAddress(DevExt->pReadCommonBuffer[i]);

		RtlZeroMemory( DevExt->pReadCommonBufferBase[i],
			DevExt->ReadCommonBufferSize);

#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			"ReadCommonBuffer[%d]  0x%p  (#0x%I64X), length %I64d",
			i,
			DevExt->pReadCommonBufferBase[i],
			DevExt->pReadCommonBufferBaseLA[i].QuadPart,
			WdfCommonBufferGetLength(DevExt->pReadCommonBuffer[i]) );
#endif
	}

#else 
#if (CACHE_MODE == PING_PANG)
	DevExt->ReadCommonBufferSize = DevExt->MaximumTransferLength * HSAC_TRANSFER_BUFFER_NUM;
	//sizeof(DMA_TRANSFER_ELEMENT) * DevExt->ReadTransferElements;
#else
	DevExt->ReadCommonBufferSize = DevExt->MaximumTransferLength;
#endif
	status = WdfCommonBufferCreate( DevExt->DmaEnabler,
		DevExt->ReadCommonBufferSize,
		WDF_NO_OBJECT_ATTRIBUTES,
		&DevExt->ReadCommonBuffer );

	if (!NT_SUCCESS(status)) {
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
			"WdfCommonBufferCreate (read) failed %!STATUS!", status);
#endif
		return status;
	}

	DevExt->ReadCommonBufferBase =
		WdfCommonBufferGetAlignedVirtualAddress(DevExt->ReadCommonBuffer);

	DevExt->ReadCommonBufferBaseLA =
		WdfCommonBufferGetAlignedLogicalAddress(DevExt->ReadCommonBuffer);

	RtlZeroMemory( DevExt->ReadCommonBufferBase,
		DevExt->ReadCommonBufferSize);
#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
		"ReadCommonBuffer  0x%p  (#0x%I64X), length %I64d",
		DevExt->ReadCommonBufferBase,
		DevExt->ReadCommonBufferBaseLA.QuadPart,
		WdfCommonBufferGetLength(DevExt->ReadCommonBuffer) );
#endif

#endif

    //
    // Since we are using sequential queue and processing one request
    // at a time, we will create transaction objects upfront and reuse
    // them to do DMA transfer. Transactions objects are parented to
    // DMA enabler object by default. They will be deleted along with
    // along with the DMA enabler object. So need to delete them
    // explicitly.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);
    
    status = WdfDmaTransactionCreate( DevExt->DmaEnabler,
                                      &attributes,
                                      &DevExt->ReadDmaTransaction);

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionCreate(read) failed: %!STATUS!", status);
#endif
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);
    //
    // Create a new DmaTransaction.
    //
    status = WdfDmaTransactionCreate( DevExt->DmaEnabler,
                                      &attributes,
                                      &DevExt->WriteDmaTransaction );

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionCreate(write) failed: %!STATUS!", status);
#endif
        return status;
    }

    return status;
}

NTSTATUS
HSACInitializeDirectDMA(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Initializes the DMA adapter.

Arguments:

    DevExt      Pointer to our DEVICE_EXTENSION

Return Value:

     None

--*/
{
	/*
    NTSTATUS    status;
    WDF_OBJECT_ATTRIBUTES attributes;

    PAGED_CODE();

    //
    // HSAC DMA_TRANSFER_ELEMENTS must be 16-byte aligned
    //
    WdfDeviceSetAlignmentRequirement( DevExt->Device,
                                      HSAC_DTE_ALIGNMENT_16 );

    //
    // Create a new DMA Enabler instance.
    // Use Packet, 64-bit Addresses profile.
    //
    {
        WDF_DMA_ENABLER_CONFIG   dmaConfig;

        WDF_DMA_ENABLER_CONFIG_INIT( &dmaConfig,
                                     WdfDmaProfilePacket64,
                                     DevExt->MaximumTransferLength );
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                    " - The DMA Profile is WdfDmaProfilePacket64");
#endif

        status = WdfDmaEnablerCreate( DevExt->Device,
                                      &dmaConfig,
                                      WDF_NO_OBJECT_ATTRIBUTES,
                                      &DevExt->DirectDmaEnabler );

        if (!NT_SUCCESS (status)) {
#if (DBG != 0)
            TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                        "WdfDmaEnablerCreate failed: %!STATUS!", status);
#endif
            return status;
        }
    }

    //
    // Allocate common buffer for building writes
    //
    // NOTE: This common buffer will not be cached.
    //       Perhaps in some future revision, cached option could
    //       be used. This would have faster access, but requires
    //       flushing before starting the DMA in HSACStartWriteDma.
    //
    DevExt->DirectDmaWriteCommonBufferSize = DevExt->MaximumTransferLength;

    status = WdfCommonBufferCreate( DevExt->DirectDmaEnabler,
                                    DevExt->DirectDmaWriteCommonBufferSize,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    &DevExt->DirectDmaWriteCommonBuffer );

    if (!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfCommonBufferCreate (write) failed: %!STATUS!", status);
#endif
        return status;
    }


    DevExt->DirectDmaWriteCommonBufferBase =
        WdfCommonBufferGetAlignedVirtualAddress(DevExt->DirectDmaWriteCommonBuffer);

    DevExt->DirectDmaWriteCommonBufferBaseLA =
        WdfCommonBufferGetAlignedLogicalAddress(DevExt->DirectDmaWriteCommonBuffer);

    RtlZeroMemory( DevExt->DirectDmaWriteCommonBufferBase,
                   DevExt->DirectDmaWriteCommonBufferSize);
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "WriteCommonBuffer 0x%p  (#0x%I64X), length %I64d",
                DevExt->DirectDmaWriteCommonBufferBase,
                DevExt->DirectDmaWriteCommonBufferBaseLA.QuadPart,
                WdfCommonBufferGetLength(DevExt->DirectDmaWriteCommonBuffer) );
#endif
    //
    // Allocate common buffer for building reads
    //
    // NOTE: This common buffer will not be cached.
    //       Perhaps in some future revision, cached option could
    //       be used. This would have faster access, but requires
    //       flushing before starting the DMA in HSACStartReadDma.
    //
    DevExt->DirectDmaReadCommonBufferSize = DevExt->MaximumTransferLength;

    status = WdfCommonBufferCreate( DevExt->DirectDmaEnabler,
                                    DevExt->DirectDmaReadCommonBufferSize,
                                    WDF_NO_OBJECT_ATTRIBUTES,
                                    &DevExt->DirectDmaReadCommonBuffer );

    if (!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
                    "WdfCommonBufferCreate (read) failed %!STATUS!", status);
#endif
        return status;
    }

    DevExt->ReadCommonBufferBase =
        WdfCommonBufferGetAlignedVirtualAddress(DevExt->DirectDmaReadCommonBuffer);

    DevExt->ReadCommonBufferBaseLA =
        WdfCommonBufferGetAlignedLogicalAddress(DevExt->DirectDmaReadCommonBuffer);

    RtlZeroMemory( DevExt->DirectDmaReadCommonBufferBase,
                   DevExt->DirectDmaReadCommonBufferSize);
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
                "ReadCommonBuffer  0x%p  (#0x%I64X), length %I64d",
                DevExt->DirectDmaReadCommonBufferBase,
                DevExt->DirectDmaReadCommonBufferBaseLA.QuadPart,
                WdfCommonBufferGetLength(DevExt->DirectDmaReadCommonBuffer) );
#endif
    //
    // Since we are using sequential queue and processing one request
    // at a time, we will create transaction objects upfront and reuse
    // them to do DMA transfer. Transactions objects are parented to
    // DMA enabler object by default. They will be deleted along with
    // along with the DMA enabler object. So need to delete them
    // explicitly.
    //
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);
    
    status = WdfDmaTransactionCreate( DevExt->DirectDmaEnabler,
                                      &attributes,
                                      &DevExt->DirectDmaReadTransaction);

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionCreate(read) failed: %!STATUS!", status);
#endif
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, TRANSACTION_CONTEXT);
    //
    // Create a new DmaTransaction.
    //
    status = WdfDmaTransactionCreate( DevExt->DirectDmaEnabler,
                                      &attributes,
                                      &DevExt->DirectDmaWriteTransaction );

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionCreate(write) failed: %!STATUS!", status);
#endif
        return status;
    }

    return status;
	*/
}

NTSTATUS
HSACInitWrite(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Initialize write data structures

Arguments:

    DevExt     Pointer to Device Extension

Return Value:

    None

--*/
{
    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> HSACInitWrite");

    //
    // Make sure the Dma0 DAC (Dual Address Cycle) register is set to 0.
    //
    //WRITE_REGISTER_ULONG( (PULONG) &DevExt->Regs->Dma0_PCI_DAC, 0 );

    //
    // Clear the saved copy of the DMA Channel 0's CSR
    //
    //DevExt->Dma0Csr.uchar = 0;


    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- HSACInitWrite");

    return STATUS_SUCCESS;
}

NTSTATUS
HSACInitRead(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Initialize read data structures

Arguments:

    DevExt     Pointer to Device Extension

Return Value:

--*/
{
    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> HSACInitRead");

    //
    // Make sure the DMA Chan 1 DAC (Dual Address Cycle) is set to 0.
    //
    //WRITE_REGISTER_ULONG( (PULONG) &DevExt->Regs->Dma1_PCI_DAC, 0 );

    //
    // Clear the saved copy of the DMA Channel 1's CSR
    //
    //DevExt->Dma1Csr.uchar = 0;


    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- HSACInitRead");

    return STATUS_SUCCESS;
}

VOID
HSACShutdown(
    IN PDEVICE_EXTENSION DevExt
    )
/*++

Routine Description:

    Reset the device to put the device in a known initial state.
    This is called from D0Exit when the device is torn down or
    when the system is shutdown. Note that Wdf has already
    called out EvtDisable callback to disable the interrupt.

Arguments:

    DevExt -  Pointer to our adapter

Return Value:

    None

--*/
{
    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "---> HSACShutdown");

    //
    // WdfInterrupt is already disabled so issue a full reset
    //
    if (DevExt->Regs) {

        HSACHardwareReset(DevExt);
    }

    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<--- HSACShutdown");
}

VOID
HSACHardwareReset(
    IN PDEVICE_EXTENSION DevExt
    )
/*++
Routine Description:

    Called by D0Exit when the device is being disabled or when the system is shutdown to
    put the device in a known initial state.

Arguments:

    DevExt     Pointer to Device Extension

Return Value:

--*/
{
//    LARGE_INTEGER delay;

    //union {
    //    EEPROM_CSR  bits;
    //    ULONG       ulong;
    //} eeCSR;

    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> HSACIssueFullReset");

    ////
    //// Drive the HSAC into soft reset.
    ////
    //eeCSR.ulong =
    //    READ_REGISTER_ULONG( (PULONG) &DevExt->Regs->EEPROM_Ctrl_Stat );

    //eeCSR.bits.SoftwareReset = TRUE;

    //WRITE_REGISTER_ULONG( (PULONG) &DevExt->Regs->EEPROM_Ctrl_Stat,
    //                      eeCSR.ulong );

    ////
    //// Wait 100 msec.
    ////
    //delay.QuadPart =  WDF_REL_TIMEOUT_IN_MS(100);

    //KeDelayExecutionThread( KernelMode, TRUE, &delay );

    ////
    //// Finally pull the HSAC out of reset.
    ////
    //eeCSR.bits.SoftwareReset = FALSE;

    //WRITE_REGISTER_ULONG( (PULONG) &DevExt->Regs->EEPROM_Ctrl_Stat,
    //                      eeCSR.ulong );

    //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- HSACIssueFullReset");
}

VOID
HSACMapUserAddress(
	IN PDEVICE_EXTENSION DevExt
	)
{
	ULONG i = 0;
	//KIRQL oldIrpl;
//	oldIrpl = KeGetCurrentIrql();
//#if (DBG != 0)
//	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
//		"KeGetCurrentIrql: %d", oldIrpl);
//#endif
//	KeRaiseIrql(DISPATCH_LEVEL, &oldIrpl);
//#if (DBG != 0)
//	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
//		"KeRaiseIrql: %d", oldIrpl);
//#endif
	//采用直接DMA的天然缺陷，用UserMode映射到用户空间使用。
	KeLowerIrql(APC_LEVEL);
	// AllocateMDL

#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	for (i = 0; i < DevExt->readCommonBufferNum; i++)
	{
		DevExt->pReadMDL[i] = IoAllocateMdl(
			DevExt->pReadCommonBufferBase[i], 
			DevExt->ReadCommonBufferSize, 
			FALSE, 
			FALSE, 
			NULL);

		if(DevExt->pReadMDL[i] ==NULL)
		{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
				"HSACMapUserAddress ReadUserAddress Failed");
#endif
			return;
		}

		MmBuildMdlForNonPagedPool(DevExt->pReadMDL[i]);

		__try
		{
			DevExt->pReadUserAddress[i] = MmMapLockedPagesSpecifyCache(
				DevExt->pReadMDL[i],             // MDL for region
				UserMode,            // User or kernel mode?
				MmCached,            // System RAM is always Cached (otherwise mapping fails)
				NULL,              // User address to use
				FALSE,               // Do not issue a bug check (KernelMode only)
				NormalPagePriority     // Priority of success
				);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
				"EXCEPTION: Raised by MmMapLockedPagesSpecifyCache for ReadMDL%d", i);
#endif
			return;
		}

#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			"HSACMapUserAddress ReadUserAddress%d: 0x%I64x", i, DevExt->pReadUserAddress[i]);
#endif
	}

	for (i = 0; i < DevExt->writeCommonBufferNum; i++)
	{
		DevExt->pWriteMDL[i] = IoAllocateMdl(
			DevExt->pWriteCommonBufferBase[i],
			DevExt->WriteCommonBufferSize,
			FALSE,          // Is this a secondary buffer?
			FALSE,          // Charge quota?
			NULL            // No IRP associated with MDL
			);	
		if(DevExt->pWriteMDL[i] ==NULL)
		{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
				"HSACMapUserAddress WriteUserAddress Failed");
#endif
			return;
		}

		MmBuildMdlForNonPagedPool(DevExt->pWriteMDL[i]);

		__try
		{
			DevExt->pWriteUserAddress[i] =
				MmMapLockedPagesSpecifyCache(
				DevExt->pWriteMDL[i],             // MDL for region
				UserMode,            // User or kernel mode?
				MmCached,            // System RAM is always Cached (otherwise mapping fails)
				NULL,              // User address to use
				FALSE,               // Do not issue a bug check (KernelMode only)
				NormalPagePriority     // Priority of success
				);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
				"EXCEPTION: Raised by MmMapLockedPagesSpecifyCache for WriteMDL%d", i);
#endif
			return;
		}

#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			"HSACMapUserAddress WriteUserAddress%d: 0x%I64x", i, DevExt->pWriteUserAddress[i]);
#endif
	}

#else
	DevExt->ReadMDL0 = IoAllocateMdl(
		DevExt->ReadCommonBufferBase, 
		DevExt->ReadCommonBufferSize, 
		FALSE, 
		FALSE, 
		NULL);

	if(DevExt->ReadMDL0 ==NULL)
	{
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			"HSACMapUserAddress ReadUserAddress Failed");
#endif
		return;
	}

	MmBuildMdlForNonPagedPool(DevExt->ReadMDL0);

	__try
	{
		DevExt->ReadUserAddress0 = MmMapLockedPagesSpecifyCache(
			DevExt->ReadMDL0,             // MDL for region
			UserMode,            // User or kernel mode?
			MmCached,            // System RAM is always Cached (otherwise mapping fails)
			NULL,              // User address to use
			FALSE,               // Do not issue a bug check (KernelMode only)
			NormalPagePriority     // Priority of success
			);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
				"EXCEPTION: Raised by MmMapLockedPagesSpecifyCache for ReadMDL0");
#endif
		return;
	}

#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
		"HSACMapUserAddress ReadUserAddress0: 0x%I64x", DevExt->ReadUserAddress0);
#endif

	//// AllocateMDL
	//pdx->ReadMDL1 =
	//	IoAllocateMdl(
	//	pdx->ReadSystemVitualAddress1,
	//	pdx->ReadDMASize,
	//	FALSE,          // Is this a secondary buffer?
	//	FALSE,          // Charge quota?
	//	NULL            // No IRP associated with MDL
	//	);	

	//if(pdx->ReadMDL1 ==NULL)
	//{
	//	DebugPrintToFile(pdx->hFileLog, "Allocate ReadMDL0 Failed\r\n");
	//	return FALSE;
	//}

	//MmBuildMdlForNonPagedPool(pdx->ReadMDL1);
	//__try
	//{
	//	// Attempt to map next region
	//	pdx->ReadUserAddress1 =
	//		MmMapLockedPagesSpecifyCache(
	//		pdx->ReadMDL1,             // MDL for region
	//		UserMode,            // User or kernel mode?
	//		MmCached,            // System RAM is always Cached (otherwise mapping fails)
	//		NULL,              // User address to use
	//		FALSE,               // Do not issue a bug check (KernelMode only)
	//		NormalPagePriority     // Priority of success
	//		);
	//}
	//__except (EXCEPTION_EXECUTE_HANDLER)
	//{
	//	DebugPrintToFile(pdx->hFileLog, "EXCEPTION: Raised by MmMapLockedPagesSpecifyCache() for ReadMDL1\r\n");
	//	return FALSE;
	//}

	// AllocateMDL
	DevExt->WriteMDL0 = IoAllocateMdl(
		DevExt->WriteCommonBufferBase,
		DevExt->WriteCommonBufferSize,
		FALSE,          // Is this a secondary buffer?
		FALSE,          // Charge quota?
		NULL            // No IRP associated with MDL
		);	
	if(DevExt->WriteMDL0 ==NULL)
	{
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			"HSACMapUserAddress WriteUserAddress Failed");
#endif
		return;
	}

	MmBuildMdlForNonPagedPool(DevExt->WriteMDL0);

	__try
	{
		DevExt->WriteUserAddress0 =
			MmMapLockedPagesSpecifyCache(
			DevExt->WriteMDL0,             // MDL for region
			UserMode,            // User or kernel mode?
			MmCached,            // System RAM is always Cached (otherwise mapping fails)
			NULL,              // User address to use
			FALSE,               // Do not issue a bug check (KernelMode only)
			NormalPagePriority     // Priority of success
			);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
			"EXCEPTION: Raised by MmMapLockedPagesSpecifyCache for WriteMDL0");
#endif
		return;
	}

#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
		"HSACMapUserAddress WriteUserAddress0: 0x%I64x", DevExt->WriteUserAddress0);
#endif

	//// AllocateMDL
	//pdx->WriteMDL1 =
	//	IoAllocateMdl(
	//	pdx->WriteSystemVitualAddress1,
	//	pdx->WriteDMASize,
	//	FALSE,          // Is this a secondary buffer?
	//	FALSE,          // Charge quota?
	//	NULL            // No IRP associated with MDL
	//	);	

	//if(pdx->WriteMDL1 ==NULL)
	//{
	//	DebugPrintToFile(pdx->hFileLog, "Allocate ReadMDL0 Failed\r\n");
	//	return FALSE;
	//}

	//MmBuildMdlForNonPagedPool(pdx->WriteMDL1);
	//__try
	//{
	//	// Attempt to map next region
	//	pdx->WriteUserAddress1 =
	//		MmMapLockedPagesSpecifyCache(
	//		pdx->WriteMDL1,             // MDL for region
	//		UserMode,            // User or kernel mode?
	//		MmCached,            // System RAM is always Cached (otherwise mapping fails)
	//		NULL,              // User address to use
	//		FALSE,               // Do not issue a bug check (KernelMode only)
	//		NormalPagePriority     // Priority of success
	//		);
	//}
	//__except (EXCEPTION_EXECUTE_HANDLER)
	//{
	//	DebugPrintToFile(pdx->hFileLog, "EXCEPTION: Raised by MmMapLockedPagesSpecifyCache() for WriteMDL1\r\n");
	//	return FALSE;
	//}

#endif

}

VOID
HSACUnmapUserAddress(
	IN PDEVICE_EXTENSION DevExt
	)
{
	ULONG i = 0;
#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP,
		"HSACUnmapUserAddress");
#endif

#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	for (i = 0; i < DevExt->readCommonBufferNum; i++)
	{
		if(DevExt->pReadUserAddress[i] != NULL)
		{
			MmUnmapLockedPages(DevExt->pReadUserAddress[i], DevExt->pReadMDL[i]);
			DevExt->pReadUserAddress[i] = NULL;
		}

		if(DevExt->pReadMDL[i] != NULL)
		{
			IoFreeMdl(DevExt->pReadMDL[i]);
			DevExt->pReadMDL[i] = NULL;
		}
	}

	for (i = 0; i < DevExt->writeCommonBufferNum; i++)
	{
		if(DevExt->pWriteUserAddress[i] != NULL)
		{
			MmUnmapLockedPages(DevExt->pWriteUserAddress[i], DevExt->pWriteMDL[i]);
			DevExt->pWriteUserAddress[i] = NULL;
		}

		if(DevExt->pWriteMDL[i] != NULL)
		{
			IoFreeMdl(DevExt->pWriteMDL[i]);
			DevExt->pWriteMDL[i] = NULL;
		}
	}
#else
	if(DevExt->ReadUserAddress0 != NULL)
	{
		MmUnmapLockedPages(DevExt->ReadUserAddress0,DevExt->ReadMDL0);
		DevExt->ReadUserAddress0 = NULL;
	}

	if(DevExt->ReadMDL0 != NULL)
	{
		IoFreeMdl(DevExt->ReadMDL0);
		DevExt->ReadMDL0 = NULL;
	}

	//if(pdx->ReadUserAddress1 != NULL)
	//{
	//	MmUnmapLockedPages(pdx->ReadUserAddress1,pdx->ReadMDL1);
	//	pdx->ReadUserAddress1 = NULL;
	//}

	//if(pdx->ReadMDL1 != NULL)
	//{
	//	IoFreeMdl(pdx->ReadMDL1);
	//	pdx->ReadMDL1 = NULL;
	//}

	if(DevExt->WriteUserAddress0 != NULL)
	{
		MmUnmapLockedPages(DevExt->WriteUserAddress0,DevExt->WriteMDL0);
		DevExt->WriteUserAddress0 = NULL;
	}

	if(DevExt->WriteMDL0 != NULL)
	{
		IoFreeMdl(DevExt->WriteMDL0);
		DevExt->WriteMDL0 = NULL;
	}

	//if(pdx->WriteUserAddress1 != NULL)
	//{
	//	MmUnmapLockedPages(pdx->WriteUserAddress1,pdx->WriteMDL1);
	//	pdx->WriteUserAddress1 = NULL;
	//}

	//if(pdx->WriteMDL1 != NULL)
	//{
	//	IoFreeMdl(pdx->WriteMDL1);
	//	pdx->WriteMDL1 = NULL;
	//}
#endif
}



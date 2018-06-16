/*++

Copyright (c) ESSS.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Write.c

Abstract:


Environment:

    Kernel mode

--*/

#include "precomp.h"

#include "Write.tmh"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
VOID
HSACEvtIoWrite(
    IN WDFQUEUE         Queue,
    IN WDFREQUEST       Request,
    IN size_t            Length
    )
/*++

Routine Description:

    Called by the framework as soon as it receives a write request.
    If the device is not ready, fail the request.
    Otherwise get scatter-gather list for this request and send the
    packet to the hardware for DMA.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    Length - Length of the IO operation
                 The default property of the queue is to not dispatch
                 zero length read & write requests to the driver and
                 complete is with status success. So we will never get
                 a zero length request.

Return Value:

--*/
{
    NTSTATUS          status = STATUS_UNSUCCESSFUL;
    PDEVICE_EXTENSION devExt = NULL;

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "--> HSACEvtIoWrite: Request %p", Request);
#endif
    //
    // Get the DevExt from the Queue handle
    //
    devExt = HSACGetDeviceContext(WdfIoQueueGetDevice(Queue));

    //
    // Validate the Length parameter.
    //
    if (Length > HSAC_PHY_BUF_SIZE)  {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto CleanUp;
    }

	if (devExt->dmaProfile == WdfDmaProfilePacket64)
	{
		PVOID					pInputBuffer = NULL;
		size_t                  InputBufferlength = 0;
		status = WdfRequestRetrieveInputBuffer(Request, 0, &pInputBuffer, &InputBufferlength);
//		if( !NT_SUCCESS(status)) {
//#if (DBG != 0)
//			TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
//				"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
//#endif
//		}
		//RtlCopyMemory(devExt->WriteCommonBufferBase, pInputBuffer, InputBufferlength);
		devExt->WriteSize = *(PULONG)pInputBuffer;

#if (CACHE_MODE != CACHE_NONE_MODE)
		devExt->WriteBufIndex = *((PULONG)pInputBuffer + 1);
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
			"--> devExt->WriteBufIndex: %d, devExt->WriteSize: %d", devExt->WriteBufIndex, devExt->WriteSize);
#endif

#else
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
			"--> HSACEvtIoWrite: WriteSize %d", devExt->WriteSize);
#endif

#endif

	}

#ifdef ENABLE_CANCEL
	// Mark the request is cancelable
	WdfRequestMarkCancelable(Request, HSACEvtRequestCancelWrite);
#endif
    //
    // Following code illustrates two different ways of initializing a DMA
    // transaction object. If ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION is
    // defined in the sources file, the first section will be used.
    //
#ifdef ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION

    //
    // This section illustrates how to create and initialize
    // a DmaTransaction using a WDF Request.
    // This type of coding pattern would probably be the most commonly used
    // for handling client Requests.
    //
    status = WdfDmaTransactionInitializeUsingRequest(
                                           devExt->WriteDmaTransaction,
                                           Request,
                                           HSACEvtProgramWriteDma,
                                           WdfDmaDirectionWriteToDevice );

    if(!NT_SUCCESS(status)) {
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionInitializeUsingRequest failed: "
                    "%!STATUS!", status);
#endif
        goto CleanUp;
    }
#else
    //
    // This section illustrates how to create and initialize
    // a DmaTransaction via direct parameters (e.g. not using a WDF Request).
    // This type of coding pattern might be used for driver-initiated DMA
    // operations (e.g. DMA operations not based on driver client requests.)
    //
    // NOTE: This example unpacks the WDF Request in order to get a set of
    //       parameters for the call to WdfDmaTransactionInitialize. While
    //       this is completely legitimate, the representative usage pattern
    //       for WdfDmaTransactionIniitalize would have the driver create/
    //       initialized a DmaTransactin without a WDF Request. A simple
    //       example might be where the driver needs to DMA the device's
    //       firmware to it during device initialization. There would be
    //       no WDF Request; the driver would supply the parameters for
    //       WdfDmaTransactionInitialize directly.
    //
    {
        PTRANSACTION_CONTEXT  transContext;
        PMDL                  mdl;
        PVOID                 virtualAddress;
        ULONG                 length;

        //
        // Initialize this new DmaTransaction with direct parameters.
        //
        status = WdfRequestRetrieveInputWdmMdl(Request, &mdl);
        if (!NT_SUCCESS(status)) {
#if (DBG != 0)
            TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                        "WdfRequestRetrieveInputWdmMdl failed: %!STATUS!", status);
#endif
            goto CleanUp;
        }

        virtualAddress = MmGetMdlVirtualAddress(mdl);
        length = MmGetMdlByteCount(mdl);

        status = WdfDmaTransactionInitialize( devExt->WriteDmaTransaction,
                                              HSACEvtProgramWriteDma,
                                              WdfDmaDirectionWriteToDevice,
                                              mdl,
                                              virtualAddress,
                                              length );

        if(!NT_SUCCESS(status)) {
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                        "WdfDmaTransactionInitialize failed: %!STATUS!", status);
#endif
              goto CleanUp;
        }

        //
        // Retreive this DmaTransaction's context ptr (aka TRANSACTION_CONTEXT)
        // and fill it in with info.
        //
        transContext = HSACGetTransactionContext( devExt->WriteDmaTransaction );
        transContext->Request = Request;
    }
#endif

#if 0 //FYI
        //
        // Modify the MaximumLength for this DmaTransaction only.
        //
        // Note: The new length must be less than or equal to that set when
        //       the DmaEnabler was created.
        //
        {
            ULONG length =  devExt->MaximumTransferLength / 2;

            //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
            //            "Setting a new MaxLen %d", length);

            WdfDmaTransactionSetMaximumLength( devExt->WriteDmaTransaction, length );
        }
#endif

    //
    // Execute this DmaTransaction transaction.
    //
		
    status = WdfDmaTransactionExecute( devExt->WriteDmaTransaction, 
                                       WDF_NO_CONTEXT);

    if(!NT_SUCCESS(status)) {

        //
        // Couldn't execute this DmaTransaction, so fail Request.
        //
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "WdfDmaTransactionExecute failed: %!STATUS!", status);
#endif
        goto CleanUp;
    }

    //
    // Indicate that Dma transaction has been started successfully. The request
    // will be complete by the Dpc routine when the DMA transaction completes.
    //
    status = STATUS_SUCCESS;

CleanUp:

    //
    // If there are errors, then clean up and complete the Request.
    //
    if (!NT_SUCCESS(status)) {
        WdfDmaTransactionRelease(devExt->WriteDmaTransaction);        
        WdfRequestComplete(Request, status);
    }
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "<-- HSACEvtIoWrite: %!STATUS!", status);
#endif

    return;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
BOOLEAN
HSACEvtProgramWriteDma(
    IN  WDFDMATRANSACTION       Transaction,
    IN  WDFDEVICE               Device,
    IN  PVOID                   Context,
    IN  WDF_DMA_DIRECTION       Direction,
    IN  PSCATTER_GATHER_LIST    SgList
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    PDEVICE_EXTENSION        devExt;
    size_t                   offset;
    PDMA_TRANSFER_ELEMENT    dteVA;
 //   ULONG_PTR                dteLALow;
	//ULONG_PTR                dteLAHigh;
	ULONG                dteLALow;
	LONG                 dteLAHigh;
    BOOLEAN                  errors;
    ULONG                    i;

	ULONG transferSize;
	ULONG dma0Ctl;
	// 可以放到驱动对象的上下文中，就不用for循环中计算了
	ULONG sgTransferSize = 0;

    UNREFERENCED_PARAMETER( Context );
    UNREFERENCED_PARAMETER( Direction );
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "--> HSACEvtProgramWriteDma");
#endif
    //
    // Initialize locals
    //
    devExt = HSACGetDeviceContext(Device);
    errors = FALSE;

	if (devExt->dmaProfile == WdfDmaProfilePacket64)
	{
		WdfInterruptAcquireLock( devExt->Interrupt );

#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR32,
			devExt->pWriteCommonBufferBaseLA[devExt->WriteBufIndex].LowPart);
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR64,
			devExt->pWriteCommonBufferBaseLA[devExt->WriteBufIndex].HighPart );
#else
#if (CACHE_MODE == PING_PANG)
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR32,
			devExt->WriteCommonBufferBaseLA.LowPart + devExt->WriteBufIndex * devExt->MaximumTransferLength );
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR64,
			devExt->WriteCommonBufferBaseLA.HighPart );
#else if (CACHE_MODE == CACHE_NONE_MODE)
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR32,
			devExt->WriteCommonBufferBaseLA.LowPart );
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR64,
			devExt->WriteCommonBufferBaseLA.HighPart );
#endif
#endif

		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_SIZE, devExt->WriteSize);

		dma0Ctl = DMA_CTRL_START;
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_CTRL, dma0Ctl);

		WdfInterruptReleaseLock( devExt->Interrupt );

		return TRUE;
	}
    //
    // Get the number of bytes as the offset to the beginning of this
    // Dma operations transfer location in the buffer.
    //
//    offset = WdfDmaTransactionGetBytesTransferred(Transaction);
//#if (DBG != 0)
//	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
//		"offset (%d)\n", offset);
//#endif
    //
    // Setup the pointer to the next DMA_TRANSFER_ELEMENT
    // for both virtual and physical address references.
    //
#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	dteVA = (PDMA_TRANSFER_ELEMENT) devExt->pWriteCommonBufferBase[0];
	dteLALow = (devExt->pWriteCommonBufferBaseLA[0].LowPart +
		sizeof(DMA_TRANSFER_ELEMENT));
	dteLAHigh = devExt->pWriteCommonBufferBaseLA[0].HighPart;
#else
    dteVA = (PDMA_TRANSFER_ELEMENT) devExt->WriteCommonBufferBase;
    dteLALow = (devExt->WriteCommonBufferBaseLA.LowPart +
                        sizeof(DMA_TRANSFER_ELEMENT));
	dteLAHigh = devExt->WriteCommonBufferBaseLA.HighPart;
#endif
    //
    // Translate the System's SCATTER_GATHER_LIST elements
    // into the device's DMA_TRANSFER_ELEMENT elements.
    //
    for (i=0; i < SgList->NumberOfElements; i++) {

        //
        // Construct this DTE.
        //
		dteVA->PageAddressLow  = SgList->Elements[i].Address.LowPart;
		dteVA->PageAddressHigh = SgList->Elements[i].Address.HighPart;
		dteVA->TransferSize    = SgList->Elements[i].Length;

		dteVA->DescPtrLow.LastElement   = FALSE;
		dteVA->DescPtrLow.LowAddress    = DESC_PTR_ADDR( dteLALow );
		dteVA->DescPtrHigh.HighAddress  = dteLAHigh;//0;
		//
		// If at end of SgList, then set LastElement bit in final NTE.
		//

		sgTransferSize += dteVA->TransferSize;

		if (i == SgList->NumberOfElements - 1) {

			dteVA->DescPtrLow.LastElement = TRUE;

#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
				"\tDTE[%d] : Addr #%X%08X, Len %8d,LA #%X%08X, Last(%d)\n",
				i,
				dteVA->PageAddressHigh,
				dteVA->PageAddressLow,
				dteVA->TransferSize,
				dteLAHigh,
				dteLALow,
				dteVA->DescPtrLow.LastElement );
#endif
            break;
        }

#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
			"\tDTE[%d] : Addr #%X%08X, Len %8d,LA #%X%08X, Last(%d)\n",
			i,
			dteVA->PageAddressHigh,
			dteVA->PageAddressLow,
			dteVA->TransferSize,
			dteLAHigh,
			dteLALow,
			dteVA->DescPtrLow.LastElement );
#endif

        //
        // Adjust the next DMA_TRANSFER_ELEMEMT
        //
        dteVA++;
        dteLALow += sizeof(DMA_TRANSFER_ELEMENT);
    }

    //
    // Start the DMA operation.
    // Acquire this device's InterruptSpinLock.
    //
    WdfInterruptAcquireLock( devExt->Interrupt );

    //
    // DMA 0 Descriptor Pointer Register - (DMADPR0)
    // Write the base LOGICAL address of the DMA_TRANSFER_ELEMENT list.
    //
#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR32,
		/*DESC_PTR_ADDR( devExt->WriteCommonBufferBaseLA.LowPart )*/
		devExt->pWriteCommonBufferBaseLA[0].LowPart );
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR64,
		devExt->pWriteCommonBufferBaseLA[0].HighPart );

	transferSize = SgList->NumberOfElements * sizeof(DMA_TRANSFER_ELEMENT);
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_SIZE, sgTransferSize/*transferSize*/);

#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
		"WriteCommonBufferBaseLA: #%X%08X Len %8d\n",
		devExt->pWriteCommonBufferBaseLA[0].HighPart,
		devExt->pWriteCommonBufferBaseLA[0].LowPart,
		transferSize);
#endif

#else
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR32,
                              /*DESC_PTR_ADDR( devExt->WriteCommonBufferBaseLA.LowPart )*/
							  devExt->WriteCommonBufferBaseLA.LowPart );
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_ADDR64,
							  devExt->WriteCommonBufferBaseLA.HighPart );

	transferSize = SgList->NumberOfElements * sizeof(DMA_TRANSFER_ELEMENT);
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_SIZE, sgTransferSize/*transferSize*/);

#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
		"WriteCommonBufferBaseLA: #%X%08X Len %8d\n",
		devExt->WriteCommonBufferBaseLA.HighPart,
		devExt->WriteCommonBufferBaseLA.LowPart,
		transferSize);
#endif
#endif

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "    HSACEvtProgramWriteDma: Start a Write DMA operation total size: %d", sgTransferSize);
#endif
    //
    // DMA 0 CSR Register - (DMACSR0)
    // Start the DMA operation: Set Start bits. Enable Scatter/Gather Mode
    //
	dma0Ctl = DMA_CTRL_START | DMA_CTRL_SG_ENA;
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA0_CTRL, dma0Ctl);

    //
    // Release our interrupt spinlock
    //
    WdfInterruptReleaseLock( devExt->Interrupt );

    //
    // NOTE: This shows how to process errors which occur in the
    //       PFN_WDF_PROGRAM_DMA function in general.
    //       Basically the DmaTransaction must be deleted and
    //       the Request must be completed.
    //
    if (errors) {
        //
        // Must abort the transaction before deleting it.
        //
        NTSTATUS status;

        (VOID) WdfDmaTransactionDmaCompletedFinal(Transaction, 0, &status);
        ASSERT(NT_SUCCESS(status));
        HSACWriteRequestComplete( Transaction, STATUS_INVALID_DEVICE_STATE );
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_WRITE,
                    "<-- HSACEvtProgramWriteDma: error ****");
#endif
        return FALSE;
    }
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
                "<-- HSACEvtProgramWriteDma");
#endif
    return TRUE;
}


VOID
HSACWriteRequestComplete(
    IN WDFDMATRANSACTION  DmaTransaction,
    IN NTSTATUS           Status
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    WDFDEVICE          device;
    WDFREQUEST         request;
    PDEVICE_EXTENSION  devExt;
    size_t             bytesTransferred;

    //
    // Initialize locals
    //
    device  = WdfDmaTransactionGetDevice(DmaTransaction);
    devExt  = HSACGetDeviceContext(device);

#ifdef ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION

    request = WdfDmaTransactionGetRequest(DmaTransaction);

#else
    //
    // If CreateDirect was used then there will be no assoc. Request.
    //
    {
        PTRANSACTION_CONTEXT transContext = HSACGetTransactionContext(DmaTransaction);

        request = transContext->Request;
        transContext->Request = NULL;
        
    }
#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE,
		"UNDEF ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION");
#endif

#endif

#ifdef ENABLE_CANCEL
	if (request == NULL)
	{
		return;
	}
	// We cannot call WdfRequestUnmarkCancelable
	// after a request completes, so check here to see
	// if EchoEvtRequestCancel cleared our saved
	// request handle. 
	Status = WdfRequestUnmarkCancelable(request);
	if (Status == STATUS_CANCELLED)
	{
		return;
	}
#endif
    //
    // Get the final bytes transferred count.
    //
    bytesTransferred =  WdfDmaTransactionGetBytesTransferred( DmaTransaction );
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_DPC,
                "HSACWriteRequestComplete:  Request %p, Status %!STATUS!, "
                "bytes transferred %d\n",
                 request, Status, (int) bytesTransferred );
#endif
    WdfDmaTransactionRelease(DmaTransaction);        

	WdfRequestCompleteWithInformation( request, Status, bytesTransferred);

}

VOID
HSACEvtRequestCancelWrite(
    IN WDFREQUEST Request
    )
/*++

Routine Description:


    Called when an I/O request is cancelled after the driver has marked
    the request cancellable. This callback is automatically synchronized
    with the I/O callbacks since we have chosen to use frameworks Device
    level locking.

Arguments:

    Request - Request being cancelled.

Return Value:

    VOID

--*/
{
	NTSTATUS            status = STATUS_SUCCESS;
	WDFDEVICE			device;
	PDEVICE_EXTENSION   devExt;
    //PQUEUE_CONTEXT queueContext = QueueGetContext(WdfRequestGetIoQueue(Request));
	device = WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request));
	devExt  = HSACGetDeviceContext(device);

	WdfDmaTransactionDmaCompleted( devExt->WriteDmaTransaction, &status );

	WdfDmaTransactionRelease(devExt->WriteDmaTransaction);  

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_WRITE, "HSACEvtRequestCancelWrite called on Request 0x%p\n",  Request);
#endif
    //
    // The following is race free by the callside or DPC side
    // synchronizing completion by calling
    // WdfRequestMarkCancelable(Queue, Request, FALSE) before
    // completion and not calling WdfRequestComplete if the
    // return status == STATUS_CANCELLED.
    //
    WdfRequestCompleteWithInformation(Request, STATUS_CANCELLED, 0L);

    //
    // This book keeping is synchronized by the common
    // Queue presentation lock
    //

    return;
}



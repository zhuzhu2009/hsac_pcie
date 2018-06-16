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

#include "Read.tmh"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
VOID
HSACEvtIoRead(
    IN WDFQUEUE         Queue,
    IN WDFREQUEST       Request,
    IN size_t            Length
    )
/*++

Routine Description:

    Called by the framework as soon as it receives a read request.
    If the device is not ready, fail the request.
    Otherwise get scatter-gather list for this request and send the
    packet to the hardware for DMA.

Arguments:

    Queue      - Default queue handle
    Request    - Handle to the read request
    Length - Length of the data buffer associated with the request.
                     The default property of the queue is to not dispatch
                 zero length read & write requests to the driver and
                 complete is with status success. So we will never get
                 a zero length request.

Return Value:

--*/
{
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    PDEVICE_EXTENSION       devExt;
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
                "--> HSACEvtIoRead: Request %p", Request);
#endif

    //
    // Get the DevExt from the Queue handle
    //
    devExt = HSACGetDeviceContext(WdfIoQueueGetDevice(Queue));

    do {
        //
        // Validate the Length parameter.
        //
        if (Length > HSAC_PHY_BUF_SIZE)  {
            status = STATUS_INVALID_BUFFER_SIZE;
            break;
        }

		if (devExt->dmaProfile == WdfDmaProfilePacket64)
		{
			size_t                  length = 0;
			PVOID					pOutputBuffer = NULL;
			status = WdfRequestRetrieveOutputBuffer(Request, 0, &pOutputBuffer, &length);
//			if( !NT_SUCCESS(status)) {
//#if (DBG != 0)
//				TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
//					"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
//#endif
//			}
			devExt->ReadSize = *(PULONG)pOutputBuffer;

#if (CACHE_MODE != CACHE_NONE_MODE)
			devExt->ReadBufIndex = *((PULONG)pOutputBuffer + 1);
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
				"devExt->ReadBufIndex: %d, devExt->ReadSize: %d\n", devExt->ReadBufIndex, devExt->ReadSize);
#endif
#else
#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
				"devExt->ReadSize: %d\n", devExt->ReadSize);
#endif
#endif

		}

#ifdef ENABLE_CANCEL
		// Mark the request is cancelable
		WdfRequestMarkCancelable(Request, HSACEvtRequestCancelRead);
#endif
        //
        // Initialize this new DmaTransaction.
        //
        status = WdfDmaTransactionInitializeUsingRequest(
                                              devExt->ReadDmaTransaction,
                                              Request,
                                              HSACEvtProgramReadDma,
                                              WdfDmaDirectionReadFromDevice );

        if(!NT_SUCCESS(status)) {
#if (DBG != 0)
            TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
                        "WdfDmaTransactionInitializeUsingRequest "
                        "failed: %!STATUS!", status);
#endif
            break;
        }

#if 0 // FYI
        //
        // Modify the MaximumLength for this DmaTransaction only.
        //
        // Note: The new length must be less than or equal to that set when
        //       the DmaEnabler was created.
        //
        {
            ULONG length =  devExt->MaximumTransferLength / 2;

            //TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
            //            "Setting a new MaxLen %d\n", length);

            WdfDmaTransactionSetMaximumLength( devExt->ReadDmaTransaction, 
                                               length );
        }
#endif

        //
        // Execute this DmaTransaction.
        //
        status = WdfDmaTransactionExecute( devExt->ReadDmaTransaction, 
                                           WDF_NO_CONTEXT);

        if(!NT_SUCCESS(status)) {
            //
            // Couldn't execute this DmaTransaction, so fail Request.
            //
#if (DBG != 0)
            TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
                        "WdfDmaTransactionExecute failed: %!STATUS!", status);
#endif
            break;
        }

        //
        // Indicate that Dma transaction has been started successfully.
        // The request will be complete by the Dpc routine when the DMA
        // transaction completes.
        //
        status = STATUS_SUCCESS;

    } while (0);

    //
    // If there are errors, then clean up and complete the Request.
    //
    if (!NT_SUCCESS(status )) {
        WdfDmaTransactionRelease(devExt->ReadDmaTransaction);
        WdfRequestComplete(Request, status);
    }
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
                "<-- HSACEvtIoRead: status %!STATUS!", status);
#endif

    return;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
BOOLEAN
HSACEvtProgramReadDma(
    IN  WDFDMATRANSACTION       Transaction,
    IN  WDFDEVICE               Device,
    IN  WDFCONTEXT              Context,
    IN  WDF_DMA_DIRECTION       Direction,
    IN  PSCATTER_GATHER_LIST    SgList
    )
/*++

Routine Description:

  The framework calls a driver's EvtProgramDma event callback function
  when the driver calls WdfDmaTransactionExecute and the system has
  enough map registers to do the transfer. The callback function must
  program the hardware to start the transfer. A single transaction
  initiated by calling WdfDmaTransactionExecute may result in multiple
  calls to this function if the buffer is too large and there aren't
  enough map registers to do the whole transfer.


Arguments:

Return Value:

--*/
{
    PDEVICE_EXTENSION        devExt;
    size_t                   offset;
    PDMA_TRANSFER_ELEMENT    dteVA;
	//ULONG_PTR                dteLALow;
	//ULONG_PTR                dteLAHigh;
	ULONG                dteLALow;
	LONG                 dteLAHigh;
    BOOLEAN                  errors;
    ULONG                    i;

	ULONG transferSize;
	ULONG dma1Ctl;
	ULONG sgTransferSize = 0;

    UNREFERENCED_PARAMETER( Context );
    UNREFERENCED_PARAMETER( Direction );

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
                "--> HSACEvtProgramReadDma");
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
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR32,
			devExt->pReadCommonBufferBaseLA[devExt->ReadBufIndex].LowPart );
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR64,
			devExt->pReadCommonBufferBaseLA[devExt->ReadBufIndex].HighPart );
#else
#if (CACHE_MODE == PING_PANG)
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR32,
			devExt->ReadCommonBufferBaseLA.LowPart + devExt->ReadBufIndex * devExt->MaximumTransferLength);
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR64,
			devExt->ReadCommonBufferBaseLA.HighPart );
#else if (CACHE_MODE == CACHE_NONE_MODE)
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR32,
			devExt->ReadCommonBufferBaseLA.LowPart);
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR64,
			devExt->ReadCommonBufferBaseLA.HighPart );
#endif
#endif

		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_SIZE, devExt->ReadSize);
		//devExt->ReadSize = 0;

		dma1Ctl = DMA_CTRL_START;
		WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_CTRL, dma1Ctl);

		//
		// Release our interrupt spinlock
		//
		WdfInterruptReleaseLock( devExt->Interrupt );

		return TRUE;
	}
    //
    // Get the number of bytes as the offset to the beginning of this
    // Dma operations transfer location in the buffer.
    //
//    offset = WdfDmaTransactionGetBytesTransferred(Transaction);
//#if (DBG != 0)
//	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
//		"offset (%d)\n", offset);
//#endif

    //
    // Setup the pointer to the next DMA_TRANSFER_ELEMENT
    // for both virtual and physical address references.
    //
#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	dteVA = (PDMA_TRANSFER_ELEMENT) devExt->pReadCommonBufferBase[0];
	dteLALow = (devExt->pReadCommonBufferBaseLA[0].LowPart +
		sizeof(DMA_TRANSFER_ELEMENT));
	dteLAHigh = devExt->pReadCommonBufferBaseLA[0].HighPart;
#else
    dteVA = (PDMA_TRANSFER_ELEMENT) devExt->ReadCommonBufferBase;
    dteLALow = (devExt->ReadCommonBufferBaseLA.LowPart +
                        sizeof(DMA_TRANSFER_ELEMENT));
	dteLAHigh = devExt->ReadCommonBufferBaseLA.HighPart;
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
		dteVA->DescPtrHigh.HighAddress  = dteLAHigh;
        //
        // If at end of SgList, then set LastElement bit in final NTE.
        //

		sgTransferSize += dteVA->TransferSize;

        if (i == SgList->NumberOfElements - 1) {

            dteVA->DescPtrLow.LastElement = TRUE;

#if (DBG != 0)
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
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
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
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
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR32,
		/*DESC_PTR_ADDR( devExt->WriteCommonBufferBaseLA.LowPart )*/
		devExt->pReadCommonBufferBaseLA[0].LowPart);
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR64,
		devExt->pReadCommonBufferBaseLA[0].HighPart );

	transferSize = SgList->NumberOfElements * sizeof(DMA_TRANSFER_ELEMENT);
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_SIZE, sgTransferSize/*transferSize*/);

#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
		"ReadCommonBufferBaseLA: #%X%08X Len %8d\n",
		devExt->pReadCommonBufferBaseLA[0].HighPart,
		devExt->pReadCommonBufferBaseLA[0].LowPart,
		transferSize);
#endif
#else
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR32,
		/*DESC_PTR_ADDR( devExt->WriteCommonBufferBaseLA.LowPart )*/
		devExt->ReadCommonBufferBaseLA.LowPart);
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_ADDR64,
		devExt->ReadCommonBufferBaseLA.HighPart );

	transferSize = SgList->NumberOfElements * sizeof(DMA_TRANSFER_ELEMENT);
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_SIZE, sgTransferSize/*transferSize*/);

#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
		"ReadCommonBufferBaseLA: #%X%08X Len %8d\n",
		devExt->ReadCommonBufferBaseLA.HighPart,
		devExt->ReadCommonBufferBaseLA.LowPart,
		transferSize);
#endif

#endif

#if (DBG != 0)
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
		"    HSACEvtProgramReadDma: Start a Read DMA operation, total size: %d", sgTransferSize);
#endif
	//
	// DMA 0 CSR Register - (DMACSR0)
	// Start the DMA operation: Set Start bits. Enable Scatter/Gather Mode
	//
	dma1Ctl = DMA_CTRL_START | DMA_CTRL_SG_ENA;
	WRITE_REGISTER_ULONG( (PULONG) &devExt->Regs->DMA1_CTRL, dma1Ctl);

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
        NTSTATUS status;

        //
        // Must abort the transaction before deleting.
        //
        (VOID) WdfDmaTransactionDmaCompletedFinal(Transaction, 0, &status);
        ASSERT(NT_SUCCESS(status));

        HSACReadRequestComplete( Transaction, Device, STATUS_INVALID_DEVICE_STATE );
#if (DBG != 0)
        TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
                    "<-- HSACEvtProgramReadDma: errors ****");
#endif
        return FALSE;
    }
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
                "<-- HSACEvtProgramReadDma");
#endif

    return TRUE;
}

VOID
HSACReadRequestComplete(
    IN WDFDMATRANSACTION  DmaTransaction,
	IN WDFDEVICE          Device,
    IN NTSTATUS           Status
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    WDFREQUEST         request;
    size_t             bytesTransferred;
	PDEVICE_EXTENSION   devExt;
	size_t                  length = 0;
	NTSTATUS                status = STATUS_SUCCESS;
	PVOID					pOutputBuffer = NULL;
    //
    // Get the associated request from the transaction.
    //
    request = WdfDmaTransactionGetRequest(DmaTransaction);
	devExt  = HSACGetDeviceContext(Device);

#ifdef ENABLE_CANCEL
	if (request == NULL)
	{
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
			"HSACReadRequestComplete requst = NULL\n");
#endif
		return;
	}
	// We cannot call WdfRequestUnmarkCancelable
	// after a request completes, so check here to see
	// if EchoEvtRequestCancel cleared our saved
	// request handle. 
	Status = WdfRequestUnmarkCancelable(request);
	if (Status == STATUS_CANCELLED)
	{
#if (DBG != 0)
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
			"WdfRequestUnmarkCancelable Status == STATUS_CANCELLED\n");
#endif
		return;
	}
#endif
    //ASSERT(request);

    //
    // Get the final bytes transferred count.
    //
    bytesTransferred =  WdfDmaTransactionGetBytesTransferred( DmaTransaction );
#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
                "HSACReadRequestComplete:  Request %p, Status %!STATUS!, "
                "bytes transferred %d\n",
                 request, Status, (int) bytesTransferred );
#endif

    WdfDmaTransactionRelease(DmaTransaction);

//	if (devExt->dmaProfile == WdfDmaProfilePacket64)
//	{
//		status = WdfRequestRetrieveOutputBuffer(request, 0, &pOutputBuffer, &length);
//		if( !NT_SUCCESS(status)) {
//#if (DBG != 0)
//			TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
//				"WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
//#endif
//		}
//		//RtlCopyMemory(pOutputBuffer, devExt->ReadCommonBufferBase, bytesTransferred);
//	}
    //
    // Complete this Request.
    //

	WdfRequestCompleteWithInformation( request, Status, bytesTransferred);
}

VOID
HSACEvtRequestCancelRead(
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

	WdfDmaTransactionDmaCompleted( devExt->ReadDmaTransaction, &status );

	WdfDmaTransactionRelease(devExt->ReadDmaTransaction);  

#if (DBG != 0)
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ,
		"HSACEvtRequestCancelRead called on Request 0x%p\n",  Request);
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
    //ASSERT(queueContext->CurrentRequest == Request);
    //queueContext->CurrentRequest = NULL;

    return;
}



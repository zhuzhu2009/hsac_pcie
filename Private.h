/*++

Copyright (c) ESSS.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    Private.h

Abstract:

Environment:

    Kernel mode

--*/


#if !defined(_HSAC_H_)
#define _HSAC_H_

//#define ENABLE_PING_PANG
#define PING_PANG				0x00
#define PING_PANG_DISCRETE		0x10
#define MULTI_CACHE				0x01
#define MULTI_DISCRETE_CACHE	0x11
#define CACHE_NONE_MODE			0xff
#define CACHE_MODE				MULTI_DISCRETE_CACHE //CACHE_NONE_MODE //


#define ENABLE_CANCEL
//
// The device extension for the device object
//
typedef struct _DEVICE_EXTENSION {

    WDFDEVICE               Device;

	PVOID lockDataHandle;

    // Following fields are specific to the hardware
    // Configuration

    // HW Resources
    PHSAC_REGS				Regs;             // Registers address
    PUCHAR                  RegsBase;         // Registers base address
    ULONG                   RegsLength;       // Registers base length

    PUCHAR                  PortBase;         // Port base address
    ULONG                   PortLength;       // Port base length

    PUCHAR                  SRAMBase;         // SRAM base address
    ULONG                   SRAMLength;       // SRAM base length

    WDFINTERRUPT            Interrupt;     // Returned by InterruptCreate

	union {
		INT_REG bits;
		ULONG ul;
	}IntStatus;

	union {
		DMA_CTRL bits;
		ULONG ul;
	}dma0;

	union {
		DMA_CTRL bits;
		ULONG ul;
	}dma1;

	WDF_DMA_PROFILE			dmaProfile;

#if (CACHE_MODE != CACHE_NONE_MODE)
	ULONG					ReadBufIndex;
	ULONG					WriteBufIndex;
#endif

	ULONG					ReadSize;
	ULONG					WriteSize;
    // DmaEnabler
    WDFDMAENABLER           DmaEnabler;
    ULONG                   MaximumTransferLength;

    // Write
    WDFQUEUE                WriteQueue;
    WDFDMATRANSACTION       WriteDmaTransaction;
    ULONG                   WriteTransferElements;
    size_t                  WriteCommonBufferSize;
#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	ULONG					writeCommonBufferNum;
	WDFCOMMONBUFFER         pWriteCommonBuffer[HSAC_TRANSFER_BUFFER_NUM];
	PVOID                   pWriteCommonBufferBase[HSAC_TRANSFER_BUFFER_NUM];
	PHYSICAL_ADDRESS        pWriteCommonBufferBaseLA[HSAC_TRANSFER_BUFFER_NUM];  // Logical Address
	PVOID					pWriteUserAddress[HSAC_TRANSFER_BUFFER_NUM];
	PMDL					pWriteMDL[HSAC_TRANSFER_BUFFER_NUM];
#else
	WDFCOMMONBUFFER         WriteCommonBuffer;
	PVOID                   WriteCommonBufferBase;
	PHYSICAL_ADDRESS        WriteCommonBufferBaseLA;  // Logical Address
	PVOID					WriteUserAddress0;
	//PVOID					WriteUserAddress1;
	PMDL					WriteMDL0;
	//PMDL					WriteMDL1;
#endif

    // Read
    WDFQUEUE                ReadQueue;   
    WDFDMATRANSACTION       ReadDmaTransaction;
	ULONG                   ReadTransferElements;
	size_t                  ReadCommonBufferSize;
#if (CACHE_MODE == MULTI_DISCRETE_CACHE)
	ULONG					readCommonBufferNum;
	WDFCOMMONBUFFER         pReadCommonBuffer[HSAC_TRANSFER_BUFFER_NUM];
	PVOID                   pReadCommonBufferBase[HSAC_TRANSFER_BUFFER_NUM];
	PHYSICAL_ADDRESS        pReadCommonBufferBaseLA[HSAC_TRANSFER_BUFFER_NUM];  // Logical Address
	PVOID					pReadUserAddress[HSAC_TRANSFER_BUFFER_NUM];
	PMDL					pReadMDL[HSAC_TRANSFER_BUFFER_NUM];
#else
	WDFCOMMONBUFFER         ReadCommonBuffer;
	PVOID                   ReadCommonBufferBase;
	PHYSICAL_ADDRESS        ReadCommonBufferBaseLA;   // Logical Address
	PVOID					ReadUserAddress0;
	PVOID					ReadUserAddress1;
	PMDL					ReadMDL0;
	PMDL					ReadMDL1;
#endif

	ULONG					MapFlag;
	// Device Control
	WDFQUEUE				DeviceControlQueue;


	//// Direct DmaEnabler
	//WDFDMAENABLER           DirectDmaEnabler;

	//WDFDMATRANSACTION       DirectDmaWriteTransaction;

	//WDFCOMMONBUFFER         DirectDmaWriteCommonBuffer;
	//size_t                  DirectDmaWriteCommonBufferSize;
	//PUCHAR                  DirectDmaWriteCommonBufferBase;
	//PHYSICAL_ADDRESS        DirectDmaWriteCommonBufferBaseLA;  // Logical Address

	//WDFCOMMONBUFFER         DirectDmaReadCommonBuffer;
	//size_t                  DirectDmaReadCommonBufferSize;
	//PUCHAR                  DirectDmaReadCommonBufferBase;
	//PHYSICAL_ADDRESS        DirectDmaReadCommonBufferBaseLA;   // Logical Address

	//WDFDMATRANSACTION       DirectDmaReadTransaction;

    //ULONG                   HwErrCount;

}  DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// This will generate the function named HSACGetDeviceContext to be use for
// retrieving the DEVICE_EXTENSION pointer.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, HSACGetDeviceContext)

#if !defined(ASSOC_WRITE_REQUEST_WITH_DMA_TRANSACTION)
//
// The context structure used with WdfDmaTransactionCreate
//
typedef struct TRANSACTION_CONTEXT {

    WDFREQUEST     Request;

} TRANSACTION_CONTEXT, * PTRANSACTION_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(TRANSACTION_CONTEXT, HSACGetTransactionContext)

#endif

//
// Function prototypes
//
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD HSACEvtDeviceAdd;

EVT_WDF_OBJECT_CONTEXT_CLEANUP HSACEvtDriverContextCleanup;

EVT_WDF_DEVICE_D0_ENTRY HSACEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT HSACEvtDeviceD0Exit;
EVT_WDF_DEVICE_PREPARE_HARDWARE HSACEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE HSACEvtDeviceReleaseHardware;

EVT_WDF_IO_QUEUE_IO_READ HSACEvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE HSACEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL HSACEvtIoDeviceControl;

EVT_WDF_REQUEST_CANCEL HSACEvtRequestCancelRead;
EVT_WDF_REQUEST_CANCEL HSACEvtRequestCancelWrite;

EVT_WDF_INTERRUPT_ISR HSACEvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC HSACEvtInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE HSACEvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE HSACEvtInterruptDisable;

NTSTATUS
HSACSetIdleAndWakeSettings(
    IN PDEVICE_EXTENSION FdoData
    );

NTSTATUS
HSACInitializeDeviceExtension(
    IN PDEVICE_EXTENSION DevExt
    );

NTSTATUS
HSACPrepareHardware(
    IN PDEVICE_EXTENSION DevExt,
    IN WDFCMRESLIST     ResourcesTranslated
    );

NTSTATUS
HSACInitRead(
    IN PDEVICE_EXTENSION DevExt
    );

NTSTATUS
HSACInitWrite(
    IN PDEVICE_EXTENSION DevExt
    );

//
// WDFINTERRUPT Support
//
NTSTATUS
HSACInterruptCreate(
    IN PDEVICE_EXTENSION DevExt
    );

VOID
HSACReadRequestComplete(
    IN WDFDMATRANSACTION  DmaTransaction,
	IN WDFDEVICE          Device,
    IN NTSTATUS           Status
    );

VOID
HSACWriteRequestComplete(
    IN WDFDMATRANSACTION  DmaTransaction,
    IN NTSTATUS           Status
    );

NTSTATUS
HSACInitializeHardware(
    IN PDEVICE_EXTENSION DevExt
    );

VOID
HSACShutdown(
    IN PDEVICE_EXTENSION DevExt
    );

EVT_WDF_PROGRAM_DMA HSACEvtProgramReadDma;
EVT_WDF_PROGRAM_DMA HSACEvtProgramWriteDma;

VOID
HSACHardwareReset(
    IN PDEVICE_EXTENSION    DevExt
    );

NTSTATUS
HSACInitializeDMA(
    IN PDEVICE_EXTENSION DevExt
    );

NTSTATUS
HSACInitializeDirectDMA(
	IN PDEVICE_EXTENSION DevExt
	);

VOID
HSACMapUserAddress(
	IN PDEVICE_EXTENSION DevExt
	);

VOID
HSACUnmapUserAddress(
	IN PDEVICE_EXTENSION DevExt
	);
#pragma warning(disable:4127) // avoid conditional expression is constant error with W4

#endif  // _HSAC_H_



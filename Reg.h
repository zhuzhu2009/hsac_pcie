#ifndef REG_H
#define REG_H

//*****************************************************************************
//  
//  File Name: Reg.h
// 
//  Description:  This file defines all the HSAC Registers.
// 
//  NOTE: These definitions are for memory-mapped register access only.
//
//*****************************************************************************

//-----------------------------------------------------------------------------   
// PCI Device/Vendor Ids.
//-----------------------------------------------------------------------------   
#define HSAC_PCI_VENDOR_ID               0x9408
#define HSAC_PCI_DEVICE_ID               0x2801
                                        
//-----------------------------------------------------------------------------   
// Expected size of the HSAC on-board DDR
//-----------------------------------------------------------------------------   
#define HSAC_PHY_BUF_SIZE                  (1024*1024*1024)

//-----------------------------------------------------------------------------   
// Maximum DMA transfer size (in bytes).
//
// NOTE: This value is rather arbitrary for this drive, 
//       but must be between [0 - HSAC_DDR_SIZE] in value.
//       You can play with the value to see how requests are sequenced as a
//       set of one or more DMA operations.
//-----------------------------------------------------------------------------   
#define HSAC_MAXIMUM_TRANSFER_LENGTH     (8*1024*1024)//(8*1024) 

// ping-pang operation
#define HSAC_TRANSFER_BUFFER_NUM	64
//-----------------------------------------------------------------------------   
// The DMA_TRANSFER_ELEMENTS (the HSAC's hardware scatter/gather list element)
// must be aligned on a 16-byte boundary.  This is because the lower 4 bits of
// the DESC_PTR.Address contain bit fields not included in the "next" address.
//-----------------------------------------------------------------------------   
#define HSAC_DTE_ALIGNMENT_16      FILE_OCTA_ALIGNMENT 

//-----------------------------------------------------------------------------   
// Number of DMA channels supported by HSAC Chip
//-----------------------------------------------------------------------------   
#define HSAC_DMA_CHANNELS               (2) 

//-----------------------------------------------------------------------------   
// DMA Transfer Element (DTE)
//-----------------------------------------------------------------------------   
/*
   Offset                 Field                                                Description

	00h              Page address [31:3]			        Indicates start address of memory page that must be 
															aligned on an 8- byte boundary (bits 2:0 must be "000") for the 64-bit data path, or a 16-
															byte boundary for the 128-bit data path:
	04h              Page address [63:32]					• If a page is located in the 32-bit addressing space, then bits [63..32]
															must be set to 0.
															• If a page is located in the 64-bit addressing space, then a full 64-bit address must be
															initialized. 
															Note that a page must not cross a 4GB address boundary.
															

	08h              Page Size			                    Indicates the size of the memory page in units of 
															bytes. This value must be a multiple of 8 bytes .

	0Ch              Next descriptor pointer [31:2] &		Specifies the address of the next page descriptor, which must be aligned on a 4-byte boundary (bits
						end of chain bit					1:0 must be 00).
															Setting bit 0 to 1 indicates that the current descriptor is the last descriptor in the chain and 
	10h              Next descriptor pointer [63:32]		the DMA engine will not attempt to fetch other descriptors.	
*/
typedef struct _DESC_PTR_LOW_ { 

    unsigned int     LastElement	: 1  ;  // TRUE - last NTE in chain
    unsigned int     Reserved		: 1  ;  // TRUE - Desc in PCI (host) memory
    unsigned int     LowAddress     : 30 ;

} DESC_PTR_LOW;

typedef struct _DESC_PTR_HIGH_ { 

	unsigned int     HighAddress;

} DESC_PTR_HIGH;

typedef struct _DMA_TRANSFER_ELEMENT {

    unsigned int       PageAddressLow  ;
    unsigned int       PageAddressHigh ;
    unsigned int       TransferSize    ;
    DESC_PTR_LOW       DescPtrLow      ;
	DESC_PTR_HIGH	   DescPtrHigh	   ;

} DMA_TRANSFER_ELEMENT, * PDMA_TRANSFER_ELEMENT;

#define DESC_PTR_ADDR_SHIFT (2)
#define DESC_PTR_ADDR(a) (((ULONG) a) >> DESC_PTR_ADDR_SHIFT)

//-----------------------------------------------------------------------------   
// Define the DMA Control Register (CR)
//-----------------------------------------------------------------------------   
typedef struct _DMA_CTRL_ {

	unsigned int  INTEnable					: 1 ;  // demo bit 0
	unsigned int  Reserved0					: 1 ;  // bit  1-0 //demo bit 1
	unsigned int  Start						: 1 ;  // bit 2 //Run bit
	unsigned int  Abort						: 1 ;  // bit 3
	unsigned int  SGEnable					: 1 ;  // bit 4
	unsigned int  Reserved2					: 3 ;  // bit 7-5
	unsigned int  ChannelState				: 4 ;  // bit 11-8
	unsigned int  Reserved3					: 20;  // bit 31-12

} DMA_CTRL;
//Registers definition (masks)
enum
{
	DMA_CTRL_DMA_ENA   = 0x0001,  // Interrupt enable
	DMA_CTRL_DEMO_ENA  = 0x0002, // DMA demo mode enable
	DMA_CTRL_START     = 0x0004, // DMA START
	DMA_CTRL_ABORT     = 0x0008, // DMA ABORT
	DMA_CTRL_SG_ENA    = 0x0010, // Scatter Gather Enable
	DMA_CTRL_CH_STATUS = 0x0F00 // DMA CHANNEL STATUS
};// DMA_CTRL_REG_MASK;
// DMA channel state encoding
enum
{
	CHAN_SUCCESS		= 0x0000,
	CHAN_STOPPED		= 0x0001,
	CHAN_CPL_TIMEOUT	= 0x0010,
	CHAN_CPL_UR			= 0x0011,
	CHAN_CPL_CA			= 0x0100,
	CHAN_CPL_CRS		= 0x0101,
	CHAN_BUSY			= 0x1000,
	CHAN_REQUESTING		= 0x1001,
	CHAN_WAIT_CPL		= 0x1010,
	CHAN_WAIT_DATA		= 0x1011
};

typedef struct _INT_REG_ {

	unsigned int DMA0IntState		: 1 ;
	unsigned int DMA1IntState		: 1 ;
	unsigned int Reserved			: 30;

} INT_REG;
//interrupt reg state
enum
{
	DMA0IntActive	= 0x01,
	DMA1IntActive	= 0x02
};
//-----------------------------------------------------------------------------   
// HSAC_REGS structure
//-----------------------------------------------------------------------------   
typedef struct _HSAC_REGS_ {

	unsigned int      DMA0_ADDR32			;  // 0x000 
	unsigned int      DMA0_ADDR64			;  // 0x004 
	unsigned int      DMA0_SIZE				;  // 0x008 
	DMA_CTRL		  DMA0_CTRL				;  // 0x00C 
	unsigned int      DMA1_ADDR32			;  // 0x010 
	unsigned int      DMA1_ADDR64			;  // 0x014 
	unsigned int      DMA1_SIZE				;  // 0x018 
	DMA_CTRL	      DMA1_CTRL				;  // 0x01C 
	unsigned int      VERSION_REG			;  // 0x020 
	unsigned int      MAILBOX_REG			;  // 0x024
	unsigned int	  FLASH_CTRL_REG0		;  // 0x028
	unsigned int	  FLASH_CTRL_REG1		;  // 0x02C
	unsigned int	  RETRAIN_CNT			;  // 0x020
	INT_REG			  INT_STATE				;  // 0x034 
	unsigned int	  ID					;  // 0x038
	unsigned int	  CONTROL_STATUS		;  // 0x03C
} HSAC_REGS, * PHSAC_REGS; 


#endif  // __HSAC_H_


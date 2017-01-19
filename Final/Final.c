/*++

Module Name:

    Final.c

Abstract:

    This is the main module of the Final miniFilter driver.

Environment:

    Kernel mode

--*/

#include "common.h"
#include "process.h"
#include "callbackRoutines.h"


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
FinalUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );


EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, FinalUnload)
#pragma alloc_text(PAGE,PreCreate)
#pragma alloc_text(PAGE,PostCreate)

#pragma alloc_text(PAGE,PreQueryInfo)
#pragma alloc_text(PAGE,PostQueryInfo)
#pragma alloc_text(PAGE,PreSetInfo)
#pragma alloc_text(PAGE,PreDirCtrl)
#pragma alloc_text(PAGE,PostDirCtrl)


#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

	{
		IRP_MJ_CREATE,
		FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
		PreCreate,
		PostCreate
	},

	{ IRP_MJ_QUERY_INFORMATION, 0, PreQueryInfo, PostQueryInfo },
	{ IRP_MJ_SET_INFORMATION,   0, PreSetInfo,   NULL },
	{ IRP_MJ_DIRECTORY_CONTROL, 0, PreDirCtrl,   PostDirCtrl },



    { IRP_MJ_OPERATION_END }
};


//
// context registration
//

CONST FLT_CONTEXT_REGISTRATION ContextNotifications[] = {

	{
		FLT_STREAM_CONTEXT,
		0,
		CleanupStreamContext,
		STREAM_CONTEXT_SIZE,
		CONTEXT_TAG
	},


	{ FLT_CONTEXT_END }
};



//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

	ContextNotifications,                               //  Context
    Callbacks,                          //  Operation callbacks

    FinalUnload,                           //  MiniFilterUnload

    NULL,                    //  InstanceSetup
    NULL,            //  InstanceQueryTeardown
	NULL,            //  InstanceTeardownStart
    NULL,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("Final!DriverEntry: Entered\n") );

	//
	// initial work here
	//

	InitializeCipherProcList();
	InitializePre2PostContextList();
	File_InitFileFlag();


    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

			goto cleanMem;
        }

		return status;
    }

cleanMem:
	ClearCipherProcList();
	DeletePre2PostContextList();
	File_UninitFileFlag();
	FltUnregisterFilter(gFilterHandle);

    return status;
}

NTSTATUS
FinalUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

   
	KdPrint(("Final!FinalUnload: Entered\n"));

    FltUnregisterFilter( gFilterHandle );

	//
	// wait for 5s until all irp finished
	//
	
	LARGE_INTEGER interval;
	KdPrint(("KeDelayExecutionThread\n"));
	interval.QuadPart = -(1 * 10000000);//5S
	KeDelayExecutionThread(KernelMode, FALSE, &interval);

	//
	// clear the memory
	//

	ClearCipherProcList();
	DeletePre2PostContextList();
	File_UninitFileFlag();


    return STATUS_SUCCESS;
}



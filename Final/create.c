#include "callbackRoutines.h"

FLT_PREOP_CALLBACK_STATUS
PreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



FLT_POSTOP_CALLBACK_STATUS
PostCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	NTSTATUS status;
	PFLT_FILE_NAME_INFORMATION pfNameInfo = NULL;
	BOOLEAN isDir = FALSE;

	PSTREAM_CONTEXT pStreamCtx = NULL;
	LARGE_INTEGER originalOffset = { 0 };
	LARGE_INTEGER FileSize = { 0 };

	PFILE_FLAG pFileFlag = NULL;
	BOOLEAN bEncrypt = FALSE;
	LARGE_INTEGER ByteOffset = { 0 };

	KIRQL OldIrql;
	BOOLEAN bNewCreated = FALSE;

	try
	{
		if (!NT_SUCCESS(Data->IoStatus.Status))
		{
			leave;
		}

		status = FltIsDirectory(Data->Iopb->TargetFileObject, Data->Iopb->TargetInstance, &isDir);
		if (!NT_SUCCESS(status) || isDir)
		{
			leave;
		}

		//
		// Get File Name
		//

		status = FltGetFileNameInformation(Data, 
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &pfNameInfo);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		FltParseFileNameInformation(pfNameInfo);
		if (pfNameInfo->Name.Length == 0)
		{
			leave;
		}
		if (RtlCompareUnicodeString(&pfNameInfo->Name, &pfNameInfo->Volume, TRUE) == 0)
		{
			leave;
		}

		//
		// start filter work
		// here we need to filter the file extension
		//

		UNICODE_STRING ext = { 0 };
		RtlInitUnicodeString(&ext, L"txt");
		if (RtlCompareUnicodeString(&ext, &pfNameInfo->Extension, TRUE) != 0)
		{
			leave;
		}

		Cc_ClearFileCache(FltObjects->FileObject, TRUE, NULL, 0);
	
		//
		// filter the process
		//

		PCHAR procName = GetProcessName();
		if (!IsMonitoredProcess(procName))
		{
			leave;
		}

		status = Ctx_FindOrCreateStreamContext(Data, FltObjects, TRUE, &pStreamCtx, &bNewCreated);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		status = Ctx_UpdateNameInStreamContext(&pfNameInfo->Name, pStreamCtx);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		if (!bNewCreated)
		{
			KdPrint(("    The Stream Context Has already Created, Just Use it."));
			leave;
		}
	
		//
		// check the file content
		//

		File_GetFileSize(Data, FltObjects, &FileSize);
		File_GetFileOffset(Data, FltObjects, &originalOffset);

		pFileFlag = ExAllocatePoolWithTag(NonPagedPool, FILE_FLAG_LEN, FILEFLAG_POOL_TAG);
		if (FileSize.QuadPart == 0 &&
			(Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess&FILE_GENERIC_WRITE))
		{
			status = FltWriteFile(
				FltObjects->Instance,
				FltObjects->FileObject,
				&ByteOffset,
				FILE_FLAG_LEN,
				g_pFileFlag,
				FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET,
				NULL,
				NULL,
				NULL);
			if (NT_SUCCESS(status))
			{
				bEncrypt = TRUE;
			}

		}
		else if (FileSize.QuadPart >= FILE_FLAG_LEN)
		{
			ByteOffset.QuadPart = 0;
			status = FltReadFile(
				FltObjects->Instance,
				FltObjects->FileObject,
				&ByteOffset,
				FILE_FLAG_LEN,
				pFileFlag,
				FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET,
				NULL,
				NULL,
				NULL);
			if (!NT_SUCCESS(status))
			{
				leave;
			}

			bEncrypt = FILE_GUID_LEN == RtlCompareMemory(g_pFileFlag, pFileFlag, FILE_GUID_LEN);
		}
		else
		{
			bEncrypt = FALSE;
		}

		File_SetFileOffset(Data, FltObjects, &originalOffset);

		if (!bEncrypt)
		{
			leave;
		}

		//
		// set the stream context
		//

		SC_LOCK(pStreamCtx, &OldIrql);

		pStreamCtx->bHasFileEncrypted = bEncrypt;
		pStreamCtx->index = g_pFileFlag->FileFlagInfo.index;

		SC_UNLOCK(pStreamCtx, OldIrql);


		
		KdPrint(("    Process Name: %s\n", procName));
		KdPrint(("    File Name: %wZ", &pStreamCtx->uniFileName));
		KdPrint(("    File Size: %d\n", FileSize.QuadPart));




	}
	finally
	{
		if (NULL != pfNameInfo)
			FltReleaseFileNameInformation(pfNameInfo);

		if (NULL != pStreamCtx)
			FltReleaseContext(pStreamCtx);

		if (NULL != pFileFlag)
		{
			ExFreePoolWithTag(pFileFlag, FILEFLAG_POOL_TAG);
		}
	}

	return FLT_POSTOP_FINISHED_PROCESSING;
}

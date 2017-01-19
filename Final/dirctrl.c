#include "callbackRoutines.h"

BOOLEAN S_CheckDirFile(
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in PUNICODE_STRING PathName,
	__in BOOLEAN bVolumePath,
	__in PWCHAR FileName,
	__in ULONG FileNameLength
)
{
	NTSTATUS status;
	WCHAR  wszFileDosFullPath[260] = { 0 };
	PWCHAR pszRelativePathPtr = NULL;
	UNICODE_STRING sFileDosFullPath;
	OBJECT_ATTRIBUTES ob;
	IO_STATUS_BLOCK IoStatus;
	HANDLE hFile = NULL;
	PFILE_OBJECT FileObject = NULL;

	PVOID pFileGUID = NULL;

	BOOLEAN CheckRet = FALSE;
	LARGE_INTEGER FileSize = { 0 };
	BOOLEAN bDirectory = FALSE;

	try {
		RtlCopyMemory(wszFileDosFullPath, PathName->Buffer, PathName->Length);
		pszRelativePathPtr = wszFileDosFullPath;
		pszRelativePathPtr = pszRelativePathPtr + wcslen(pszRelativePathPtr);
		if (!bVolumePath)
			wcscpy(pszRelativePathPtr, L"\\");
		pszRelativePathPtr = pszRelativePathPtr + wcslen(pszRelativePathPtr);
		RtlCopyMemory(pszRelativePathPtr, FileName, FileNameLength);

		RtlInitUnicodeString(&sFileDosFullPath, wszFileDosFullPath);

		// init object attribute
		InitializeObjectAttributes(&ob,
			&sFileDosFullPath,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
			NULL,
			NULL);

		// open file manually
		status = FltCreateFile(FltObjects->Filter,
			FltObjects->Instance,
			&hFile,
			FILE_READ_DATA | FILE_WRITE_DATA,
			&ob,
			&IoStatus,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE,
			NULL,
			0,
			IO_IGNORE_SHARE_ACCESS_CHECK
		);
		if (!NT_SUCCESS(status))
			leave;

		// get fileobject
		status = ObReferenceObjectByHandle(hFile,
			STANDARD_RIGHTS_ALL,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
		);
		if (!NT_SUCCESS(status))
			leave;
		status = File_GetFileStandardInfo(FileObject, FltObjects, NULL, &FileSize, &bDirectory);
		if (!NT_SUCCESS(status))
			leave;
		if (bDirectory || (FileSize.QuadPart < FILE_FLAG_LEN))
			leave;
		pFileGUID = ExAllocatePoolWithTag(NonPagedPool, FILE_GUID_LEN, NAME_TAG);
		status = ReadFileFlag(FileObject, FltObjects, pFileGUID, FILE_GUID_LEN);
		if (!NT_SUCCESS(status))
			leave;

		CheckRet = FILE_GUID_LEN== RtlCompareMemory(g_pFileFlag, pFileGUID, FILE_GUID_LEN);
	}
	finally {
		if (NULL != FileObject)
			ObDereferenceObject(FileObject);
		if (NULL != hFile)
			FltClose(hFile);

		if (NULL != pFileGUID)
			ExFreePool(pFileGUID);
	}

	return CheckRet;
}

NTSTATUS S_DoDirCtrl(
	__in PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__inout PVOID pBuf,
	__in FILE_INFORMATION_CLASS FileInfoClass
)
{
	PFLT_FILE_NAME_INFORMATION pfNameInfo = NULL;
	NTSTATUS status;
	BOOLEAN bVolumePath = FALSE;;

	if ((FileBothDirectoryInformation != FileInfoClass)
		&& (FileDirectoryInformation != FileInfoClass)
		&& (FileFullDirectoryInformation != FileInfoClass))
		return STATUS_SUCCESS;

	try {
		//获取文件全路径
		status = FltGetFileNameInformation(Data,
			FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
			&pfNameInfo);
		if (!NT_SUCCESS(status))
			leave;

		FltParseFileNameInformation(pfNameInfo);
		if (0 == pfNameInfo->Name.Length)
			leave;

		//判断是否卷根目录，如果是，Name最后有'\'，否则，没有
		bVolumePath = sizeof(WCHAR) == pfNameInfo->ParentDir.Length;
		switch (FileInfoClass)
		{
		case FileBothDirectoryInformation:
		{
			PFILE_BOTH_DIR_INFORMATION pbdi = (PFILE_BOTH_DIR_INFORMATION)pBuf;
			do {
				if (S_CheckDirFile(
					FltObjects,
					&pfNameInfo->Name,
					bVolumePath,
					pbdi->FileName,
					pbdi->FileNameLength))
				{
					pbdi->AllocationSize.QuadPart -= FILE_FLAG_LEN;
					pbdi->EndOfFile.QuadPart -= FILE_FLAG_LEN;
				}
				if (!pbdi->NextEntryOffset)
					break;
				pbdi = (PFILE_BOTH_DIR_INFORMATION)((CHAR*)pbdi + pbdi->NextEntryOffset);
			} while (FALSE);

			break;
		}
		case FileDirectoryInformation:
		{
			PFILE_DIRECTORY_INFORMATION pfdi = (PFILE_DIRECTORY_INFORMATION)pBuf;
			do {
				if (S_CheckDirFile(
					FltObjects,
					&pfNameInfo->Name,
					bVolumePath,
					pfdi->FileName,
					pfdi->FileNameLength))
				{
					pfdi->AllocationSize.QuadPart -= FILE_FLAG_LEN;
					pfdi->EndOfFile.QuadPart -= FILE_FLAG_LEN;
				}
				if (!pfdi->NextEntryOffset)
					break;
				pfdi = (PFILE_DIRECTORY_INFORMATION)((CHAR*)pfdi + pfdi->NextEntryOffset);
			} while (FALSE);

			break;
		}
		case FileFullDirectoryInformation:
		{
			PFILE_FULL_DIR_INFORMATION pffdi = (PFILE_FULL_DIR_INFORMATION)pBuf;
			do {
				if (S_CheckDirFile(
					FltObjects,
					&pfNameInfo->Name,
					bVolumePath,
					pffdi->FileName,
					pffdi->FileNameLength))
				{
					pffdi->AllocationSize.QuadPart -= FILE_FLAG_LEN;
					pffdi->EndOfFile.QuadPart -= FILE_FLAG_LEN;
				}
				if (!pffdi->NextEntryOffset)
					break;
				pffdi = (PFILE_BOTH_DIR_INFORMATION)((CHAR*)pffdi + pffdi->NextEntryOffset);
			} while (FALSE);

			break;
		}
		default: break;
		}
	}
	finally {
		if (NULL != pfNameInfo)
			FltReleaseFileNameInformation(pfNameInfo);
	}

	return STATUS_SUCCESS;
}



FLT_PREOP_CALLBACK_STATUS
PreDirCtrl(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	FLT_PREOP_CALLBACK_STATUS FltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	PVOID newBuf = NULL;
	PMDL newMdl = NULL;
	PPRE_2_POST_CONTEXT p2pCtx;
	NTSTATUS status;


	try {

		PCHAR procName = GetProcessName();
		if (!IsMonitoredProcess(procName) && !IsSystemProcess(procName))
		{
			leave;
		}

		//if fast io, forbid it
		if (FLT_IS_FASTIO_OPERATION(Data))
		{
			FltStatus = FLT_PREOP_DISALLOW_FASTIO;
			leave;
		}

		//if not dir query, skip it
		if ((iopb->MinorFunction != IRP_MN_QUERY_DIRECTORY) ||
			(iopb->Parameters.DirectoryControl.QueryDirectory.Length == 0))
			leave;

		newBuf = ExAllocatePoolWithTag(NonPagedPool, iopb->Parameters.DirectoryControl.QueryDirectory.Length, BUFFER_SWAP_TAG);
		if (newBuf == NULL)
			leave;

		if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_IRP_OPERATION))
		{
			newMdl = IoAllocateMdl(newBuf, iopb->Parameters.DirectoryControl.QueryDirectory.Length, FALSE, FALSE, NULL);
			if (newMdl == NULL)
				leave;
			MmBuildMdlForNonPagedPool(newMdl);
		}

		p2pCtx = ExAllocateFromNPagedLookasideList(&Pre2PostContextList);
		if (p2pCtx == NULL)
			leave;

		//Update the buffer pointers and MDL address
		iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer = newBuf;
		iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress = newMdl;
		FltSetCallbackDataDirty(Data);

		//Pass state to our post-operation callback.
		p2pCtx->SwappedBuffer = newBuf;
		*CompletionContext = p2pCtx;

		FltStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
	}
	finally {
		if (FltStatus != FLT_PREOP_SUCCESS_WITH_CALLBACK)
		{
			if (newBuf != NULL)
				ExFreePool(newBuf);

			if (newMdl != NULL)
				IoFreeMdl(newMdl);
		}
	}
	return FltStatus;
}



FLT_POSTOP_CALLBACK_STATUS
PostDirCtrl(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PVOID origBuf;
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	FLT_POSTOP_CALLBACK_STATUS FltStatus = FLT_POSTOP_FINISHED_PROCESSING;
	PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;
	BOOLEAN cleanupAllocatedBuffer = TRUE;

	FILE_INFORMATION_CLASS FileInfoClass = iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass;

	ASSERT(!FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING));

	try {

		if (!NT_SUCCESS(Data->IoStatus.Status) || (Data->IoStatus.Information == 0))
			leave;

		//  We need to copy the read data back into the users buffer.  Note
		//  that the parameters passed in are for the users original buffers
		//  not our swapped buffers
		if (iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress != NULL)
		{
			origBuf = MmGetSystemAddressForMdlSafe(
				iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress,
				NormalPagePriority);
			if (origBuf == NULL)
			{
				Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Data->IoStatus.Information = 0;
				leave;
			}
		}
		else if (FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) ||
			FlagOn(Data->Flags, FLTFL_CALLBACK_DATA_FAST_IO_OPERATION))
		{
			origBuf = iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
		}
		else {
			if (FltDoCompletionProcessingWhenSafe(Data,
				FltObjects,
				CompletionContext,
				Flags,
				PostDirCtrlWhenSafe
				, &FltStatus))
			{
				cleanupAllocatedBuffer = FALSE;
			}
			else {
				Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Data->IoStatus.Information = 0;
			}
			leave;
		}

		//
		//  We either have a system buffer or this is a fastio operation
		//  so we are in the proper context.  Copy the data handling an
		//  exception.
		//
		//  NOTE:  Due to a bug in FASTFAT where it is returning the wrong
		//         length in the information field (it is sort) we are always
		//         going to copy the original buffer length.
		//
		try {
			status = S_DoDirCtrl(Data, FltObjects, p2pCtx->SwappedBuffer, FileInfoClass);
			RtlCopyMemory(origBuf,
				p2pCtx->SwappedBuffer,
				/*Data->IoStatus.Information*/
				iopb->Parameters.DirectoryControl.QueryDirectory.Length);
		} except(EXCEPTION_EXECUTE_HANDLER) {
			Data->IoStatus.Status = GetExceptionCode();
			Data->IoStatus.Information = 0;
		}

	}
	finally {
		//
		//  If we are supposed to, cleanup the allocate memory and release
		//  the volume context.  The freeing of the MDL (if there is one) is
		//  handled by FltMgr.
		//
		if (cleanupAllocatedBuffer)
		{
			ExFreePool(p2pCtx->SwappedBuffer);
			ExFreeToNPagedLookasideList(&Pre2PostContextList, p2pCtx);
		}
	}
	return FltStatus;
}


FLT_POSTOP_CALLBACK_STATUS
PostDirCtrlWhenSafe(
	__inout PFLT_CALLBACK_DATA Data,
	__in PCFLT_RELATED_OBJECTS FltObjects,
	__in PVOID CompletionContext,
	__in FLT_POST_OPERATION_FLAGS Flags
)
{
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PPRE_2_POST_CONTEXT p2pCtx = CompletionContext;
	PVOID origBuf;
	NTSTATUS status;

	FILE_INFORMATION_CLASS  FileInfoClass = iopb->Parameters.DirectoryControl.QueryDirectory.FileInformationClass;

	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(Flags);
	ASSERT(Data->IoStatus.Information != 0);

	status = FltLockUserBuffer(Data);

	if (!NT_SUCCESS(status))
	{
		Data->IoStatus.Status = status;
		Data->IoStatus.Information = 0;
	}
	else {
		origBuf = MmGetSystemAddressForMdlSafe(
			iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress,
			NormalPagePriority);
		if (origBuf == NULL)
		{
			Data->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			Data->IoStatus.Information = 0;
		}
		else {
			status = S_DoDirCtrl(Data, FltObjects, p2pCtx->SwappedBuffer, FileInfoClass);

			RtlCopyMemory(origBuf,
				p2pCtx->SwappedBuffer,
				iopb->Parameters.DirectoryControl.QueryDirectory.Length);
		}
	}

	ExFreePool(p2pCtx->SwappedBuffer);
	ExFreeToNPagedLookasideList(&Pre2PostContextList,
		p2pCtx);
	return FLT_POSTOP_FINISHED_PROCESSING;
}
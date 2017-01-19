#include "callbackRoutines.h"

FLT_PREOP_CALLBACK_STATUS
PreSetInfo(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
	PVOID FileInfoBuffer = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
	FLT_PREOP_CALLBACK_STATUS FltPreStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

	PSTREAM_CONTEXT pStreamCtx = NULL;
	KIRQL OldIrql;

	UNREFERENCED_PARAMETER(CompletionContext);
	PAGED_CODE();

	try
	{
		status = Ctx_FindOrCreateStreamContext(Data, FltObjects, FALSE, &pStreamCtx, NULL);
		if (!NT_SUCCESS(status))
		{
			leave;
		}

		if (!pStreamCtx->bHasFileEncrypted)
		{
			leave;
		}

		if (FLT_IS_FASTIO_OPERATION(Data))
		{
			FltPreStatus = FLT_PREOP_DISALLOW_FASTIO;
			leave;
		}

		PCHAR procName = GetProcessName();
		if (!IsMonitoredProcess(procName) && !IsSystemProcess(procName))
		{
			leave;
		}

		if (!(FileInfoClass == FileAllInformation ||
			FileInfoClass == FileAllocationInformation ||
			FileInfoClass == FileEndOfFileInformation ||
			FileInfoClass == FileStandardInformation ||
			FileInfoClass == FilePositionInformation ||
			FileInfoClass == FileValidDataLengthInformation))
		{
			KdPrint(("SetInfo:%d\n", FileInfoClass));
			leave;
		}

		switch (FileInfoClass)
		{
		case FileAllInformation:
		{
			PFILE_ALL_INFORMATION psFileAllInfo = (PFILE_ALL_INFORMATION)FileInfoBuffer;
			psFileAllInfo->PositionInformation.CurrentByteOffset.QuadPart += FILE_FLAG_LEN;
			psFileAllInfo->StandardInformation.EndOfFile.QuadPart += FILE_FLAG_LEN;
			psFileAllInfo->StandardInformation.AllocationSize.QuadPart += FILE_FLAG_LEN;
			break;
		}
		case FileAllocationInformation:
		{
			PFILE_ALLOCATION_INFORMATION psFileAllocInfo = (PFILE_ALLOCATION_INFORMATION)FileInfoBuffer;
			psFileAllocInfo->AllocationSize.QuadPart += FILE_FLAG_LEN;
			break;
		}
		case FileEndOfFileInformation:
		{// update file size on disk
			PFILE_END_OF_FILE_INFORMATION psFileEndInfo = (PFILE_END_OF_FILE_INFORMATION)FileInfoBuffer;
			psFileEndInfo->EndOfFile.QuadPart += FILE_FLAG_LEN;
			break;
		}
		case FileStandardInformation:
		{
			PFILE_STANDARD_INFORMATION psStandardInfo = (PFILE_STANDARD_INFORMATION)FileInfoBuffer;
			psStandardInfo->EndOfFile.QuadPart += FILE_FLAG_LEN;
			psStandardInfo->AllocationSize.QuadPart += FILE_FLAG_LEN;
			break;
		}
		case FilePositionInformation:
		{
			PFILE_POSITION_INFORMATION psFilePosInfo = (PFILE_POSITION_INFORMATION)FileInfoBuffer;
			psFilePosInfo->CurrentByteOffset.QuadPart += FILE_FLAG_LEN;
			break;
		}
		case FileValidDataLengthInformation:
		{
			PFILE_VALID_DATA_LENGTH_INFORMATION psFileValidInfo = (PFILE_VALID_DATA_LENGTH_INFORMATION)FileInfoBuffer;
			psFileValidInfo->ValidDataLength.QuadPart += FILE_FLAG_LEN;
			break;
		}
		};
	}
	finally 
	{
		if (NULL != pStreamCtx)
			FltReleaseContext(pStreamCtx);
	}

	return FltPreStatus;

}

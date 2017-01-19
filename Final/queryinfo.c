#include "callbackRoutines.h"


FLT_PREOP_CALLBACK_STATUS
PreQueryInfo(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FLT_PREOP_CALLBACK_STATUS retVal = FLT_PREOP_SYNCHRONIZE;
	FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.QueryFileInformation.FileInformationClass;

	PSTREAM_CONTEXT pStreamCtx = NULL;
	KIRQL OldIrql;

	try
	{
		status = Ctx_FindOrCreateStreamContext(Data, FltObjects, FALSE, &pStreamCtx, NULL);
		if (!NT_SUCCESS(status))
		{
			retVal = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (!pStreamCtx->bHasFileEncrypted)
		{
			retVal = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (FLT_IS_FASTIO_OPERATION(Data))
		{
			retVal = FLT_PREOP_DISALLOW_FASTIO;
			leave;
		}

		PCHAR procName = GetProcessName();
		if (!IsMonitoredProcess(procName) && !IsSystemProcess(procName))
		{
			retVal = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (FileInfoClass == FileAllInformation ||
			FileInfoClass == FileAllocationInformation ||
			FileInfoClass == FileEndOfFileInformation ||
			FileInfoClass == FileStandardInformation ||
			FileInfoClass == FilePositionInformation ||
			FileInfoClass == FileValidDataLengthInformation ||
			FileInfoClass == FileNetworkOpenInformation)
		{
			retVal = FLT_PREOP_SUCCESS_WITH_CALLBACK;
		}
		else {
			retVal = FLT_PREOP_SUCCESS_NO_CALLBACK;
		}




	}
	finally
	{
		if (NULL != pStreamCtx)
		{
			FltReleaseContext(pStreamCtx);
		}
	}

	return retVal;

}



FLT_POSTOP_CALLBACK_STATUS
PostQueryInfo(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FLT_POSTOP_CALLBACK_STATUS FltStatus = FLT_POSTOP_FINISHED_PROCESSING;
	FILE_INFORMATION_CLASS FileInfoClass = Data->Iopb->Parameters.QueryFileInformation.FileInformationClass;
	PVOID FileInfoBuffer = Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;
	ULONG FileInfoLength = Data->IoStatus.Information;

	switch (FileInfoClass)
	{
	case FileAllInformation:
	{
		PFILE_ALL_INFORMATION psFileAllInfo = (PFILE_ALL_INFORMATION)FileInfoBuffer;
		// 注意FileAllInformation，即使长度不够，
		// 依然可以返回前面的字节。
		// 需要注意的是返回的字节里是否包含了StandardInformation
		// 这个可能影响文件的大小的信息。
		if (Data->IoStatus.Information >=
			sizeof(FILE_BASIC_INFORMATION) +
			sizeof(FILE_STANDARD_INFORMATION))
		{
			psFileAllInfo->StandardInformation.EndOfFile.QuadPart -= FILE_FLAG_LEN;
			psFileAllInfo->StandardInformation.AllocationSize.QuadPart -= FILE_FLAG_LEN;
			if (Data->IoStatus.Information >=
				sizeof(FILE_BASIC_INFORMATION) +
				sizeof(FILE_STANDARD_INFORMATION) +
				sizeof(FILE_INTERNAL_INFORMATION) +
				sizeof(FILE_EA_INFORMATION) +
				sizeof(FILE_ACCESS_INFORMATION) +
				sizeof(FILE_POSITION_INFORMATION))
			{
				if (psFileAllInfo->PositionInformation.CurrentByteOffset.QuadPart >= FILE_FLAG_LEN)
					psFileAllInfo->PositionInformation.CurrentByteOffset.QuadPart -= FILE_FLAG_LEN;
			}
		}
		break;
	}

	case FileAllocationInformation:
	{
		PFILE_ALLOCATION_INFORMATION psFileAllocInfo = (PFILE_ALLOCATION_INFORMATION)FileInfoBuffer;

		ASSERT(psFileAllocInfo->AllocationSize.QuadPart >= FILE_FLAG_LEN);
		psFileAllocInfo->AllocationSize.QuadPart -= FILE_FLAG_LEN;
		break;
	}
	case FileValidDataLengthInformation:
	{
		PFILE_VALID_DATA_LENGTH_INFORMATION psFileValidLengthInfo = (PFILE_VALID_DATA_LENGTH_INFORMATION)FileInfoBuffer;
		ASSERT(psFileValidLengthInfo->ValidDataLength.QuadPart >= FILE_FLAG_LEN);
		psFileValidLengthInfo->ValidDataLength.QuadPart -= FILE_FLAG_LEN;
		break;
	}
	case FileStandardInformation:
	{
		PFILE_STANDARD_INFORMATION psFileStandardInfo = (PFILE_STANDARD_INFORMATION)FileInfoBuffer;
		//ASSERT(psFileStandardInfo->AllocationSize.QuadPart >= FILE_FLAG_LEN);
		psFileStandardInfo->AllocationSize.QuadPart -= FILE_FLAG_LEN;
		psFileStandardInfo->EndOfFile.QuadPart -= FILE_FLAG_LEN;
		break;
	}
	case FileEndOfFileInformation:
	{
		PFILE_END_OF_FILE_INFORMATION psFileEndInfo = (PFILE_END_OF_FILE_INFORMATION)FileInfoBuffer;
		ASSERT(psFileEndInfo->EndOfFile.QuadPart >= FILE_FLAG_LEN);
		psFileEndInfo->EndOfFile.QuadPart -= FILE_FLAG_LEN;
		break;
	}
	case FilePositionInformation:
	{
		PFILE_POSITION_INFORMATION psFilePosInfo = (PFILE_POSITION_INFORMATION)FileInfoBuffer;
		if (psFilePosInfo->CurrentByteOffset.QuadPart >= FILE_FLAG_LEN)
			psFilePosInfo->CurrentByteOffset.QuadPart -= FILE_FLAG_LEN;
		break;
	}
	case FileNetworkOpenInformation:
	{
		PFILE_NETWORK_OPEN_INFORMATION psFileNetOpenInfo = (PFILE_NETWORK_OPEN_INFORMATION)FileInfoBuffer;
		psFileNetOpenInfo->AllocationSize.QuadPart -= FILE_FLAG_LEN;
		psFileNetOpenInfo->EndOfFile.QuadPart -= FILE_FLAG_LEN;
		break;
	}
	default:
		ASSERT(FALSE);
	};

	return  FltStatus;
}
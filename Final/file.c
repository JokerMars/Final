#include "file.h"

PFILE_FLAG g_pFileFlag;

static UCHAR gc_sGuid[FILE_GUID_LEN] = { \
0x7a, 0x4d, 0x2c, 0xd1, 0x3b, 0x2f, 0x97, 0x9f, \
0x7d, 0x6b, 0x3f, 0x6b, 0x74, 0x9c, 0x7e, 0x7b };



/****************************************************************************/
/*                        Routine definition                               */
/****************************************************************************/

static
NTSTATUS File_ReadWriteFileComplete(
	PDEVICE_OBJECT dev,
	PIRP irp,
	PVOID context
)
{
	*irp->UserIosb = irp->IoStatus;
	KeSetEvent(irp->UserEvent, 0, FALSE);
	IoFreeIrp(irp);
	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
File_ReadWriteFile(
	__in ULONG MajorFunction,
	__in PFLT_INSTANCE Instance,
	__in PFILE_OBJECT FileObject,
	__in PLARGE_INTEGER ByteOffset,
	__in ULONG Length,
	__in PVOID  Buffer,
	__out PULONG BytesReadWrite,
	__in FLT_IO_OPERATION_FLAGS FltFlags
)
{
	ULONG i;
	PIRP irp;
	KEVENT Event;
	PIO_STACK_LOCATION ioStackLocation;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

	PDEVICE_OBJECT pVolumeDevObj = NULL;
	PDEVICE_OBJECT pFileSysDevObj = NULL;
	PDEVICE_OBJECT pNextDevObj = NULL;

	//获取minifilter相邻下层的设备对象
	pVolumeDevObj = IoGetDeviceAttachmentBaseRef(FileObject->DeviceObject);
	if (NULL == pVolumeDevObj)
	{
		return STATUS_UNSUCCESSFUL;
	}

	//共享路径没有这个值，故这里需要判断一下，也就是说共享读取写入目前不支持
	if (NULL == pVolumeDevObj->Vpb)
	{
		return STATUS_UNSUCCESSFUL;
	}

	pFileSysDevObj = pVolumeDevObj->Vpb->DeviceObject;
	pNextDevObj = pFileSysDevObj;

	if (NULL == pNextDevObj)
	{
		ObDereferenceObject(pVolumeDevObj);
		return STATUS_UNSUCCESSFUL;
	}

	//开始构建读写IRP
	KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

	// 分配irp.
	irp = IoAllocateIrp(pNextDevObj->StackSize, FALSE);
	if (irp == NULL) {
		ObDereferenceObject(pVolumeDevObj);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	irp->AssociatedIrp.SystemBuffer = NULL;
	irp->MdlAddress = NULL;
	irp->UserBuffer = Buffer;
	irp->UserEvent = &Event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->RequestorMode = KernelMode;
	if (MajorFunction == IRP_MJ_READ)
		irp->Flags = IRP_DEFER_IO_COMPLETION | IRP_READ_OPERATION | IRP_NOCACHE;
	else if (MajorFunction == IRP_MJ_WRITE)
		irp->Flags = IRP_DEFER_IO_COMPLETION | IRP_WRITE_OPERATION | IRP_NOCACHE;
	else
	{
		ObDereferenceObject(pVolumeDevObj);
		return STATUS_UNSUCCESSFUL;
	}

	if ((FltFlags & FLTFL_IO_OPERATION_PAGING) == FLTFL_IO_OPERATION_PAGING)
	{
		irp->Flags |= IRP_PAGING_IO;
	}

	// 填写irpsp
	ioStackLocation = IoGetNextIrpStackLocation(irp);
	ioStackLocation->MajorFunction = (UCHAR)MajorFunction;
	ioStackLocation->MinorFunction = (UCHAR)IRP_MN_NORMAL;
	ioStackLocation->DeviceObject = pNextDevObj;
	ioStackLocation->FileObject = FileObject;
	if (MajorFunction == IRP_MJ_READ)
	{
		ioStackLocation->Parameters.Read.ByteOffset = *ByteOffset;
		ioStackLocation->Parameters.Read.Length = Length;
	}
	else
	{
		ioStackLocation->Parameters.Write.ByteOffset = *ByteOffset;
		ioStackLocation->Parameters.Write.Length = Length;
	}

	// 设置完成
	IoSetCompletionRoutine(irp, File_ReadWriteFileComplete, 0, TRUE, TRUE, TRUE);
	(void)IoCallDriver(pNextDevObj, irp);
	KeWaitForSingleObject(&Event, Executive, KernelMode, TRUE, 0);
	*BytesReadWrite = IoStatusBlock.Information;

	ObDereferenceObject(pVolumeDevObj);

	return IoStatusBlock.Status;
}

NTSTATUS
File_GetFileOffset(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__out PLARGE_INTEGER FileOffset
)
{
	NTSTATUS status;
	FILE_POSITION_INFORMATION NewPos;

	//修改为向下层Call
	status = FltQueryInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&NewPos,
		sizeof(FILE_POSITION_INFORMATION),
		FilePositionInformation,
		NULL
	);
	if (NT_SUCCESS(status))
	{
		FileOffset->QuadPart = NewPos.CurrentByteOffset.QuadPart;
	}

	return status;
}


NTSTATUS File_SetFileOffset(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__in PLARGE_INTEGER FileOffset
)
{
	NTSTATUS status;
	FILE_POSITION_INFORMATION NewPos;
	//修改为向下层Call
	LARGE_INTEGER NewOffset = { 0 };

	NewOffset.QuadPart = FileOffset->QuadPart;
	NewOffset.LowPart = FileOffset->LowPart;

	NewPos.CurrentByteOffset = NewOffset;

	status = FltSetInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&NewPos,
		sizeof(FILE_POSITION_INFORMATION),
		FilePositionInformation
	);
	return status;
}


NTSTATUS
File_SetFileSize(
	__in PFLT_CALLBACK_DATA Data,
	__in PFLT_RELATED_OBJECTS FltObjects,
	__in PLARGE_INTEGER FileSize
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FILE_END_OF_FILE_INFORMATION EndOfFile;
	PFSRTL_COMMON_FCB_HEADER Fcb = (PFSRTL_COMMON_FCB_HEADER)FltObjects->FileObject->FsContext;;

	EndOfFile.EndOfFile.QuadPart = FileSize->QuadPart;
	EndOfFile.EndOfFile.LowPart = FileSize->LowPart;

	//修改为向下层Call
	status = FltSetInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&EndOfFile,
		sizeof(FILE_END_OF_FILE_INFORMATION),
		FileEndOfFileInformation
	);

	return status;
}

NTSTATUS
File_GetFileSize(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__in PLARGE_INTEGER FileSize
)
{
	NTSTATUS status;
	FILE_STANDARD_INFORMATION fileInfo;

	//修改为向下层Call
	status = FltQueryInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&fileInfo,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation,
		NULL
	);
	if (NT_SUCCESS(status))
	{
		FileSize->QuadPart = fileInfo.EndOfFile.QuadPart;
	}
	else
	{
		FileSize->QuadPart = 0;
	}

	return status;
}


NTSTATUS
File_GetFileStandardInfo(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__in PLARGE_INTEGER FileAllocationSize,
	__in PLARGE_INTEGER FileSize,
	__in PBOOLEAN bDirectory
)
{
	NTSTATUS status = STATUS_SUCCESS;
	FILE_STANDARD_INFORMATION sFileStandardInfo;

	//修改为向下层Call
	status = FltQueryInformationFile(FltObjects->Instance,
		FltObjects->FileObject,
		&sFileStandardInfo,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation,
		NULL
	);
	if (NT_SUCCESS(status))
	{
		if (NULL != FileSize)
			*FileSize = sFileStandardInfo.EndOfFile;
		if (NULL != FileAllocationSize)
			*FileAllocationSize = sFileStandardInfo.AllocationSize;
		if (NULL != bDirectory)
			*bDirectory = sFileStandardInfo.Directory;
	}

	return status;
}

NTSTATUS
File_InitFileFlag()
{
	if (NULL != g_pFileFlag)
		return STATUS_SUCCESS;

	g_pFileFlag = ExAllocatePoolWithTag(NonPagedPool, FILE_FLAG_LEN, FILEFLAG_POOL_TAG);
	if (NULL == g_pFileFlag)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(g_pFileFlag, FILE_FLAG_LEN);

	RtlCopyMemory(g_pFileFlag->FileFlagInfo.mark, gc_sGuid, FILE_GUID_LEN);
	g_pFileFlag->FileFlagInfo.index = 1;

	return STATUS_SUCCESS;
}

NTSTATUS
File_UninitFileFlag()
{
	if (NULL != g_pFileFlag)
	{
		ExFreePoolWithTag(g_pFileFlag, FILEFLAG_POOL_TAG);
		g_pFileFlag = NULL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
File_EncryptBuffer(
	__in PVOID buffer,
	__in ULONG Length,
	__in PCHAR PassWord,
	__inout PUSHORT CryptIndex,
	__in ULONG ByteOffset
)
{
	PCHAR buf;
	UINT64 i;

	UNREFERENCED_PARAMETER(PassWord);

	buf = (PCHAR)buffer;

	if (NULL != CryptIndex)
	{
		if (*CryptIndex == 1)
		{
			for (i = 0; i<Length; i++)
			{
				buf[i] ^= 0x77;
			}
		}
		else {
			//其他加密算法
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS
File_DecryptBuffer(
	__in PVOID buffer,
	__in ULONG Length,
	__in PCHAR PassWord,
	__inout PUSHORT CryptIndex,
	__in ULONG ByteOffset
)
{
	PCHAR buf;
	UINT64 i;

	UNREFERENCED_PARAMETER(PassWord);

	buf = (PCHAR)buffer;

	if (NULL != CryptIndex)
	{
		if (*CryptIndex == 1)
		{
			for (i = 0; i<Length; i++)
			{
				buf[i] ^= 0x77;
			}
		}
		else {
			//其他加密算法
		}
	}

	return STATUS_SUCCESS;
}


NTSTATUS
WriteFileFlag(
	PFILE_OBJECT FileObject,
	PCFLT_RELATED_OBJECTS FltObjects,
	PFILE_FLAG FileFlag
)
{
	LARGE_INTEGER ByteOffset = { 0 };
	ULONG BytesWritten = 0;
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	if (NULL == FileFlag)
		return status;

	//g_psFileFlag为初始化的通用头部，这里可以自己定义一些头部的值
	RtlCopyMemory(FileFlag, g_pFileFlag, FILE_FLAG_LEN);
	FileFlag->FileFlagInfo.index = 1;

	ByteOffset.QuadPart = 0;
	status = File_ReadWriteFile(
		IRP_MJ_WRITE,
		FltObjects->Instance,
		FileObject,
		&ByteOffset,
		FILE_FLAG_LEN,
		FileFlag,
		&BytesWritten,
		FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
	return status;
}

NTSTATUS 
ReadFileFlag(
	PFILE_OBJECT FileObject,
	PCFLT_RELATED_OBJECTS FltObjects,
	PVOID FileFlag,
	ULONG ReadLength
)
{
	LARGE_INTEGER ByteOffset = { 0 };
	ULONG BytesRead = 0;
	NTSTATUS status;

	if (NULL == FileFlag)
		return STATUS_UNSUCCESSFUL;
	ByteOffset.QuadPart = 0;
	status = File_ReadWriteFile(IRP_MJ_READ,
		FltObjects->Instance,
		FileObject,
		&ByteOffset,
		ReadLength,
		FileFlag,
		&BytesRead,
		FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET
	);
	return status;
}


NTSTATUS 
UpdateFileFlag(
	__in PFLT_CALLBACK_DATA Data,
	PFILE_OBJECT FileObject,
	PCFLT_RELATED_OBJECTS FltObjects,
	PVOID WriteBuf,
	ULONG Offset,
	ULONG WriteLen
)
{
	LARGE_INTEGER ByteOffset = { 0 };
	ULONG BytesWritten = 0;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PFILE_FLAG pFileFlag = NULL;
	PCHAR p;

	if (NULL == WriteBuf)
		return status;
	try
	{
		pFileFlag = (PFILE_FLAG)ExAllocatePoolWithTag(NonPagedPool, FILE_FLAG_LEN, FILEFLAG_POOL_TAG);
		status = ReadFileFlag(FileObject, FltObjects, pFileFlag, FILE_FLAG_LEN);
		if (!NT_SUCCESS(status))
			leave;

		p = pFileFlag;
		p += Offset;
		if (RtlCompareMemory(p, WriteBuf, WriteLen) != WriteLen)
		{
			RtlCopyMemory(p, WriteBuf, WriteLen);

			ByteOffset.QuadPart = 0;
			status = File_ReadWriteFile(
				IRP_MJ_WRITE,
				FltObjects->Instance,
				FileObject,
				&ByteOffset,
				FILE_FLAG_LEN,
				pFileFlag,
				&BytesWritten,
				FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET);
		}
		else {
			status = STATUS_SUCCESS;
		}
	}
	finally {
		if (NULL != pFileFlag)
			ExFreePool(pFileFlag);
	}
	return status;
}

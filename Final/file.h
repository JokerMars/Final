#pragma once
#ifndef _FILE_OPERATION_H
#define _FILE_OPERATION_H

#include "common.h"

#define FILE_GUID_LEN		16
#define FILE_FLAG_LEN		PAGE_SIZE
#define FILEFLAG_POOL_TAG	'FASV'
#define NAME_TAG			'ntsv'

#pragma pack(1)

typedef struct _FILE_FLAG_INFO
{
	UCHAR mark[FILE_GUID_LEN];
	UCHAR index;
}FILE_FLAG_INFO, *PFILE_FLAG_INFO;

#define FILE_FLAG_INFO_LEN sizeof(FILE_FLAG_INFO)

typedef struct _FILE_FLAG
{
	FILE_FLAG_INFO FileFlagInfo;

	UCHAR reserved[FILE_FLAG_LEN - FILE_FLAG_INFO_LEN];
}FILE_FLAG, *PFILE_FLAG;


#pragma pack()


extern PFILE_FLAG g_pFileFlag;

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
);

NTSTATUS
File_GetFileOffset(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__out PLARGE_INTEGER FileOffset
);

NTSTATUS
File_SetFileOffset(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__in PLARGE_INTEGER FileOffset
);

NTSTATUS
File_GetFileSize(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__in PLARGE_INTEGER FileSize
);

NTSTATUS
File_GetFileStandardInfo(
	__in  PFLT_CALLBACK_DATA Data,
	__in  PFLT_RELATED_OBJECTS FltObjects,
	__in PLARGE_INTEGER FileAllocationSize,
	__in PLARGE_INTEGER FileSize,
	__in PBOOLEAN bDirectory
);


NTSTATUS
File_InitFileFlag();

NTSTATUS
File_UninitFileFlag();

/**
* 加密缓存内容
*buffer:指向缓存的指针
*Length:缓存长度
*PassWord:密码，目前未使用
*CryptIndex:加密解密算法索引号
*/
NTSTATUS
File_EncryptBuffer(
	__in PVOID buffer,
	__in ULONG Length,
	__in PCHAR PassWord,
	__inout PUSHORT CryptIndex,
	__in ULONG ByteOffset
);

/**
* 解密缓存内容
*buffer:指向缓存的指针
*Length:缓存长度
*PassWord:密码，目前未使用
*CryptIndex:加密解密算法索引号
*/
NTSTATUS
File_DecryptBuffer(
	__in PVOID buffer,
	__in ULONG Length,
	__in PCHAR PassWord,
	__inout PUSHORT CryptIndex,
	__in ULONG ByteOffset
);

NTSTATUS
WriteFileFlag(
	PFILE_OBJECT FileObject,
	PCFLT_RELATED_OBJECTS FltObjects,
	PFILE_FLAG FileFlag
);

NTSTATUS
ReadFileFlag(
	PFILE_OBJECT FileObject,
	PCFLT_RELATED_OBJECTS FltObjects,
	PVOID FileFlag,
	ULONG ReadLength
);

NTSTATUS
UpdateFileFlag(
	__in PFLT_CALLBACK_DATA Data,
	PFILE_OBJECT FileObject,
	PCFLT_RELATED_OBJECTS FltObjects,
	PVOID WriteBuf,
	ULONG Offset,
	ULONG WriteLen
);






#endif
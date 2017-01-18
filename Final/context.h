#pragma once
#ifndef _CONTEXT_FUNC_H
#define _CONTEXT_FUNC_H

#include "common.h"

#define STRING_TAG			'SXSC'
#define RESOURCE_TAG		'rtSC'
#define CONTEXT_TAG			'CTSC'
#define PRE_2_POST_TAG		'PPSC'

typedef struct _STREAM_CONTEXT
{
	//Keep a record of file name
	UNICODE_STRING uniFileName;

	BOOLEAN bHasFileEncrypted;

	UCHAR index;

	PERESOURCE resource;

	KSPIN_LOCK resource1;

}STREAM_CONTEXT, *PSTREAM_CONTEXT;

#define STREAM_CONTEXT_SIZE sizeof(STREAM_CONTEXT)


typedef struct _PRE_2_POST_CONTEXT
{
	PSTREAM_CONTEXT pStreamCtx;

	PVOID SwappedBuffer;
}PRE_2_POST_CONTEXT, *PPRE_2_POST_CONTEXT;

extern NPAGED_LOOKASIDE_LIST Pre2PostContextList;

VOID SC_iLOCK(PERESOURCE pResource);
VOID SC_iUNLOCK(PERESOURCE pResource);

VOID
SC_LOCK(PSTREAM_CONTEXT SC, PKIRQL OldIrql);

VOID
SC_UNLOCK(PSTREAM_CONTEXT SC, KIRQL OldIrql);



NTSTATUS
Ctx_FindOrCreateStreamContext(
	__in PFLT_CALLBACK_DATA Cbd,
	__in PFLT_RELATED_OBJECTS FltObjects,
	__in BOOLEAN CreateIfNotFound,
	__deref_out PSTREAM_CONTEXT *StreamContext,
	__out_opt BOOLEAN* ContextCreated
);

NTSTATUS
Ctx_UpdateNameInStreamContext(
	__in PUNICODE_STRING DirectoryName,
	__inout PSTREAM_CONTEXT StreamContext
);


VOID CleanupStreamContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
);


VOID InitializePre2PostContextList();

VOID DeletePre2PostContextList();


#endif
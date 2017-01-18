#pragma once
#ifndef _GET_PROC_H
#define _GET_PROC_H

#include "common.h"

#define MAX_PROC_LEN 32
#define MAX_EXT_NUM 20

extern LIST_ENTRY CipherProcList;

extern ULONG procNameOffset;

typedef struct _RuleNode
{
	CHAR procName[MAX_PROC_LEN];
	LIST_ENTRY next;
}List_RuleNode, *PList_RuleNode;

BOOLEAN InsertRuleToLinkList(PCHAR str);

VOID Dbg_OutRules(PLIST_ENTRY listHead);

VOID ClearList(PLIST_ENTRY head);

BOOLEAN SearchProcess(PCHAR procName, PLIST_ENTRY head);

BOOLEAN IsMonitoredProcess(PCHAR procName);

BOOLEAN IsSystemProcess(PCHAR procName);

VOID InitializeCipherProcList();

VOID ClearCipherProcList();


ULONG GetProcessNameOffset();

PCHAR GetProcessName();


#endif
// Copyright (C) James Baxter 2017. All Rights Reserved.
// Created by Rob Perren.

#include "NTGame.h"
#include "Public/Asserts.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "StackTracker.h"
#endif //WITH_EDITOR

#if ENABLE_ASSERTS && WITH_EDITOR

const int kiStackThreshold = 4;

FString EditorAssertManager::GetStackTrace(FString& lsOutputFilename)
{
	lsOutputFilename = FString::Printf(TEXT("%sEditorAssertManagerST-%s.csv"), *FPaths::GameLogDir(), *FDateTime::Now().ToString());
	FOutputDeviceFile lOutputFile(*lsOutputFilename);

	FStackTracker lStackTracker = FStackTracker(NULL, NULL, true);
	lStackTracker.CaptureStackTrace(kiStackThreshold, NULL);
	lStackTracker.DumpStackTraces(0, lOutputFile);

	lOutputFile.TearDown();

	FString lsStackTraceOutput;
	FFileHelper::LoadFileToString(lsStackTraceOutput, *lsOutputFilename);

	return lsStackTraceOutput;
}

void VARARGS EditorAssertManager::AssertFailed(const ANSICHAR* lacExpr, const ANSICHAR* lFile, int32 liLine, const TCHAR* lacFormat/*=TEXT("")*/, ...)
{
	TCHAR lacDescriptionString[4096];
	GET_VARARGS(lacDescriptionString, ARRAY_COUNT(lacDescriptionString), ARRAY_COUNT(lacDescriptionString) - 1, lacFormat, lacFormat);

	if (GEngine->IsEditor() && GEditor->PlayWorld != NULL)
	{
		TCHAR lacErrorString[MAX_SPRINTF];
		FCString::Sprintf(lacErrorString, TEXT("Assertion failed: %s"), ANSI_TO_TCHAR(lacExpr));

		FString lsSTFilePath;
		FString lsStackTrace = GetStackTrace(lsSTFilePath);

		FString lStackTraceOutput = "\n--------------------------------\n";;
		lStackTraceOutput += TEXT("\nCall Stack: - ") + lsSTFilePath + "\n";
		lStackTraceOutput += lsStackTrace;

		FString lsFullErrorInfo = FString::Printf(TEXT("Assertion failed: %s [File:%s] [Line: %i] \n%s\n"), lacErrorString, ANSI_TO_TCHAR(lFile), liLine, lacDescriptionString);
		FString lsTitle = lacErrorString;
		FString lsMessage = lsFullErrorInfo + lStackTraceOutput;

		GEditor->OnModalMessageDialog(EAppMsgType::Ok, FText::FromString(lsMessage), FText::FromString(lsTitle));

		GEditor->RequestEndPlayMap();
	}
	else
	{
		FDebug::AssertFailed(lacExpr, lFile, liLine, lacDescriptionString);
	}
}

#endif //defined(ENABLE_ASSERTS) && defined(WITH_EDITOR)
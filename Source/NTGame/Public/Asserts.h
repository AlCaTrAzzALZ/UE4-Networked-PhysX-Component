// Copyright (C) James Baxter 2017. All Rights Reserved.
// Created by Rob Perren.
// Custom assertions that do not trigger a closure of the editor, only the PIE session.

#pragma once

//Should we enable Asserts!
#if DO_CHECK
	#define ENABLE_ASSERTS 1
#endif //DO_CHECK

#if ENABLE_ASSERTS
	#if WITH_EDITOR

class EditorAssertManager
{
public:

	static FString GetStackTrace(FString& lOutputFilename);

	static void VARARGS AssertFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format = TEXT(""), ...);
};

		#define ASSERT(expr)							{ if(!(expr)) { EditorAssertManager::AssertFailed( #expr, __FILE__, __LINE__ ); CA_ASSUME(expr); return; } }
		#define ASSERTV(expr, ...)						{ if(!(expr)) { EditorAssertManager::AssertFailed( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); CA_ASSUME(expr); return; } }

		#define ASSERT_WR(expr, rresult)				{ if(!(expr)) { EditorAssertManager::AssertFailed( #expr, __FILE__, __LINE__ ); CA_ASSUME(expr); return rresult; } }
		#define ASSERTV_WR(expr, rresult, ...)			{ if(!(expr)) { EditorAssertManager::AssertFailed( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); CA_ASSUME(expr); return rresult; } }

	#else //WITH_EDITOR

		#define ASSERT(expr)							check(expr)
		#define ASSERTV(expr, ...)						checkf(expr, ##__VA_ARGS__)
		#define ASSERT_WR(expr, rresult)				check(expr)
		#define ASSERTV_WR(expr, rresult, ...)			checkf(expr, ##__VA_ARGS__)

	#endif //WITH_EDITOR

#else //ENABLE_ASSERTS

	#define ASSERT(...)
	#define ASSERTV(...)
	#define ASSERT_WR(expr, rresult)				
	#define ASSERTV_WR(expr, rresult, ...)			

#endif //ENABLE_ASSERTS
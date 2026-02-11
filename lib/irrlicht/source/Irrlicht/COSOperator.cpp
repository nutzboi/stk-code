// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "COSOperator.h"

#ifdef _IRR_WINDOWS_API_
#ifndef _IRR_XBOX_PLATFORM_
#include <windows.h>
#endif
#else
#include <string.h>
#include <unistd.h>
#if !defined(_IRR_SOLARIS_PLATFORM_) && !defined(__CYGWIN__) && !defined(__HAIKU__)
#include <sys/param.h>
#include <sys/types.h>
#endif
#endif

#include <cassert>

#include "IrrlichtDevice.h"
#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#include "SDL_clipboard.h"
#endif
#ifdef _IRR_COMPILE_WITH_OSX_DEVICE_
#include "MacOSX/OSXClipboard.h"
#endif

namespace irr
{

// constructor  linux
   COSOperator::COSOperator(const core::stringc& osVersion, IrrlichtDevice* /*device*/)
: OperatingSystem(osVersion)
{
}


// constructor
COSOperator::COSOperator(const core::stringc& osVersion) 
: OperatingSystem(osVersion)
{
	#ifdef _DEBUG
	setDebugName("COSOperator");
	#endif
}


//! returns the current operating system version as string.
const core::stringc& COSOperator::getOperatingSystemVersion() const
{
	return OperatingSystem;
}


//! copies text to the clipboard
#if defined(_IRR_COMPILE_WITH_WINDOWS_DEVICE_)
void COSOperator::copyToClipboard(const wchar_t* text) const
{
	if (wcslen(text)==0)
		return;

// Windows version
#if defined(_IRR_XBOX_PLATFORM_)
#elif defined(_IRR_WINDOWS_API_)
	if (!OpenClipboard(NULL) || text == 0)
		return;

	EmptyClipboard();

	HGLOBAL clipbuffer;
	wchar_t * buffer;

	clipbuffer = GlobalAlloc(GMEM_DDESHARE, wcslen(text)*sizeof(wchar_t) + sizeof(wchar_t));
	buffer = (wchar_t*)GlobalLock(clipbuffer);

	wcscpy(buffer, text);

	GlobalUnlock(clipbuffer);
	SetClipboardData(CF_UNICODETEXT, clipbuffer); //Windwos converts between CF_UNICODETEXT and CF_TEXT automatically.
	CloseClipboard();

#else

#endif
}
#else
void COSOperator::copyToClipboard(const c8* text) const
{
	if (strlen(text)==0)
		return;

// Windows version
#if defined(_IRR_XBOX_PLATFORM_)
#elif defined(_IRR_WINDOWS_API_)
	if (!OpenClipboard(NULL) || text == 0)
		return;

	EmptyClipboard();

	HGLOBAL clipbuffer;

	int widelen = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, text, -1, nullptr, 0);
	if(widelen > 0)
	{
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, widelen*sizeof(wchar_t));
		wchar_t* buffer = (wchar_t*)GlobalLock(clipbuffer);
		MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, text, -1, buffer, widelen);
		SetClipboardData(CF_UNICODETEXT, clipbuffer);
	}
	else
	{	// if MultiByteToWideChar fails, fallback to the old behaviour.
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(text)+1);
		char* buffer = (char*)GlobalLock(clipbuffer);
		strcpy(buffer, text);
		SetClipboardData(CF_TEXT, clipbuffer);
	}

	GlobalUnlock(clipbuffer);
	CloseClipboard();

// MacOSX version
#elif defined(_IRR_COMPILE_WITH_OSX_DEVICE_)

	OSXCopyToClipboard(text);
	
#elif defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	SDL_SetClipboardText(text);
#endif
}
#endif


//! gets text from the clipboard
//! \return Returns 0 if no string is in there.
#if defined(_IRR_COMPILE_WITH_WINDOWS_DEVICE_)
const wchar_t* COSOperator::getTextFromClipboard() const
{
#if defined(_IRR_XBOX_PLATFORM_)
		return 0;
#elif defined(_IRR_WINDOWS_API_)
	if (!OpenClipboard(NULL))
		return 0;

	wchar_t * widebuffer = 0;
	char buffer[1401];

	HANDLE hData = GetClipboardData( CF_UNICODETEXT ); //Windwos converts between CF_UNICODETEXT and CF_TEXT automatically.
	widebuffer = (wchar_t*)GlobalLock( hData );
	
	int widelen = WideCharToMultiByte(CP_UTF8, WC_DEFAULTCHAR, widebuffer, -1, nullptr, 0, NULL, NULL);
	if(widelen > 0)
	{
		widelen = (widelen < 360 ? widelen : 360); // limit to max user message size.
		WideCharToMultiByte(CP_UTF8, WC_DEFAULTCHAR, widebuffer, -1, buffer, widelen, NULL, NULL);
		buffer[1400] = '\0';
	}
	char * result = buffer;

	GlobalUnlock( hData );
	CloseClipboard();
	return result;

#else

	return 0;
#endif
}
#else
const c8* COSOperator::getTextFromClipboard() const
{
#if defined(_IRR_XBOX_PLATFORM_)
		return 0;
#elif defined(_IRR_WINDOWS_API_)
	if (!OpenClipboard(NULL))
		return 0;

	wchar_t * widebuffer = 0;
	char buffer[1401];

	HANDLE hData = GetClipboardData( CF_UNICODETEXT );
	widebuffer = (wchar_t*)GlobalLock( hData );
	
	int widelen = WideCharToMultiByte(CP_UTF8, WC_DEFAULTCHAR, widebuffer, -1, nullptr, 0, NULL, NULL);
	if(widelen > 0)
	{
		widelen = (widelen < 360 ? widelen : 360);
		WideCharToMultiByte(CP_UTF8, WC_DEFAULTCHAR, widebuffer, -1, &buffer[0], widelen, NULL, NULL);
		buffer[1400] = '\0';
	}
	char * result = buffer;

	GlobalUnlock( hData );
	CloseClipboard();
	return result;

#elif defined(_IRR_COMPILE_WITH_OSX_DEVICE_)
	return (OSXCopyFromClipboard());
	
	

#elif defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	return SDL_GetClipboardText();
#else

	return 0;
#endif
}
#endif


} // end namespace


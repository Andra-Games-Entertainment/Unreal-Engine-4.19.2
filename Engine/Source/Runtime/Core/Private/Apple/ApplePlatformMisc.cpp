// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformMisc.mm: iOS implementations of misc functions
=============================================================================*/

#include "ApplePlatformMisc.h"
#include "ExceptionHandling.h"
#include "SecureHash.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "Apple/ApplePlatformDebugEvents.h"
#include "Apple/ApplePlatformCrashContext.h"

void FApplePlatformMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
	ANSICHAR *AnsiResult = getenv(TCHAR_TO_ANSI(VariableName));
	if (AnsiResult)
	{
		wcsncpy(Result, ANSI_TO_TCHAR(AnsiResult), ResultLength);
	}
	else
	{
		*Result = 0;
	}
}

// Make sure that SetStoredValue and GetStoredValue generate the same key
static NSString* MakeStoredValueKeyName(const FString& SectionName, const FString& KeyName)
{
	return [NSString stringWithFString:(SectionName + "/" + KeyName)];
}

bool FApplePlatformMisc::SetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, const FString& InValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];

	// convert input to an NSString
	NSString* StoredValue = [NSString stringWithFString:InValue];

	// store it
	[UserSettings setObject:StoredValue forKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	return true;
}

bool FApplePlatformMisc::GetStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName, FString& OutValue)
{
	NSUserDefaults* UserSettings = [NSUserDefaults standardUserDefaults];
	
	// get the stored NSString
	NSString* StoredValue = [UserSettings objectForKey:MakeStoredValueKeyName(InSectionName, InKeyName)];

	// if it was there, convert back to FString
	if (StoredValue != nil)
	{
		OutValue = StoredValue;
		return true;
	}

	return false;
}

bool FApplePlatformMisc::DeleteStoredValue(const FString& InStoreId, const FString& InSectionName, const FString& InKeyName)
{
	// No Implementation (currently only used by editor code so not needed on iOS)
	return false;
}

void FApplePlatformMisc::LowLevelOutputDebugString( const TCHAR *Message )
{
	//NsLog will out to all iOS output consoles, instead of just the Xcode console.
	NSLog(@"%@", [NSString stringWithUTF8String:TCHAR_TO_UTF8(Message)]);
}

const TCHAR* FApplePlatformMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	// There's no iOS equivalent for GetLastError()
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	return OutBuffer;
}

FString FApplePlatformMisc::GetDefaultLocale()
{
	CFLocaleRef loc = CFLocaleCopyCurrent();
    
	TCHAR langCode[20];
	CFArrayRef langs = CFLocaleCopyPreferredLanguages();
	CFStringRef langCodeStr = (CFStringRef)CFArrayGetValueAtIndex(langs, 0);
	FPlatformString::CFStringToTCHAR(langCodeStr, langCode);
    
	TCHAR countryCode[20];
	CFStringRef countryCodeStr = (CFStringRef)CFLocaleGetValue(loc, kCFLocaleCountryCode);
	FPlatformString::CFStringToTCHAR(countryCodeStr, countryCode);
    
	return FString::Printf(TEXT("%s_%s"), langCode, countryCode);
}

FString FApplePlatformMisc::GetDefaultLanguage()
{
	CFArrayRef Languages = CFLocaleCopyPreferredLanguages();
	CFStringRef LangCodeStr = (CFStringRef)CFArrayGetValueAtIndex(Languages, 0);
	FString LangCode((__bridge NSString*)LangCodeStr);
	CFRelease(Languages);

	return LangCode;
}

int32 FApplePlatformMisc::NumberOfCores()
{
	// cache the number of cores
	static int32 NumberOfCores = -1;
	if (NumberOfCores == -1)
	{
		SIZE_T Size = sizeof(int32);
		if (sysctlbyname("hw.ncpu", &NumberOfCores, &Size, NULL, 0) != 0)
		{
			NumberOfCores = 1;
		}
	}
	return NumberOfCores;
}

void FApplePlatformMisc::CreateGuid(FGuid& Result)
{
	uuid_t UUID;
	uuid_generate(UUID);
	
	uint32* Values = (uint32*)(&UUID[0]);
	Result[0] = Values[0];
	Result[1] = Values[1];
	Result[2] = Values[2];
	Result[3] = Values[3];
}

void* FApplePlatformMisc::CreateAutoreleasePool()
{
	return [[NSAutoreleasePool alloc] init];
}

void FApplePlatformMisc::ReleaseAutoreleasePool(void *Pool)
{
	[(NSAutoreleasePool*)Pool release];
}

struct FFontHeader
{
	int32 Version;
	uint16 NumTables;
	uint16 SearchRange;
	uint16 EntrySelector;
	uint16 RangeShift;
};

struct FFontTableEntry
{
	uint32 Tag;
	uint32 CheckSum;
	uint32 Offset;
	uint32 Length;
};


static uint32 CalcTableCheckSum(const uint32 *Table, uint32 NumberOfBytesInTable)
{
	uint32 Sum = 0;
	uint32 NumLongs = (NumberOfBytesInTable + 3) / 4;
	while (NumLongs-- > 0)
	{
		Sum += CFSwapInt32HostToBig(*Table++);
	}
	return Sum;
}

static uint32 CalcTableDataRefCheckSum(CFDataRef DataRef)
{
	const uint32 *DataBuff = (const uint32 *)CFDataGetBytePtr(DataRef);
	uint32 DataLength = (uint32)CFDataGetLength(DataRef);
	return CalcTableCheckSum(DataBuff, DataLength);
}

/**
 * In order to get a system font from IOS we need to build one from the data we can gather from a CGFontRef
 * @param InFontName - The name of the system font we are seeking to load.
 * @param OutBytes - The data we have built for the font.
 */
void GetBytesForFont(const NSString* InFontName, OUT TArray<uint8>& OutBytes)
{
	CGFontRef cgFont = CGFontCreateWithFontName((CFStringRef)InFontName);

	if (cgFont)
	{
		CFRetain(cgFont);

		// Gather information on the font tags
		CFArrayRef Tags = CGFontCopyTableTags(cgFont);
		int TableCount = CFArrayGetCount(Tags);

		// Collate the table sizes
		TArray<size_t> TableSizes;

		bool bContainsCFFTable = false;

		size_t TotalSize = sizeof(FFontHeader)+sizeof(FFontTableEntry)* TableCount;
		for (int TableIndex = 0; TableIndex < TableCount; ++TableIndex)
		{
			size_t TableSize = 0;
			
			uint64 aTag = (uint64)CFArrayGetValueAtIndex(Tags, TableIndex);
			if (aTag == 'CFF ' && !bContainsCFFTable)
			{
				bContainsCFFTable = true;
			}

			CFDataRef TableDataRef = CGFontCopyTableForTag(cgFont, aTag);
			if (TableDataRef != NULL)
			{
				TableSize = CFDataGetLength(TableDataRef);
				CFRelease(TableDataRef);
			}

			TotalSize += (TableSize + 3) & ~3;
			TableSizes.Add( TableSize );
		}

		OutBytes.Reserve( TotalSize );
		OutBytes.AddZeroed( TotalSize );

		// Start copying the table data into our buffer
		uint8* DataStart = OutBytes.GetData();
		uint8* DataPtr = DataStart;

		// Compute font header entries
		uint16 EntrySelector = 0;
		uint16 SearchRange = 1;
		while (SearchRange < TableCount >> 1)
		{
			EntrySelector++;
			SearchRange <<= 1;
		}
		SearchRange <<= 4;

		uint16 RangeShift = (TableCount << 4) - SearchRange;

		// Write font header (also called sfnt header, offset subtable)
		FFontHeader* OffsetTable = (FFontHeader*)DataPtr;

		// OpenType Font contains CFF Table use 'OTTO' as version, and with .otf extension
		// otherwise 0001 0000
		OffsetTable->Version = bContainsCFFTable ? 'OTTO' : CFSwapInt16HostToBig(1);
		OffsetTable->NumTables = CFSwapInt16HostToBig((uint16)TableCount);
		OffsetTable->SearchRange = CFSwapInt16HostToBig((uint16)SearchRange);
		OffsetTable->EntrySelector = CFSwapInt16HostToBig((uint16)EntrySelector);
		OffsetTable->RangeShift = CFSwapInt16HostToBig((uint16)RangeShift);

		DataPtr += sizeof(FFontHeader);

		// Write tables
		FFontTableEntry* CurrentTableEntry = (FFontTableEntry*)DataPtr;
		DataPtr += sizeof(FFontTableEntry) * TableCount;

		for (int TableIndex = 0; TableIndex < TableCount; ++TableIndex)
		{
			uint64 aTag = (uint64)CFArrayGetValueAtIndex(Tags, TableIndex);
			CFDataRef TableDataRef = CGFontCopyTableForTag(cgFont, aTag);
			uint32 TableSize = CFDataGetLength(TableDataRef);

			FMemory::Memcpy(DataPtr, CFDataGetBytePtr(TableDataRef), TableSize);

			CurrentTableEntry->Tag = CFSwapInt32HostToBig((uint32_t)aTag);
			CurrentTableEntry->CheckSum = CFSwapInt32HostToBig(CalcTableCheckSum((uint32 *)DataPtr, TableSize));

			uint32 Offset = DataPtr - DataStart;
			CurrentTableEntry->Offset = CFSwapInt32HostToBig((uint32)Offset);
			CurrentTableEntry->Length = CFSwapInt32HostToBig((uint32)TableSize);

			DataPtr += (TableSize + 3) & ~3;
			++CurrentTableEntry;

			CFRelease(TableDataRef);
		}

		CFRelease(cgFont);
	}
}


TArray<uint8> FApplePlatformMisc::GetSystemFontBytes()
{
#if PLATFORM_MAC
	// Gather some details about the system font
	uint32 SystemFontSize = [NSFont systemFontSize];
	NSString* SystemFontName = [NSFont systemFontOfSize:SystemFontSize].fontName;
#elif PLATFORM_TVOS
	NSString* SystemFontName = [UIFont preferredFontForTextStyle:UIFontTextStyleBody].fontName;
#else
	// Gather some details about the system font
	uint32 SystemFontSize = [UIFont systemFontSize];
	NSString* SystemFontName = [UIFont systemFontOfSize:SystemFontSize].fontName;
#endif

	TArray<uint8> FontBytes;
	GetBytesForFont(SystemFontName, FontBytes);

	return FontBytes;
}

TArray<FString> FApplePlatformMisc::GetPreferredLanguages()
{
	TArray<FString> Results;

	NSArray* Languages = [NSLocale preferredLanguages];
	for (NSString* Language in Languages)
	{
		Results.Add(FString(Language));
	}
	return Results;
}

FString FApplePlatformMisc::GetLocalCurrencyCode()
{
	return FString([[NSLocale currentLocale] objectForKey:NSLocaleCurrencyCode]);
}

FString FApplePlatformMisc::GetLocalCurrencySymbol()
{
	return FString([[NSLocale currentLocale] objectForKey:NSLocaleCurrencySymbol]);
}

#if APPLE_PROFILING_ENABLED
void FApplePlatformMisc::BeginNamedEvent(const struct FColor& Color,const TCHAR* Text)
{
	FApplePlatformDebugEvents::BeginNamedEvent(Color, Text);
}

void FApplePlatformMisc::BeginNamedEvent(const struct FColor& Color,const ANSICHAR* Text)
{
	FApplePlatformDebugEvents::BeginNamedEvent(Color, Text);
}

void FApplePlatformMisc::EndNamedEvent()
{
	FApplePlatformDebugEvents::EndNamedEvent();
}
#endif

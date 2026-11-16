#include "Session/DCCBuildStamp.h"

#include "Misc/ConfigCacheIni.h"

namespace DCCBuildStamp
{
	FString Get()
	{
#if WITH_EDITOR
		return TEXT("dev");
#else
		static const FString Stamp = []()
		{
			FString Value;
			if (GConfig)
			{
				GConfig->GetString(TEXT("DCCBuild"), TEXT("Stamp"), Value, GGameIni);
			}
			Value.TrimStartAndEndInline();
			return Value.IsEmpty() ? FString(TEXT("unstamped")) : Value;
		}();
		return Stamp;
#endif
	}

	bool IsDev()
	{
		return Get() == TEXT("dev");
	}
}

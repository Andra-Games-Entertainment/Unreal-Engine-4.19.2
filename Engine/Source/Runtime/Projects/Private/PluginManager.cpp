// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PluginManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "ProjectDescriptor.h"
#include "Interfaces/IProjectManager.h"
#include "Modules/ModuleManager.h"
#include "ProjectManager.h"

DEFINE_LOG_CATEGORY_STATIC( LogPluginManager, Log, All );

#define LOCTEXT_NAMESPACE "PluginManager"


namespace PluginSystemDefs
{
	/** File extension of plugin descriptor files.
	    NOTE: This constant exists in UnrealBuildTool code as well. */
	static const TCHAR PluginDescriptorFileExtension[] = TEXT( ".uplugin" );

	/**
	 * Parsing the command line and loads any foreign plugins that were
	 * specified using the -PLUGIN= command.
	 *
	 * @param  CommandLine    The commandline used to launch the editor.
	 * @param  SearchPathsOut 
	 * @return The number of plugins that were specified using the -PLUGIN param.
	 */
	static int32 GetAdditionalPluginPaths(TSet<FString>& PluginPathsOut)
	{
		const TCHAR* SwitchStr = TEXT("PLUGIN=");
		const int32  SwitchLen = FCString::Strlen(SwitchStr);

		int32 PluginCount = 0;

		const TCHAR* SearchStr = FCommandLine::Get();
		do
		{
			FString PluginPath;

			SearchStr = FCString::Strifind(SearchStr, SwitchStr);
			if (FParse::Value(SearchStr, SwitchStr, PluginPath))
			{
				FString PluginDir = FPaths::GetPath(PluginPath);
				PluginPathsOut.Add(PluginDir);

				++PluginCount;
				SearchStr += SwitchLen + PluginPath.Len();
			}
			else
			{
				break;
			}
		} while (SearchStr != nullptr);

#if IS_PROGRAM
		// For programs that have the project dir set, look for plugins under the project directory
		const FProjectDescriptor *Project = IProjectManager::Get().GetCurrentProject();
		if (Project != nullptr)
		{
			PluginPathsOut.Add(FPaths::GetPath(FPaths::GetProjectFilePath()) / TEXT("Plugins"));
		}
#endif
		return PluginCount;
	}
}

FPlugin::FPlugin(const FString& InFileName, const FPluginDescriptor& InDescriptor, EPluginLoadedFrom InLoadedFrom)
	: Name(FPaths::GetBaseFilename(InFileName))
	, FileName(InFileName)
	, Descriptor(InDescriptor)
	, LoadedFrom(InLoadedFrom)
	, bEnabled(false)
{

}

FPlugin::~FPlugin()
{
}

FString FPlugin::GetName() const
{
	return Name;
}

FString FPlugin::GetDescriptorFileName() const
{
	return FileName;
}

FString FPlugin::GetBaseDir() const
{
	return FPaths::GetPath(FileName);
}

FString FPlugin::GetContentDir() const
{
	return FPaths::GetPath(FileName) / TEXT("Content");
}

FString FPlugin::GetMountedAssetPath() const
{
	return FString::Printf(TEXT("/%s/"), *Name);
}

bool FPlugin::IsEnabled() const
{
	return bEnabled;
}

bool FPlugin::CanContainContent() const
{
	return Descriptor.bCanContainContent;
}

EPluginLoadedFrom FPlugin::GetLoadedFrom() const
{
	return LoadedFrom;
}

const FPluginDescriptor& FPlugin::GetDescriptor() const
{
	return Descriptor;
}

bool FPlugin::UpdateDescriptor(const FPluginDescriptor& NewDescriptor, FText& OutFailReason)
{
	if(!NewDescriptor.Save(FileName, LoadedFrom == EPluginLoadedFrom::GameProject, OutFailReason))
	{
		return false;
	}

	Descriptor = NewDescriptor;
	return true;
}

FPluginManager::FPluginManager()
	: bHaveConfiguredEnabledPlugins(false)
	, bHaveAllRequiredPlugins(false)
{
	DiscoverAllPlugins();
}

FPluginManager::~FPluginManager()
{
	// NOTE: All plugins and modules should be cleaned up or abandoned by this point

	// @todo plugin: Really, we should "reboot" module manager's unloading code so that it remembers at which startup phase
	//  modules were loaded in, so that we can shut groups of modules down (in reverse-load order) at the various counterpart
	//  shutdown phases.  This will fix issues where modules that are loaded after game modules are shutdown AFTER many engine
	//  systems are already killed (like GConfig.)  Currently the only workaround is to listen to global exit events, or to
	//  explicitly unload your module somewhere.  We should be able to handle most cases automatically though!
}

void FPluginManager::RefreshPluginsList()
{
	// Read a new list of all plugins
	TMap<FString, TSharedRef<FPlugin>> NewPlugins;
	ReadAllPlugins(NewPlugins, PluginDiscoveryPaths);

	// Build a list of filenames for plugins which are enabled, and remove the rest
	TArray<FString> EnabledPluginFileNames;
	for(TMap<FString, TSharedRef<FPlugin>>::TIterator Iter(AllPlugins); Iter; ++Iter)
	{
		const TSharedRef<FPlugin>& Plugin = Iter.Value();
		if(Plugin->bEnabled)
		{
			EnabledPluginFileNames.Add(Plugin->FileName);
		}
		else
		{
			Iter.RemoveCurrent();
		}
	}

	// Add all the plugins which aren't already enabled
	for(const TPair<FString, TSharedRef<FPlugin>>& NewPluginPair: NewPlugins)
	{
		const TSharedRef<FPlugin>& NewPlugin = NewPluginPair.Value;
		if(!EnabledPluginFileNames.Contains(NewPlugin->FileName))
		{
			AllPlugins.Add(NewPlugin->GetName(), NewPlugin);
		}
	}
}

void FPluginManager::DiscoverAllPlugins()
{
	ensure( AllPlugins.Num() == 0 );		// Should not have already been initialized!

	PluginSystemDefs::GetAdditionalPluginPaths(PluginDiscoveryPaths);
	ReadAllPlugins(AllPlugins, PluginDiscoveryPaths);
}

void FPluginManager::ReadAllPlugins(TMap<FString, TSharedRef<FPlugin>>& Plugins, const TSet<FString>& ExtraSearchPaths)
{
#if (WITH_ENGINE && !IS_PROGRAM) || WITH_PLUGIN_SUPPORT
	// Find "built-in" plugins.  That is, plugins situated right within the Engine directory.
	ReadPluginsInDirectory(FPaths::EnginePluginsDir(), EPluginLoadedFrom::Engine, Plugins);

	// Find plugins in the game project directory (<MyGameProject>/Plugins). If there are any engine plugins matching the name of a game plugin,
	// assume that the game plugin version is preferred.
	if( FApp::HasGameName() )
	{
		ReadPluginsInDirectory(FPaths::GamePluginsDir(), EPluginLoadedFrom::GameProject, Plugins);
	}

	const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
	if (Project != nullptr)
	{
		// If they have a list of additional directories to check, add those plugins too
		for (const FString& Dir : Project->GetAdditionalPluginDirectories())
		{
			ReadPluginsInDirectory(Dir, EPluginLoadedFrom::Engine, Plugins);
		}
	}

	for (const FString& ExtraSearchPath : ExtraSearchPaths)
	{
		ReadPluginsInDirectory(ExtraSearchPath, EPluginLoadedFrom::GameProject, Plugins);
	}
#endif
}

void FPluginManager::ReadPluginsInDirectory(const FString& PluginsDirectory, const EPluginLoadedFrom LoadedFrom, TMap<FString, TSharedRef<FPlugin>>& Plugins)
{
	// Make sure the directory even exists
	if(FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*PluginsDirectory))
	{
		TArray<FString> FileNames;
		FindPluginsInDirectory(PluginsDirectory, FileNames);

		for(const FString& FileName: FileNames)
		{
			FPluginDescriptor Descriptor;
			FText FailureReason;
			if ( Descriptor.Load(FileName, LoadedFrom == EPluginLoadedFrom::GameProject, FailureReason) )
			{
				TSharedRef<FPlugin> Plugin = MakeShareable(new FPlugin(FileName, Descriptor, LoadedFrom));
				
				FString FullPath = FPaths::ConvertRelativePathToFull(FileName);
				UE_LOG(LogPluginManager, Verbose, TEXT("Read plugin descriptor for %s, from %s"), *Plugin->GetName(), *FullPath);

				const TSharedRef<FPlugin>* ExistingPlugin = Plugins.Find(Plugin->GetName());
				if (ExistingPlugin == nullptr)
				{
					Plugins.Add(Plugin->GetName(), Plugin);
				}
				else if ((*ExistingPlugin)->LoadedFrom == EPluginLoadedFrom::Engine && LoadedFrom == EPluginLoadedFrom::GameProject)
				{
					Plugins[Plugin->GetName()] = Plugin;
					UE_LOG(LogPluginManager, Verbose, TEXT("Replacing engine version of '%s' plugin with game version"), *Plugin->GetName());
				}
				else if( (*ExistingPlugin)->LoadedFrom != EPluginLoadedFrom::GameProject || LoadedFrom != EPluginLoadedFrom::Engine)
				{
					UE_LOG(LogPluginManager, Warning, TEXT("Plugin '%s' exists at '%s' and '%s' - second location will be ignored"), *Plugin->GetName(), *(*ExistingPlugin)->FileName, *Plugin->FileName);
				}
			}
			else
			{
				// NOTE: Even though loading of this plugin failed, we'll keep processing other plugins
				FString FullPath = FPaths::ConvertRelativePathToFull(FileName);
				FText FailureMessage = FText::Format(LOCTEXT("FailureFormat", "{0} ({1})"), FailureReason, FText::FromString(FullPath));
				FText DialogTitle = LOCTEXT("PluginFailureTitle", "Failed to load Plugin");
				UE_LOG(LogPluginManager, Error, TEXT("%s"), *FailureMessage.ToString());
				FMessageDialog::Open(EAppMsgType::Ok, FailureMessage, &DialogTitle);
			}
		}
	}
}

void FPluginManager::FindPluginsInDirectory(const FString& PluginsDirectory, TArray<FString>& FileNames)
{
	// Class to enumerate the contents of a directory, and find all sub-directories and plugin descriptors within it
	struct FPluginDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString> SubDirectories;
		TArray<FString> PluginDescriptors;

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			FString FilenameOrDirectoryStr = FilenameOrDirectory;
			if(bIsDirectory)
			{
				SubDirectories.Add(FilenameOrDirectoryStr);
			}
			else if(FilenameOrDirectoryStr.EndsWith(TEXT(".uplugin")))
			{
				PluginDescriptors.Add(FilenameOrDirectoryStr);
			}
			return true;
		}
	};

	// Enumerate the contents of the current directory
	FPluginDirectoryVisitor Visitor;
	FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*PluginsDirectory, Visitor);

	// If there's no plugins in this directory, recurse through all the child directories
	if(Visitor.PluginDescriptors.Num() == 0)
	{
		for(const FString& SubDirectory: Visitor.SubDirectories)
		{
			FindPluginsInDirectory(SubDirectory, FileNames);
		}
	}
	else
	{
		for(const FString& PluginDescriptor: Visitor.PluginDescriptors)
		{
			if (!FileNames.Contains(PluginDescriptor))
			{
				FileNames.Add(PluginDescriptor);
			}
		}
	}
}

// Helper class to find all pak files.
class FPakFileSearchVisitor : public IPlatformFile::FDirectoryVisitor
{
	TArray<FString>& FoundFiles;
public:
	FPakFileSearchVisitor(TArray<FString>& InFoundFiles)
		: FoundFiles(InFoundFiles)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			if (Filename.MatchesWildcard(TEXT("*.pak")) && !FoundFiles.Contains(Filename))
			{
				FoundFiles.Add(Filename);
			}
		}
		return true;
	}
};

bool FPluginManager::ConfigureEnabledPlugins()
{
	if(!bHaveConfiguredEnabledPlugins)
	{
		// Don't need to run this again
		bHaveConfiguredEnabledPlugins = true;

		// Get all the enabled plugin names
		TArray<FString> InitialPluginNames;
		TMap<FString, const FPluginReferenceDescriptor*> NameToPluginReference;
#if IS_PROGRAM
		// Programs can also define the list of enabled plugins in ini
		GConfig->GetArray(TEXT("Plugins"), TEXT("ProgramEnabledPlugins"), InitialPluginNames, GEngineIni);
#endif
#if !IS_PROGRAM || HACK_HEADER_GENERATOR
		if (!FParse::Param(FCommandLine::Get(), TEXT("NoEnginePlugins")))
		{
			// Find all the plugin references in the project file
			const FProjectDescriptor* ProjectDescriptor = IProjectManager::Get().GetCurrentProject();
			if (ProjectDescriptor != nullptr)
			{
				for (const FPluginReferenceDescriptor& PluginReference : ProjectDescriptor->Plugins)
				{
					NameToPluginReference.FindOrAdd(PluginReference.Name) = &PluginReference;
				}
			}

			// Get the default list of plugin names
			for (const TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
			{
				const FPluginReferenceDescriptor* PluginReference = NameToPluginReference.FindRef(PluginPair.Key);
				if ((PluginReference == nullptr && PluginPair.Value->Descriptor.bEnabledByDefault) || (PluginReference != nullptr && PluginReference->bEnabled))
				{
					InitialPluginNames.Add(PluginPair.Key);
				}
			}
		}
#endif

		// Enable all the plugins
		TSet<FString> EnabledPluginNames;
		for (const FString& PluginName : InitialPluginNames)
		{
			const FPluginReferenceDescriptor* EnabledPluginReference = NameToPluginReference.FindRef(PluginName);
			if (EnabledPluginReference == nullptr)
			{
				if (!ConfigureEnabledPlugin(FPluginReferenceDescriptor(PluginName, true), EnabledPluginNames))
				{
					return false;
				}
			}
			else
			{
				if (!ConfigureEnabledPlugin(*EnabledPluginReference, EnabledPluginNames))
				{
					return false;
				}
			}
		}

		// If we made it here, we have all the required plugins
		bHaveAllRequiredPlugins = true;

		// Mount all the enabled plugins
		for(const TPair<FString, TSharedRef<FPlugin>>& PluginPair: AllPlugins)
		{
			const FPlugin& Plugin = *PluginPair.Value;
			if (Plugin.bEnabled)
			{
				// Build the list of content folders
				if (Plugin.Descriptor.bCanContainContent)
				{
					if (FConfigFile* EngineConfigFile = GConfig->Find(GEngineIni, false))
					{
						if (FConfigSection* CoreSystemSection = EngineConfigFile->Find(TEXT("Core.System")))
						{
							CoreSystemSection->AddUnique("Paths", Plugin.GetContentDir());
						}
					}
				}

				// Load <PluginName>.ini config file if it exists
				FString PluginConfigDir = FPaths::GetPath(Plugin.FileName) / TEXT("Config/");
				FString EngineConfigDir = FPaths::EngineConfigDir();
				FString SourceConfigDir = FPaths::SourceConfigDir();

				// Load Engine plugins out of BasePluginName.ini and the engine directory, game plugins out of DefaultPluginName.ini
				if (Plugin.LoadedFrom == EPluginLoadedFrom::Engine)
				{
					EngineConfigDir = PluginConfigDir;
				}
				else
				{
					SourceConfigDir = PluginConfigDir;
				}

				FString PluginConfigFilename = FString::Printf(TEXT("%s%s/%s.ini"), *FPaths::GeneratedConfigDir(), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()), *Plugin.Name);
				FConfigFile& PluginConfig = GConfig->Add(PluginConfigFilename, FConfigFile());

				// This will write out an ini to PluginConfigFilename
				if (!FConfigCacheIni::LoadExternalIniFile(PluginConfig, *Plugin.Name, *EngineConfigDir, *SourceConfigDir, true, nullptr, false, true))
				{
					// Nothing to add, remove from map
					GConfig->Remove(PluginConfigFilename);
				}

				if (!GIsEditor)
				{
					// override config cache entries with plugin configs (Engine.ini, Game.ini, etc in <PluginDir>\Config\)
					TArray<FString> PluginConfigs;
					IFileManager::Get().FindFiles(PluginConfigs, *PluginConfigDir, TEXT("ini"));
					for (const FString& ConfigFile : PluginConfigs)
					{
						FString PlaformName = FPlatformProperties::PlatformName();
						PluginConfigFilename = FString::Printf(TEXT("%s%s/%s.ini"), *FPaths::GeneratedConfigDir(), *PlaformName, *FPaths::GetBaseFilename(ConfigFile));
						FConfigFile* FoundConfig = GConfig->Find(PluginConfigFilename, false);
						if (FoundConfig != nullptr)
						{
							FString PluginConfigContent;
							if (FFileHelper::LoadFileToString(PluginConfigContent, *FPaths::Combine(PluginConfigDir, ConfigFile)))
							{
								FoundConfig->CombineFromBuffer(PluginConfigContent);
								// if plugin config overrides are applied then don't save
								FoundConfig->NoSave = true;
							}
						}
					}
				}
			}
		}

		// Mount all the plugin content folders and pak files
		TArray<FString>	FoundPaks;
		FPakFileSearchVisitor PakVisitor(FoundPaks);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		for(TSharedRef<IPlugin> Plugin: GetEnabledPlugins())
		{
			if (Plugin->CanContainContent() && ensure(RegisterMountPointDelegate.IsBound()))
			{
				FString ContentDir = Plugin->GetContentDir();
				RegisterMountPointDelegate.Execute(Plugin->GetMountedAssetPath(), ContentDir);

				// Pak files are loaded from <PluginName>/Content/Paks/<PlatformName>
				if (FPlatformProperties::RequiresCookedData())
				{
					FoundPaks.Reset();
					PlatformFile.IterateDirectoryRecursively(*(ContentDir / TEXT("Paks") / FPlatformProperties::PlatformName()), PakVisitor);
					for (const FString& PakPath : FoundPaks)
					{
						if (FCoreDelegates::OnMountPak.IsBound())
						{
							FCoreDelegates::OnMountPak.Execute(PakPath, 0, nullptr);
							PluginsWithPakFile.AddUnique(Plugin);
						}
						else
						{
							UE_LOG(LogPluginManager, Warning, TEXT("PAK file (%s) could not be mounted because OnMountPak is not bound"), *PakPath)
						}
					}
				}
			}
		}
	}
	return bHaveAllRequiredPlugins;
}

bool FPluginManager::ConfigureEnabledPlugin(const FPluginReferenceDescriptor& FirstReference, TSet<FString>& EnabledPluginNames)
{
	if (!EnabledPluginNames.Contains(FirstReference.Name))
	{
		// Set of plugin names we've added to the queue for processing
		TSet<FString> NewPluginNames;
		NewPluginNames.Add(FirstReference.Name);

		// Queue of plugin references to consider
		TArray<const FPluginReferenceDescriptor*> NewPluginReferences;
		NewPluginReferences.Add(&FirstReference);

		// Loop through the queue of plugin references that need to be enabled, queuing more items as we go
		TArray<TSharedRef<FPlugin>> NewPlugins;
		for (int32 Idx = 0; Idx < NewPluginReferences.Num(); Idx++)
		{
			const FPluginReferenceDescriptor& Reference = *NewPluginReferences[Idx];

			// Check if the plugin is required for this platform
			if(!Reference.IsEnabledForPlatform(FPlatformMisc::GetUBTPlatform()) || !Reference.IsEnabledForTarget(FPlatformMisc::GetUBTTarget()))
			{
				UE_LOG(LogPluginManager, Verbose, TEXT("Ignoring plugin '%s' for platform/configuration"), *Reference.Name);
				continue;
			}

			// Find the plugin being enabled
			TSharedRef<FPlugin>* PluginPtr = AllPlugins.Find(Reference.Name);
			if (PluginPtr == nullptr)
			{
				// Ignore any optional plugins
				if (Reference.bOptional)
				{
					UE_LOG(LogPluginManager, Verbose, TEXT("Ignored optional reference to '%s' plugin; plugin was not found."), *Reference.Name);
					continue;
				}

				// If we're in unattended mode, don't open any windows
				if (FApp::IsUnattended())
				{
					UE_LOG(LogPluginManager, Error, TEXT("This project requires the '%s' plugin. Install it and try again, or remove it from the project's required plugin list."), *Reference.Name);
					return false;
				}

#if !IS_MONOLITHIC
				// Try to download it from the marketplace
				if (Reference.MarketplaceURL.Len() > 0 && PromptToDownloadPlugin(Reference.Name, Reference.MarketplaceURL))
				{
					UE_LOG(LogPluginManager, Display, TEXT("Downloading '%s' plugin from marketplace (%s)."), *Reference.Name, *Reference.MarketplaceURL);
					return false;
				}

				// Prompt to disable it in the project file, if possible
				if (PromptToDisableMissingPlugin(FirstReference.Name, Reference.Name))
				{
					UE_LOG(LogPluginManager, Display, TEXT("Disabled plugin '%s', continuing."), *FirstReference.Name);
					return true;
				}
#endif

				// Unable to continue
				UE_LOG(LogPluginManager, Error, TEXT("Unable to load plugin '%s'. Aborting."), *Reference.Name);
				return false;
			}

			// Check the plugin is not disabled by the platform
			FPlugin& Plugin = PluginPtr->Get();

			// Allow the platform to disable it
			if (FPlatformMisc::ShouldDisablePluginAtRuntime(Plugin.Name))
			{
				UE_LOG(LogPluginManager, Verbose, TEXT("Plugin '%s' was disabled by platform."), *Reference.Name);
				continue;
			}

#if !IS_MONOLITHIC
			// Mount the binaries directory, and check the modules are valid
			if (Plugin.Descriptor.Modules.Num() > 0)
			{
				// Mount the binaries directory
				const FString PluginBinariesPath = FPaths::Combine(*FPaths::GetPath(Plugin.FileName), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
				FModuleManager::Get().AddBinariesDirectory(*PluginBinariesPath, Plugin.LoadedFrom == EPluginLoadedFrom::GameProject);

				// Check if the modules are valid
				TArray<FString> IncompatibleFiles;
				if (!FModuleDescriptor::CheckModuleCompatibility(Plugin.Descriptor.Modules, Plugin.LoadedFrom == EPluginLoadedFrom::GameProject, IncompatibleFiles))
				{
					if (PromptToDisableIncompatiblePlugin(FirstReference.Name, Reference.Name))
					{
						UE_LOG(LogPluginManager, Display, TEXT("Disabled plugin '%s', continuing."), *FirstReference.Name);
						return true;
					}
				}
			}

			// Check the declared engine version. This is a soft requirement, so allow the user to skip over it.
			if (!IsPluginCompatible(Plugin) && !PromptToLoadIncompatiblePlugin(Plugin, FirstReference.Name))
			{
				UE_LOG(LogPluginManager, Display, TEXT("Skipping load of '%s'."), *Plugin.Name);
				return true;
			}
#endif

			// Add references to all its dependencies
			for (const FPluginReferenceDescriptor& NextReference : Plugin.Descriptor.Plugins)
			{
				if (!EnabledPluginNames.Contains(NextReference.Name) && !NewPluginNames.Contains(NextReference.Name))
				{
					NewPluginNames.Add(NextReference.Name);
					NewPluginReferences.Add(&NextReference);
				}
			}

			// Add the plugin
			NewPlugins.Add(*PluginPtr);
		}

		// Mark all the plugins as enabled
		for (TSharedRef<FPlugin>& NewPlugin : NewPlugins)
		{
			NewPlugin->bEnabled = true;
			EnabledPluginNames.Add(NewPlugin->Name);
		}
	}
	return true;
}

bool FPluginManager::PromptToDownloadPlugin(const FString& PluginName, const FString& MarketplaceURL)
{
	FText Caption = FText::Format(LOCTEXT("DownloadPluginCaption", "Missing {0} Plugin"), FText::FromString(PluginName));
	FText Message = FText::Format(LOCTEXT("DownloadPluginMessage", "This project requires the {0} plugin.\n\nWould you like to download it from the Unreal Engine Marketplace?"), FText::FromString(PluginName));
	if(FMessageDialog::Open(EAppMsgType::YesNo, Message, &Caption) == EAppReturnType::Yes)
	{
		FString Error;
		FPlatformProcess::LaunchURL(*MarketplaceURL, nullptr, &Error);
		if (Error.Len() == 0)
		{
			return true;
		}
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
	}
	return false;
}

bool FPluginManager::PromptToDisableMissingPlugin(const FString& PluginName, const FString& MissingPluginName)
{
	FText Message;
	if (PluginName == MissingPluginName)
	{
		Message = FText::Format(LOCTEXT("DisablePluginMessage", "This project requires the '{0}' plugin, which could not be found.\n\nWould you like to disable it? You will no longer be able to open any assets created using it."), FText::FromString(PluginName));
	}
	else
	{
		Message = FText::Format(LOCTEXT("DisablePluginMessage", "This project requires the '{0}' plugin, which has a missing dependency on the '{1}' plugin.\n\nWould you like to disable it? You will no longer be able to open any assets created using it."), FText::FromString(PluginName), FText::FromString(MissingPluginName));
	}

	FText Caption(LOCTEXT("DisablePluginCaption", "Missing Plugin"));
	return PromptToDisablePlugin(Caption, Message, PluginName);
}

bool FPluginManager::PromptToDisableIncompatiblePlugin(const FString& PluginName, const FString& IncompatiblePluginName)
{
	FText Message;
	if (PluginName == IncompatiblePluginName)
	{
		Message = FText::Format(LOCTEXT("DisablePluginMessage", "This project requires the '{0}' plugin, which is not compatible with the current engine version.\n\nWould you like to disable it? You will no longer be able to open any assets created using it."), FText::FromString(PluginName));
	}
	else
	{
		Message = FText::Format(LOCTEXT("DisablePluginMessage", "This project requires the '{0}' plugin, which has a dependency on the '{1}' plugin, which is not compatible with the current engine version.\n\nWould you like to disable it? You will no longer be able to open any assets created using it."), FText::FromString(PluginName), FText::FromString(IncompatiblePluginName));
	}

	FText Caption(LOCTEXT("DisablePluginCaption", "Missing Plugin"));
	return PromptToDisablePlugin(Caption, Message, PluginName);
}

bool FPluginManager::PromptToDisablePlugin(const FText& Caption, const FText& Message, const FString& PluginName)
{
	// Check we have a project file. If this is a missing engine/program plugin referenced by something, we can't disable it through this method.
	if (IProjectManager::Get().GetCurrentProject() != nullptr)
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Caption) == EAppReturnType::Yes)
		{
			FText FailReason;
			if (IProjectManager::Get().SetPluginEnabled(*PluginName, false, FailReason))
			{
				return true;
			}
			FMessageDialog::Open(EAppMsgType::Ok, FailReason);
		}
	}
	return false;
}

bool FPluginManager::IsPluginCompatible(const FPlugin& Plugin)
{
	if (Plugin.Descriptor.EngineVersion.Len() > 0)
	{
		FEngineVersion Version;
		if (!FEngineVersion::Parse(Plugin.Descriptor.EngineVersion, Version))
		{
			UE_LOG(LogPluginManager, Warning, TEXT("Engine version string in %s could not be parsed (\"%s\")"), *Plugin.FileName, *Plugin.Descriptor.EngineVersion);
			return true;
		}

		EVersionComparison Comparison = FEngineVersion::GetNewest(FEngineVersion::CompatibleWith(), Version, nullptr);
		if (Comparison != EVersionComparison::Neither)
		{
			UE_LOG(LogPluginManager, Warning, TEXT("Plugin '%s' is not compatible with the current engine version (%s)"), *Plugin.Name, *Plugin.Descriptor.EngineVersion);
			return false;
		}
	}
	return true;
}

bool FPluginManager::PromptToLoadIncompatiblePlugin(const FPlugin& Plugin, const FString& ReferencingPluginName)
{
	// Format the message dependning on whether the plugin is referenced directly, or as a dependency
	FText Message;
	if (Plugin.Name == ReferencingPluginName)
	{
		Message = FText::Format(LOCTEXT("LoadIncompatiblePlugin", "The '{0}' plugin was designed for build {1}. Attempt to load it anyway?"), FText::FromString(Plugin.Name), FText::FromString(Plugin.Descriptor.EngineVersion));
	}
	else
	{
		Message = FText::Format(LOCTEXT("LoadIncompatibleDependencyPlugin", "The '{0}' plugin is required by the '{1}' plugin, but was designed for build {2}. Attempt to load it anyway?"), FText::FromString(Plugin.Name), FText::FromString(ReferencingPluginName), FText::FromString(Plugin.Descriptor.EngineVersion));
	}

	FText Caption = FText::Format(LOCTEXT("IncompatiblePluginCaption", "'{0}' is Incompatible"), FText::FromString(Plugin.Name));
	return FMessageDialog::Open(EAppMsgType::YesNo, Message, &Caption) == EAppReturnType::Yes;
}

TSharedPtr<FPlugin> FPluginManager::FindPluginInstance(const FString& Name)
{
	const TSharedRef<FPlugin>* Instance = AllPlugins.Find(Name);
	if (Instance == nullptr)
	{
		return TSharedPtr<FPlugin>();
	}
	else
	{
		return TSharedPtr<FPlugin>(*Instance);
	}
}


static bool TryLoadModulesForPlugin( const FPlugin& Plugin, const ELoadingPhase::Type LoadingPhase )
{
	TMap<FName, EModuleLoadResult> ModuleLoadFailures;
	FModuleDescriptor::LoadModulesForPhase(LoadingPhase, Plugin.Descriptor.Modules, ModuleLoadFailures);

	FText FailureMessage;
	for( auto FailureIt( ModuleLoadFailures.CreateConstIterator() ); FailureIt; ++FailureIt )
	{
		const FName ModuleNameThatFailedToLoad = FailureIt.Key();
		const EModuleLoadResult FailureReason = FailureIt.Value();

		if( FailureReason != EModuleLoadResult::Success )
		{
			const FText PluginNameText = FText::FromString(Plugin.Name);
			const FText TextModuleName = FText::FromName(FailureIt.Key());

			if ( FailureReason == EModuleLoadResult::FileNotFound )
			{
				FailureMessage = FText::Format( LOCTEXT("PluginModuleNotFound", "Plugin '{0}' failed to load because module '{1}' could not be found.  Please ensure the plugin is properly installed, otherwise consider disabling the plugin for this project."), PluginNameText, TextModuleName );
			}
			else if ( FailureReason == EModuleLoadResult::FileIncompatible )
			{
				FailureMessage = FText::Format( LOCTEXT("PluginModuleIncompatible", "Plugin '{0}' failed to load because module '{1}' does not appear to be compatible with the current version of the engine.  The plugin may need to be recompiled."), PluginNameText, TextModuleName );
			}
			else if ( FailureReason == EModuleLoadResult::CouldNotBeLoadedByOS )
			{
				FailureMessage = FText::Format( LOCTEXT("PluginModuleCouldntBeLoaded", "Plugin '{0}' failed to load because module '{1}' could not be loaded.  There may be an operating system error or the module may not be properly set up."), PluginNameText, TextModuleName );
			}
			else if ( FailureReason == EModuleLoadResult::FailedToInitialize )
			{
				FailureMessage = FText::Format( LOCTEXT("PluginModuleFailedToInitialize", "Plugin '{0}' failed to load because module '{1}' could not be initialized successfully after it was loaded."), PluginNameText, TextModuleName );
			}
			else 
			{
				ensure(0);	// If this goes off, the error handling code should be updated for the new enum values!
				FailureMessage = FText::Format( LOCTEXT("PluginGenericLoadFailure", "Plugin '{0}' failed to load because module '{1}' could not be loaded for an unspecified reason.  This plugin's functionality will not be available.  Please report this error."), PluginNameText, TextModuleName );
			}

			// Don't need to display more than one module load error per plugin that failed to load
			break;
		}
	}

	if( !FailureMessage.IsEmpty() )
	{
		FMessageDialog::Open(EAppMsgType::Ok, FailureMessage);
		return false;
	}

	return true;
}

bool FPluginManager::LoadModulesForEnabledPlugins( const ELoadingPhase::Type LoadingPhase )
{
	// Figure out which plugins are enabled
	if(!ConfigureEnabledPlugins())
	{
		return false;
	}

	FScopedSlowTask SlowTask(AllPlugins.Num());

	// Load plugins!
	for( const TPair<FString, TSharedRef< FPlugin >> PluginPair : AllPlugins )
	{
		const TSharedRef<FPlugin> &Plugin = PluginPair.Value;
		SlowTask.EnterProgressFrame(1);

		if ( Plugin->bEnabled )
		{
			if (!TryLoadModulesForPlugin(Plugin.Get(), LoadingPhase))
			{
				return false;
			}
		}
	}
	return true;
}

void FPluginManager::GetLocalizationPathsForEnabledPlugins( TArray<FString>& OutLocResPaths )
{
	// Figure out which plugins are enabled
	if (!ConfigureEnabledPlugins())
	{
		return;
	}

	// Gather the paths from all plugins that have localization targets that are loaded based on the current runtime environment
	for (const TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		const TSharedRef<FPlugin>& Plugin = PluginPair.Value;
		if (!Plugin->bEnabled || Plugin->GetDescriptor().LocalizationTargets.Num() == 0)
		{
			continue;
		}
		
		const FString PluginLocDir = Plugin->GetContentDir() / TEXT("Localization");
		for (const FLocalizationTargetDescriptor& LocTargetDesc : Plugin->GetDescriptor().LocalizationTargets)
		{
			if (LocTargetDesc.ShouldLoadLocalizationTarget())
			{
				OutLocResPaths.Add(PluginLocDir / LocTargetDesc.Name);
			}
		}
	}
}

void FPluginManager::SetRegisterMountPointDelegate( const FRegisterMountPointDelegate& Delegate )
{
	RegisterMountPointDelegate = Delegate;
}

bool FPluginManager::AreRequiredPluginsAvailable()
{
	return ConfigureEnabledPlugins();
}

bool FPluginManager::CheckModuleCompatibility(TArray<FString>& OutIncompatibleModules)
{
	if(!ConfigureEnabledPlugins())
	{
		return false;
	}

	bool bResult = true;
	for(const TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		const TSharedRef< FPlugin > &Plugin = PluginPair.Value;
		if (Plugin->bEnabled && !FModuleDescriptor::CheckModuleCompatibility(Plugin->Descriptor.Modules, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject, OutIncompatibleModules))
		{
			bResult = false;
		}
	}
	return bResult;
}

IPluginManager& IPluginManager::Get()
{
	// Single instance of manager, allocated on demand and destroyed on program exit.
	static FPluginManager* PluginManager = NULL;
	if( PluginManager == NULL )
	{
		PluginManager = new FPluginManager();
	}
	return *PluginManager;
}

TSharedPtr<IPlugin> FPluginManager::FindPlugin(const FString& Name)
{
	const TSharedRef<FPlugin>* Instance = AllPlugins.Find(Name);
	if (Instance == nullptr)
	{
		return TSharedPtr<IPlugin>();
	}
	else
	{
		return TSharedPtr<IPlugin>(*Instance);
	}
}

TArray<TSharedRef<IPlugin>> FPluginManager::GetEnabledPlugins()
{
	TArray<TSharedRef<IPlugin>> Plugins;
	for(TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		TSharedRef<FPlugin>& PossiblePlugin = PluginPair.Value;
		if(PossiblePlugin->bEnabled)
		{
			Plugins.Add(PossiblePlugin);
		}
	}
	return Plugins;
}

TArray<TSharedRef<IPlugin>> FPluginManager::GetDiscoveredPlugins()
{
	TArray<TSharedRef<IPlugin>> Plugins;
	for (TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins)
	{
		Plugins.Add(PluginPair.Value);
	}
	return Plugins;
}

TArray< FPluginStatus > FPluginManager::QueryStatusForAllPlugins() const
{
	TArray< FPluginStatus > PluginStatuses;

	for( const TPair<FString, TSharedRef<FPlugin>>& PluginPair : AllPlugins )
	{
		const TSharedRef< FPlugin >& Plugin = PluginPair.Value;
		
		FPluginStatus PluginStatus;
		PluginStatus.Name = Plugin->Name;
		PluginStatus.PluginDirectory = FPaths::GetPath(Plugin->FileName);
		PluginStatus.bIsEnabled = Plugin->bEnabled;
		PluginStatus.Descriptor = Plugin->Descriptor;
		PluginStatus.LoadedFrom = Plugin->LoadedFrom;

		PluginStatuses.Add( PluginStatus );
	}

	return PluginStatuses;
}

void FPluginManager::AddPluginSearchPath(const FString& ExtraDiscoveryPath, bool bRefresh)
{
	PluginDiscoveryPaths.Add(ExtraDiscoveryPath);
	if (bRefresh)
	{
		RefreshPluginsList();
	}
}

TArray<TSharedRef<IPlugin>> FPluginManager::GetPluginsWithPakFile() const
{
	return PluginsWithPakFile;
}

IPluginManager::FNewPluginMountedEvent& FPluginManager::OnNewPluginMounted()
{
	return NewPluginMountedEvent;
}

void FPluginManager::MountNewlyCreatedPlugin(const FString& PluginName)
{
	for(TMap<FString, TSharedRef<FPlugin>>::TIterator Iter(AllPlugins); Iter; ++Iter)
	{
		const TSharedRef<FPlugin>& Plugin = Iter.Value();
		if (Plugin->Name == PluginName)
		{
			// Mark the plugin as enabled
			Plugin->bEnabled = true;

			// Mount the plugin content directory
			if (Plugin->CanContainContent() && ensure(RegisterMountPointDelegate.IsBound()))
			{
				FString ContentDir = Plugin->GetContentDir();
				RegisterMountPointDelegate.Execute(Plugin->GetMountedAssetPath(), ContentDir);

				// Register this plugin's path with the list of content directories that the editor will search
				if (FConfigFile* EngineConfigFile = GConfig->Find(GEngineIni, false))
				{
					if (FConfigSection* CoreSystemSection = EngineConfigFile->Find(TEXT("Core.System")))
					{
						CoreSystemSection->AddUnique("Paths", Plugin->GetContentDir());
					}
				}
			}

			// If it's a code module, also load the modules for it
			if (Plugin->Descriptor.Modules.Num() > 0)
			{
				// Add the plugin binaries directory
				const FString PluginBinariesPath = FPaths::Combine(*FPaths::GetPath(Plugin->FileName), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
				FModuleManager::Get().AddBinariesDirectory(*PluginBinariesPath, Plugin->LoadedFrom == EPluginLoadedFrom::GameProject);

				// Load all the plugin modules
				for (ELoadingPhase::Type LoadingPhase = (ELoadingPhase::Type)0; LoadingPhase < ELoadingPhase::Max; LoadingPhase = (ELoadingPhase::Type)(LoadingPhase + 1))
				{
					if (LoadingPhase != ELoadingPhase::None)
					{
						TryLoadModulesForPlugin(Plugin.Get(), LoadingPhase);
					}
				}
			}

			// Notify any listeners that a new plugin has been mounted
			if (NewPluginMountedEvent.IsBound())
			{
				NewPluginMountedEvent.Broadcast(*Plugin);
			}
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE

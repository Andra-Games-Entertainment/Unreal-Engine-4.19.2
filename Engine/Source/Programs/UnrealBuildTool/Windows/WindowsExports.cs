﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public Linux functions exposed to UAT
	/// </summary>
	public static class WindowsExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="ProjectName"></param>
		/// <param name="ProjectDirectory"></param>
		/// <param name="InTargetConfigurations"></param>
		/// <param name="InExecutablePaths"></param>
		/// <param name="EngineDirectory"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="bIsDataDeploy"></param>
		/// <returns></returns>
		public static bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, string ProjectDirectory, List<UnrealTargetConfiguration> InTargetConfigurations, List<string> InExecutablePaths, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
	        BaseWindowsDeploy Deploy = new BaseWindowsDeploy();
            return Deploy.PrepForUATPackageOrDeploy(ProjectFile, ProjectName, ProjectDirectory, InTargetConfigurations, InExecutablePaths, EngineDirectory, bForDistribution, CookFlavor, bIsDataDeploy);
		}

		/// <summary>
		/// Tries to get the directory for an installed Visual Studio version
		/// </summary>
		/// <param name="Compiler">The compiler version</param>
		/// <param name="InstallDir">Receives the install directory on success</param>
		/// <returns>True if successful</returns>
		public static bool TryGetVSInstallDir(WindowsCompiler Compiler, out DirectoryReference InstallDir)
		{
			return WindowsPlatform.TryGetVSInstallDir(Compiler, out InstallDir);
		}

		/// <summary>
		/// Gets the path to MSBuild.exe
		/// </summary>
		/// <returns>Path to MSBuild.exe</returns>
		public static string GetMSBuildToolPath()
		{
			return VCEnvironment.GetMSBuildToolPath();
		}
	}
}

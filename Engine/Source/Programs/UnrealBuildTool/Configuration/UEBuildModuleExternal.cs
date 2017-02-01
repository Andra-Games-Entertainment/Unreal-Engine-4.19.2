﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// A module that is never compiled by us, and is only used to group include paths and libraries into a dependency unit.
	/// </summary>
	class UEBuildModuleExternal : UEBuildModule
	{
		public UEBuildModuleExternal(
			UHTModuleType InType,
			string InName,
			DirectoryReference InModuleDirectory,
			ModuleRules InRules,
			FileReference InRulesFile
			)
			: base(
				InType: InType,
				InName: InName,
				InModuleDirectory: InModuleDirectory,
				InRules: InRules,
				InRulesFile: InRulesFile
				)
		{
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, List<PrecompiledHeaderTemplate> SharedPCHs, ActionGraph ActionGraph)
		{
			return new List<FileItem>();
		}
	}
}

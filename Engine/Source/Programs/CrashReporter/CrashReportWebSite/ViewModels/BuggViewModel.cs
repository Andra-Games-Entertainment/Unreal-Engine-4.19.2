﻿// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Web.Mvc;
using Tools.CrashReporter.CrashReportWebSite.DataModels;

namespace Tools.CrashReporter.CrashReportWebSite.ViewModels
{
    public class CrashDataModel
    {
        public int Id { get; set; }
        public int CrashType { get; set; }
        public string ChangelistVersion { get; set; }
        public string BuildVersion { get; set; }
        public string UserName { get; set; }
        public string GameName { get; set; }
        public string Summary { get; set; }
        public string SourceContext { get; set; }

        public string EngineMode { get; set; }
        public string PlatformName { get; set; }
        public DateTime TimeOfCrash { get; set; }

        public string Description { get; set; }

        public string RawCallStack { get; set; }

        public string ComputerName { get; set; }

        public string Branch { get; set; }

        /// <summary>
        /// Return a display friendly version of the time of Crash.
        /// </summary>
        /// <returns>A pair of strings representing the date and time of the Crash.</returns>
        public string[] GetTimeOfCrash()
        {
            string[] Results = new string[2];
            
            DateTime LocalTimeOfCrash = TimeOfCrash.ToLocalTime();
            Results[0] = LocalTimeOfCrash.ToShortDateString();
            Results[1] = LocalTimeOfCrash.ToShortTimeString();

            return Results;
        }
    }

	/// <summary>
	/// The view model for the bugg show page.
	/// </summary>
	public class BuggViewModel
	{
		/// <summary>The current Bugg to display details of.</summary>
		public Bugg Bugg { get; set; }

		/// <summary>A container for all the Crashes associated with this Bugg.</summary>
		public List<CrashDataModel> CrashData { get; set; }

        /// <summary>A container for all the Crashes associated with this Bugg.</summary>
        public List<Crash> Crashes { get; set; }

		/// <summary>The call stack common to all Crashes in this Bugg.</summary>
		public CallStackContainer CallStack { get; set; }

		/// <summary></summary>
		public string SourceContext { get; set; }

		/// <summary>Time spent in generating this site, formatted as a string.</summary>
		public string GenerationTime { get; set; }

        /// <summary> Description of most recent Crash </summary>
        public string LatestCrashSummary { get; set; }

        /// <summary>Select list of jira projects</summary>
	    public List<SelectListItem> JiraProjects;

        /// <summary>Jira Project to which to add a new bugg.</summary>
        public string JiraProject { get; set; }

        /// <summary> </summary>
        public string Page { get; set; }

        public PagingInfo PagingInfo { get; set; }

        /// <summary>
        /// Constructor for BuggViewModel class.
        /// </summary>
	    public BuggViewModel()
	    {
            JiraProjects = new List<SelectListItem>()
	        {
                new SelectListItem(){ Text = "UE", Value = "UE"},
                new SelectListItem(){ Text = "FORT", Value = "FORT"},
                new SelectListItem(){ Text = "ORION", Value = "OR"},
	        };
	    }
	}
}
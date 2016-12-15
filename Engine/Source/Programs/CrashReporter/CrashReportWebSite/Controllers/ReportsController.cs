﻿// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Mail;
using System.Web;
using System.Web.Mvc;
using System.Web.Routing;
using Tools.CrashReporter.CrashReportWebSite.DataModels;
using Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories;
using Tools.CrashReporter.CrashReportWebSite.Models;
using Tools.CrashReporter.CrashReportWebSite.ViewModels;
using Hangfire;
using FormCollection = System.Web.Mvc.FormCollection;

namespace Tools.CrashReporter.CrashReportWebSite.Controllers
{

	//How hard would it be for you to create a CSV of:
	// Epic ID, 
	// buggs ID, 
	// engine version, 
	// 
	// # of crashes with that buggs ID for 
	//	this engine version and 
	//	user

    /// <summary>
	/// The controller to handle graphing of crashes per user group over time.
	/// </summary>
	public class ReportsController : Controller
    {
        #region Public Methods

        private IUnitOfWork _unitOfWork;

        /// <summary>
        /// 
        /// </summary>
        public ReportsController(IUnitOfWork unitOfWork)
        {
            _unitOfWork = unitOfWork;
        }

        /// <summary>Fake id for all user groups</summary>
		public static readonly int AllUserGroupId = -1;
        
		/// <summary>
		/// Get a report with the default form data and return the reports index view
		/// </summary>
		/// <param name="ReportsForm"></param>
		/// <returns></returns>
		public ActionResult Index( FormCollection ReportsForm )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString(), bCreateNewLog: true ) )
			{
				FormHelper FormData = new FormHelper( Request, ReportsForm, "JustReport" );

				// Handle 'CopyToJira' button
				int BuggIDToBeAddedToJira = 0;
				foreach( var Entry in ReportsForm )
				{
                    if (Entry.ToString().Contains("CopyToJira-"))
					{
                        int.TryParse(Entry.ToString().Substring("CopyToJira-".Length), out BuggIDToBeAddedToJira);
						break;
					}
				}
                
				var results = GetResults( FormData, BuggIDToBeAddedToJira );
				results.GenerationTime = LogTimer.GetElapsedSeconds().ToString( "F2" );
				return View( "Index", results );
			}
		}

	    /// <summary>
	    /// Return to the index view with a report for a specific branch
	    /// </summary>
	    /// <param name="ReportsForm"></param>
	    /// <returns></returns>
	    public ActionResult BranchReport(FormCollection ReportsForm)
	    {
            using (var logTimer = new FAutoScopedLogTimer(this.GetType().ToString(), bCreateNewLog: true))
            {
                var formData = new FormHelper(Request, ReportsForm, "JustReport");
                
                var buggIdToBeAddedToJira = 0;
                // Handle 'CopyToJira' button
                foreach (var Entry in ReportsForm)
                {
                    if (Entry.ToString().Contains("CopyToJira-"))
                    {
                        
                        int.TryParse(Entry.ToString().Substring("CopyToJira-".Length), out buggIdToBeAddedToJira);
                        break;
                    }
                }

                var results = GetResults(formData, buggIdToBeAddedToJira);
                results.GenerationTime = logTimer.GetElapsedSeconds().ToString("F2");
                return View("Index", results);
            }
        }

        /// <summary>
        /// Subscribe to a weekly email with a report for a specific branch over the last week.
        /// </summary>
        /// <returns>
        /// A partial view with a confirmation massge to be displayed in the source page.
        /// </returns>
        public PartialViewResult SubscribeToEmail(string branchName, string emailAddress)
        {
            var jobId = string.Format("{0}:{1}", branchName, emailAddress);

            RecurringJob.AddOrUpdate<ReportsController>(jobId, x => x.SendReportsEmail(emailAddress, branchName), Cron.Weekly);

            return PartialView("SignUpResponse", new EmailSubscriptionResponseModel () { Email = emailAddress, Branch = branchName } );
        }
        
        /// <summary>
        /// Unsubscribe from a weekly e-mail report for a branch
        /// </summary>
        /// <param name="jobId"></param>
        /// <returns></returns>
        public ActionResult UnsubscribeFromEmail(string jobId)
        {
            RecurringJob.RemoveIfExists(jobId);

            ViewData["BranchName"] = jobId.Split(':')[0];
            ViewData["EmailAddress"] = jobId.Split(':')[1];

            return View("Unsubscribe");
        }
        
        /// <summary>
        /// Private method called by hangfire to send individual report e-mails.
        /// </summary>
        /// <param name="emailAddress"></param>
        /// <param name="branchName"></param>
        public void SendReportsEmail(string emailAddress, string branchName)
        {
            var emailBody = string.Format("{0}" + Environment.NewLine + "{1}{2}%3A{3}",
                GetReportsEmailContents(branchName),
                "http://crashreporter.epicgames.net/Reports/UnsubscribeFromEmail?jobId=",
                branchName,
                emailAddress);
            var sMail = new SmtpClient();

            var message = new MailMessage("crashreporter@epicgames.com", emailAddress, "Weekly Crash Report : " + branchName, emailBody ) { IsBodyHtml = true };
            sMail.Send(message);
        }

        /// <summary>
        /// Test method - DELETE ME!
        /// </summary>
        public void GetJiraIssueSpec()
        {
            var jiraConnection = JiraConnection.Get();

            if (!jiraConnection.CanBeUsed())
                return;

            var jiraResponse = jiraConnection.JiraRequest(
                "/issue/createmeta?projectKeys=UE&issuetypeName=Bug&expand=projects.issuetypes.fields",
                JiraConnection.JiraMethod.GET, null, HttpStatusCode.OK);

            var projectSpec = JiraConnection.ParseJiraResponse(jiraResponse);

            var projects = ((ArrayList)projectSpec["projects"]);

            ArrayList issueTypes = null;

            foreach (var project in projects)
            {
                var kvp = (KeyValuePair<string, object>)project;
                if ( project == "issuetypes")
                {
                    issueTypes = kvp.Value as ArrayList;
                }

            }

            if (issueTypes != null)
            {
                var bug = issueTypes[0];


            }

            //var issuetypes = projdic["issuetypes"];

            //var issues = (Dictionary<string, object>) issuetypes;

            //var bug = (Dictionary<string, object>)issues.First().Value;

            //var fields = (Dictionary<string, object>)bug["fields"];
        }

        #endregion

        #region Private Methods
        
        private void AddBuggJiraMapping(Bugg NewBugg, ref HashSet<string> FoundJiras, ref Dictionary<string, List<Bugg>> JiraIDtoBugg)
        {
            string JiraID = NewBugg.TTPID;
            FoundJiras.Add("key = " + JiraID);

            bool bAdd = !JiraIDtoBugg.ContainsKey(JiraID);
            if (bAdd)
            {
                JiraIDtoBugg.Add(JiraID, new List<Bugg>());
            }

            JiraIDtoBugg[JiraID].Add(NewBugg);
        }

        /// <summary>
		/// Retrieve all Buggs matching the search criteria.
		/// </summary>
		/// <param name="FormData">The incoming form of search criteria from the client.</param>
		/// <param name="BuggIDToBeAddedToJira">ID of the bugg that will be added to JIRA</param>
		/// <returns>A view to display the filtered Buggs.</returns>
		private ReportsViewModel GetResults( FormHelper FormData, int BuggIDToBeAddedToJira )
		{
            var branchName = FormData.BranchName ?? "";
            var startDate = FormData.DateFrom;
            var endDate = FormData.DateTo;

            return GetResults(branchName, startDate, endDate, BuggIDToBeAddedToJira);
		}

	    /// <summary>
        /// Retrieve all Buggs matching the search criteria.
	    /// </summary>
	    /// <returns></returns>
        private ReportsViewModel GetResults(string branchName, DateTime startDate, DateTime endDate, int BuggIDToBeAddedToJira, bool sigbool)
	    {
	        // It would be great to have a CSV export of this as well with buggs ID being the key I can then use to join them :)
	        // 
	        // Enumerate JIRA projects if needed.
	        // https://jira.it.epicgames.net//rest/api/2/project
	        var JC = JiraConnection.Get();
	        var JiraComponents = JC.GetNameToComponents();
	        var JiraVersions = JC.GetNameToVersions();

	        using (var logTimer = new FAutoScopedLogTimer(this.GetType().ToString()))
	        {
	            const string anonymousGroup = "Anonymous";
                var anonymousGroupId = _unitOfWork.UserGroupRepository.First(data => data.Name == anonymousGroup).Id;
	            var anonymousIDs =
                    _unitOfWork.UserGroupRepository.First(data => data.Id == anonymousGroupId).Users.Select(data => data.Id).ToList();
	            var anonymousId = anonymousIDs.First();

	            var crashesQuery = _unitOfWork.CrashRepository.ListAll().Where(data => data.TimeOfCrash > startDate && data.TimeOfCrash <= endDate)
	                // Only crashes and asserts
	                .Where(crash => crash.CrashType == 1 || crash.CrashType == 2)
	                // Only anonymous user
	                .Where(crash => crash.UserNameId.Value == anonymousId);

	            // Filter by BranchName
	            if (!string.IsNullOrEmpty(branchName))
	            {
	                crashesQuery = crashesQuery.Where(data => data.Branch == branchName);
	            }

	            var crashes = crashesQuery.Select(data => new
	            {
                    ID = data.Id,
                    TimeOfCrash = data.TimeOfCrash.Value,
	                UserID = data.UserNameId,
                    BuildVersion = data.BuildVersion,
                    JIRA = data.Jira,
                    Platform = data.PlatformName,
                    FixCL = data.FixedChangeList,
                    BuiltFromCL = data.ChangeListVersion,
                    Pattern = data.Pattern,
                    MachineID = data.ComputerName,
                    Branch = data.Branch,
                    Description = data.Description,
                    RawCallStack = data.RawCallStack,
	            }).ToList();
                var numCrashes = crashesQuery.Count();

	            // Total # of ALL (Anonymous) crashes in timeframe
	            var totalAnonymousCrashes = numCrashes;

	            // Total # of UNIQUE (Anonymous) crashes in timeframe
                var uniquePatterns = new HashSet<string>();
                var uniqueMachines = new HashSet<string>();
	            var patternToCount = new Dictionary<string, int>();
                
	            foreach (var crash in crashes)
	            {
	                uniquePatterns.Add(crash.Pattern);
	                uniqueMachines.Add(crash.MachineID);

	                var bAdd = !patternToCount.ContainsKey(crash.Pattern);
	                if (bAdd)
	                {
	                    patternToCount.Add(crash.Pattern, 1);
	                }
	                else
	                {
	                    patternToCount[crash.Pattern]++;
	                }
	            }
	            var PatternToCountOrdered = patternToCount.OrderByDescending(X => X.Value).ToList();
	            const int NumTopRecords = 200;
	            var PatternAndCount = PatternToCountOrdered.Take(NumTopRecords).ToDictionary(x => x.Key, y => y.Value);

	            int TotalUniqueAnonymousCrashes = uniquePatterns.Count;

	            // Total # of AFFECTED USERS (Anonymous) in timeframe
	            int TotalAffectedUsers = uniqueMachines.Count;

	            var RealBuggs = _unitOfWork.BuggRepository.ListAll().Where(Bugg => PatternAndCount.Keys.Contains(Bugg.Pattern));

	            // Build search string.
	            var foundJiras = new HashSet<string>();
	            var jiraIDtoBugg = new Dictionary<string, List<Bugg>>();

	            var buggs = new List<Bugg>(NumTopRecords);
	            foreach (var Top in PatternAndCount)
	            {
	                var newBugg = RealBuggs.FirstOrDefault(X => X.Pattern == Top.Key);
	                if (newBugg != null)
	                {
                        var crashesForBugg = crashes.Where(Crash => Crash.Pattern == Top.Key).ToList();

	                    // Convert anonymous to full type;
	                    var fullCrashesForBugg = new List<Crash>(crashesForBugg.Count);
	                    foreach (var anon in crashesForBugg)
	                    {
	                        fullCrashesForBugg.Add(new Crash()
	                        {
	                            Id = anon.ID,
	                            TimeOfCrash = anon.TimeOfCrash,
	                            BuildVersion = anon.BuildVersion,
	                            Jira = anon.JIRA,
	                            PlatformName = anon.Platform,
	                            FixedChangeList = anon.FixCL,
	                            ChangeListVersion = anon.BuiltFromCL,
	                            Pattern = anon.Pattern,
	                            ComputerName = anon.MachineID,
	                            Branch = anon.Branch,
	                            Description = anon.Description,
	                            RawCallStack = anon.RawCallStack,
	                        });
	                    }

	                    newBugg.PrepareBuggForJira(fullCrashesForBugg);

	                    // Verify valid JiraID, this may be still a TTP 
	                    if (!string.IsNullOrEmpty(newBugg.TTPID))
	                    {
	                        int ttpid = 0;
	                        int.TryParse(newBugg.TTPID, out ttpid);

	                        if (ttpid == 0)
	                        {
	                            AddBuggJiraMapping(newBugg, ref foundJiras, ref jiraIDtoBugg);
	                        }
	                    }

	                    buggs.Add(newBugg);
	                }
	                else
	                {
	                    FLogger.Global.WriteEvent("Bugg for pattern " + Top.Key + " not found");
	                }
	            }

	            if (BuggIDToBeAddedToJira > 0)
	            {
                    var bugg = buggs.FirstOrDefault(x => x.Id == BuggIDToBeAddedToJira);
                    if (bugg != null)
                    {
                        bugg.CopyToJira();
                        AddBuggJiraMapping(bugg, ref foundJiras, ref jiraIDtoBugg);
                    }
	            }

	            if (JC.CanBeUsed())
	            {
	                var buggsCopy = new List<Bugg>(buggs);

	                var invalidJiras = new HashSet<string>();

	                // Simple verification of JIRA
	                foreach (var Value in foundJiras)
	                {
	                    if (Value.Length < 3 || !Value.Contains('-'))
	                    {
	                        invalidJiras.Add(Value);
	                    }
	                }

	                foreach (var invalidJira in invalidJiras)
	                {
	                    foundJiras.Remove(invalidJira);
	                }

	                // Grab the data form JIRA.
	                string JiraSearchQuery = string.Join(" OR ", foundJiras);
                    
                    bool bInvalid = false;
                    var jiraResults = new Dictionary<string, Dictionary<string, object>>();
	                try
	                {
	                    if (!string.IsNullOrWhiteSpace(JiraSearchQuery))
	                    {
	                        jiraResults = JC.SearchJiraTickets(
	                            JiraSearchQuery,
	                            new string[]
	                            {
	                                "key", // string
	                                "summary", // string
	                                "components", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                "resolution",
	                                //System.Collections.Generic.Dictionary`2[System.String,System.Object], name
	                                "fixVersions", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                "customfield_11200" // string
	                            });
	                    }
	                }
	                catch (System.Exception)
	                {
	                    bInvalid = true;
	                }

	                // Invalid records have been found, find the broken using the slow path.
	                if (bInvalid)
	                {
	                    foreach (var Query in foundJiras)
	                    {
	                        try
	                        {
	                            var TempResult = JC.SearchJiraTickets(
	                                Query,
	                                new string[]
	                                {
	                                    "key", // string
	                                    "summary", // string
	                                    "components", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                    "resolution",
	                                    // System.Collections.Generic.Dictionary`2[System.String,System.Object], name
	                                    "fixVersions", // System.Collections.ArrayList, Dictionary<string,object>, name
	                                    "customfield_11200" // string
	                                });

	                            foreach (var Temp in TempResult)
	                            {
	                                jiraResults.Add(Temp.Key, Temp.Value);
	                            }
	                        }
	                        catch (System.Exception)
	                        {

	                        }
	                    }
	                }

	                // Jira Key, Summary, Components, Resolution, Fix version, Fix changelist
	                foreach (var Jira in jiraResults)
	                {
	                    string JiraID = Jira.Key;

	                    string Summary = (string) Jira.Value["summary"];

	                    string ComponentsText = "";
	                    System.Collections.ArrayList Components =
	                        (System.Collections.ArrayList) Jira.Value["components"];
	                    foreach (Dictionary<string, object> Component in Components)
	                    {
	                        ComponentsText += (string) Component["name"];
	                        ComponentsText += " ";
	                    }

	                    Dictionary<string, object> ResolutionFields =
	                        (Dictionary<string, object>) Jira.Value["resolution"];
	                    string Resolution = ResolutionFields != null ? (string) ResolutionFields["name"] : "";

	                    string FixVersionsText = "";
	                    System.Collections.ArrayList FixVersions =
	                        (System.Collections.ArrayList) Jira.Value["fixVersions"];
	                    foreach (Dictionary<string, object> FixVersion in FixVersions)
	                    {
	                        FixVersionsText += (string) FixVersion["name"];
	                        FixVersionsText += " ";
	                    }

	                    int FixCL = Jira.Value["customfield_11200"] != null
	                        ? (int) (decimal) Jira.Value["customfield_11200"]
	                        : 0;

	                    List<Bugg> BuggsForJira;
	                    jiraIDtoBugg.TryGetValue(JiraID, out BuggsForJira);

	                    //var BuggsForJira = JiraIDtoBugg[JiraID];

	                    if (BuggsForJira != null)
	                    {
	                        foreach (Bugg Bugg in BuggsForJira)
	                        {
	                            Bugg.JiraSummary = Summary;
	                            Bugg.JiraComponentsText = ComponentsText;
	                            Bugg.JiraResolution = Resolution;
	                            Bugg.JiraFixVersionsText = FixVersionsText;
	                            if (FixCL != 0)
	                            {
	                                Bugg.JiraFixCL = FixCL.ToString();
	                            }

	                            buggsCopy.Remove(Bugg);
	                        }
	                    }
	                }

	                // If there are buggs, we need to update the summary to indicate an error.
	                // Usually caused when bugg's project has changed.
	                foreach (var Bugg in buggsCopy.Where(b => !string.IsNullOrEmpty(b.TTPID)))
	                {
	                    Bugg.JiraSummary = "JIRA MISMATCH";
	                    Bugg.JiraComponentsText = "JIRA MISMATCH";
	                    Bugg.JiraResolution = "JIRA MISMATCH";
	                    Bugg.JiraFixVersionsText = "JIRA MISMATCH";
	                    Bugg.JiraFixCL = "JIRA MISMATCH";
	                }
	            }

	            buggs = buggs.OrderByDescending(b => b.CrashesInTimeFrameGroup).ToList();

	            return new ReportsViewModel
	            {
	                Buggs = buggs,
                    BranchName = branchName,
	                BranchNames = _unitOfWork.CrashRepository.GetBranchesAsListItems(),
	                DateFrom = (long) (startDate - CrashesViewModel.Epoch).TotalMilliseconds,
	                DateTo = (long) (endDate - CrashesViewModel.Epoch).TotalMilliseconds,
	                TotalAffectedUsers = TotalAffectedUsers,
	                TotalAnonymousCrashes = totalAnonymousCrashes,
	                TotalUniqueAnonymousCrashes = TotalUniqueAnonymousCrashes
	            };
	        }
	    }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="branchName"></param>
        /// <param name="startDate"></param>
        /// <param name="endDate"></param>
        /// <returns></returns>
        private ReportsViewModel GetResults(string branchName, DateTime startDate, DateTime endDate, int BuggIDToBeAddedToJira)
        {
            const string anonymousGroup = "Anonymous";
            var anonymousGroupId = _unitOfWork.UserGroupRepository.First(data => data.Name == anonymousGroup).Id;
            var anonymousIDs =
                _unitOfWork.UserGroupRepository.First(data => data.Id == anonymousGroupId).Users.Select(data => data.Id).ToList();
            var anonymousId = anonymousIDs.First();

            var crashesQuery = _unitOfWork.CrashRepository.ListAll()
                .Where(data => data.TimeOfCrash > startDate && data.TimeOfCrash <= endDate)
                // Only crashes and asserts
                .Where(crash => crash.CrashType == 1 || crash.CrashType == 2);
                // Only anonymous user
                    //.Where(crash => crash.UserNameId.Value == anonymousId);

            // Filter by BranchName
            if (!string.IsNullOrEmpty(branchName))
            {
                crashesQuery = crashesQuery.Where(data => data.Branch == branchName);
            }

            var crashesList = crashesQuery.ToList();

            var buggsList = crashesQuery.Where(data => data.Bugg != null).GroupBy(data => data.Bugg).Select(data => data.Key).ToList();

            foreach (var bugg in buggsList)
            {
                if (bugg == null)
                    continue;

                var crashes = crashesList.Where(data => data.BuggId == bugg.Id).ToList();

                bugg.PrepareBuggForJira(crashes);

                bugg.CrashesInTimeFrameGroup = crashesList.Count(data => data.BuggId == bugg.Id);
                bugg.AffectedVersions = new SortedSet<string>(crashes.Select(data => data.BuildVersion));
                bugg.NumberOfUniqueMachines = crashes.GroupBy(data => data.ComputerName).Count();

                //get latest changelist
                int latestClAffected;
                int.TryParse(crashes.Max(data => data.ChangeListVersion), out latestClAffected);
                bugg.LatestCLAffected = latestClAffected;

                if (string.IsNullOrWhiteSpace(bugg.TTPID))
                {
                    bugg.JiraSummary = "JIRA MISMATCH";
                    bugg.JiraComponentsText = "JIRA MISMATCH";
                    bugg.JiraResolution = "JIRA MISMATCH";
                    bugg.JiraFixVersionsText = "JIRA MISMATCH";
                    bugg.JiraFixCL = "JIRA MISMATCH";
                }
            }

            //Get jira connection
            var jiraConnection = JiraConnection.Get();

            if (BuggIDToBeAddedToJira > 0)
            {
                var bugg = buggsList.FirstOrDefault(x => x.Id == BuggIDToBeAddedToJira);
                if (bugg != null)
                {
                    bugg.CopyToJira();
                }
            }

            var jiraResults = new Dictionary<string, Dictionary<string, object>>();

            foreach (var Query in buggsList.Where(data =>
                !string.IsNullOrWhiteSpace(data.TTPID)).Select(data => data.TTPID))
            {
                try
                {
                    var TempResult = jiraConnection.SearchJiraTickets(
                        "key = " + Query,
                        new string[]
	                    {
	                        "key", // string
	                        "summary", // string
	                        "components", // System.Collections.ArrayList, Dictionary<string,object>, name
	                        "resolution",
	                        // System.Collections.Generic.Dictionary`2[System.String,System.Object], name
	                        "fixVersions", // System.Collections.ArrayList, Dictionary<string,object>, name
	                        "customfield_11200" // string
	                    });

                    foreach (var Temp in TempResult)
                    {
                        jiraResults.Add(Temp.Key, Temp.Value);
                    }
                }
                catch (System.Exception)
                {

                }
            }

            // Jira Key, Summary, Components, Resolution, Fix version, Fix changelist
            foreach (var Jira in jiraResults)
            {
                var jiraId = Jira.Key;

                var summary = (string)Jira.Value["summary"];

                var ComponentsText = "";
                System.Collections.ArrayList Components =
                    (System.Collections.ArrayList)Jira.Value["components"];
                foreach (Dictionary<string, object> Component in Components)
                {
                    ComponentsText += (string)Component["name"];
                    ComponentsText += " ";
                }

                Dictionary<string, object> ResolutionFields =
                    (Dictionary<string, object>)Jira.Value["resolution"];
                string Resolution = ResolutionFields != null ? (string)ResolutionFields["name"] : "";

                string FixVersionsText = "";
                System.Collections.ArrayList FixVersions =
                    (System.Collections.ArrayList)Jira.Value["fixVersions"];
                foreach (Dictionary<string, object> FixVersion in FixVersions)
                {
                    FixVersionsText += (string)FixVersion["name"];
                    FixVersionsText += " ";
                }

                int FixCL = Jira.Value["customfield_11200"] != null
                    ? (int)(decimal)Jira.Value["customfield_11200"]
                    : 0;


                if (buggsList.Any(data => data.TTPID == jiraId))
                {
                    var bugg = buggsList.First(data => data.TTPID == jiraId);
                    bugg.JiraSummary = summary;
                    bugg.JiraComponentsText = ComponentsText;
                    bugg.JiraResolution = Resolution;
                    bugg.JiraFixVersionsText = FixVersionsText;
                    if (FixCL != 0)
                    {
                        bugg.JiraFixCL = FixCL.ToString();
                    }
                }
            }

            return new ReportsViewModel
            {
                Buggs = buggsList.OrderByDescending(data => data.CrashesInTimeFrameGroup).ToList(),
                BranchName = branchName,
                BranchNames = _unitOfWork.CrashRepository.GetBranchesAsListItems(),
                DateFrom = (long)(startDate - CrashesViewModel.Epoch).TotalMilliseconds,
                DateTo = (long)(endDate - CrashesViewModel.Epoch).TotalMilliseconds,
                TotalAffectedUsers = crashesQuery.GroupBy(data => data.ComputerName).Count(),
                TotalAnonymousCrashes = crashesQuery.Count(),
                TotalUniqueAnonymousCrashes = crashesQuery.GroupBy(data => data.BuggId).Count()
            };
        }

        /// <summary>
        /// Private method to get reports page for a branch and render to an HTML string
        /// </summary>
        /// <returns>HTML formatted string</returns>
	    private string GetReportsEmailContents(string branchName)
	    {
            var reportViewModel = GetResults(branchName, DateTime.Now.AddDays(-7), DateTime.Now, 0);
            return RenderViewToString("Reports", "~/Views/Reports/ViewReports.cshtml", reportViewModel);
	    }

        /// <summary>
        /// Uses a "fake" HTTPContext to generate html output from a view or partial view.
        /// 
        /// Essentially this function returns view data at any time from any part of the program 
        /// without having to call into a view from a normal controller context.
        /// This is important to allow us to return view data asynchronously.
        /// </summary>
        /// <param name="controllerName">A string indicating the name of the controller that normally calls into a view</param>
        /// <param name="viewName">the path to the view in quiestion</param>
        /// <param name="viewData">the model to be passed to the view</param>
        /// <returns>HTML formatted string</returns>
        private static string RenderViewToString(string controllerName, string viewName, object viewData)
        {
            using (var writer = new StringWriter())
            {
                var routeData = new RouteData();
                routeData.Values.Add("controller", controllerName);
                var fakeControllerContext = new ControllerContext(new HttpContextWrapper(new HttpContext(new HttpRequest(null, "http://google.com", null), new HttpResponse(null))), routeData, new FakeController());
                var razorViewResult = ViewEngines.Engines.FindPartialView(fakeControllerContext, viewName);

                var viewContext = new ViewContext(fakeControllerContext, razorViewResult.View, new ViewDataDictionary(viewData), new TempDataDictionary(), writer);
                razorViewResult.View.Render(viewContext, writer);
                return writer.ToString();
            }
        }

        protected override void Dispose(bool disposing)
        {
            _unitOfWork.Dispose();
            base.Dispose(disposing);
        }

        /// <summary>
        /// Get a list of bugs within a specified date range
        /// </summary>
        /// <param name="results"></param>
        /// <param name="dateFrom"></param>
        /// <param name="dateTo"></param>
        /// <returns></returns>
        public IQueryable<Bugg> FilterByDate(IQueryable<Bugg> results, DateTime dateFrom, DateTime dateTo)
        {
            dateTo = dateTo.AddDays(1);
            var buggsInTimeFrame =
                results.Where(bugg => (bugg.TimeOfFirstCrash >= dateFrom && bugg.TimeOfFirstCrash <= dateTo) ||
                    (bugg.TimeOfLastCrash >= dateFrom && bugg.TimeOfLastCrash <= dateTo) || (bugg.TimeOfFirstCrash <= dateFrom && bugg.TimeOfLastCrash >= dateTo));

            return buggsInTimeFrame;
        }

        #endregion
    }

    /// <summary>
    /// Empty Controller class used as proxy 
    /// </summary>
    class FakeController : ControllerBase
    {
        protected override void ExecuteCore()
        {
        }
    }
}
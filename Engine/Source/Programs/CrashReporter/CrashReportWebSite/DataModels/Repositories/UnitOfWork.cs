﻿// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    /// <summary>
    /// Public interface for the unit of work entity framework management class
    /// </summary>
    public interface IUnitOfWork : IDisposable
    {
        ICrashRepository CrashRepository { get; }
        IBuggRepository BuggRepository { get; }
        IFunctionRepository FunctionRepository { get; }
        IUserRepository UserRepository { get; }
        ICallstackRepository CallstackRepository { get; }
        IUserGroupRepository UserGroupRepository { get; }
        IErrorMessageRepository ErrorMessageRepository { get; }
        void Save();
        void Dispose();
    }

    /// <summary>
    /// Container for entity framework management. 
    /// </summary>
    public class UnitOfWork: IUnitOfWork
    {
        #region Attributes

        private bool _disposed = false;
        private CrashReportEntities _entityContext;
        private ICrashRepository _CrashRepository;
        private IBuggRepository _buggRepository;
        private IFunctionRepository _functionRepository;
        private IUserRepository _userRepository;
        private ICallstackRepository _callstackRepository;
        private IUserGroupRepository _userGroupRepository;
        private IErrorMessageRepository _errorMessageRepository;

        #endregion

        #region Properties

        /// <summary>
        /// 
        /// </summary>
        public ICrashRepository CrashRepository
        {
            get 
            {
                if (this._CrashRepository == null)
                {
                    _CrashRepository = new CrashRepository(_entityContext);
                } 
                return _CrashRepository;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public IBuggRepository BuggRepository
        {
            get
            {
                if (this._buggRepository == null)
                {
                    _buggRepository = new BuggRepository(_entityContext);
                }
                return _buggRepository;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public IUserRepository UserRepository
        {
            get
            {
                if (this._userRepository == null)
                {
                    _userRepository = new UserRepository(_entityContext);
                }
                return _userRepository;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public ICallstackRepository CallstackRepository
        {
            get
            {
                if (this._callstackRepository == null)
                {
                    _callstackRepository = new CallStackRepository(_entityContext);
                }
                return _callstackRepository;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public IFunctionRepository FunctionRepository
        {
            get
            {
                if (this._functionRepository == null)
                {
                    _functionRepository = new FunctionRepository(_entityContext);
                }
                return _functionRepository;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public IUserGroupRepository UserGroupRepository
        {
            get
            {
                if (this._userGroupRepository == null)
                {
                    _userGroupRepository = new UserGroupRepository(_entityContext);
                }
                return _userGroupRepository;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        public IErrorMessageRepository ErrorMessageRepository
        {
            get
            {
                if (this._errorMessageRepository == null)
                {
                    _errorMessageRepository = new ErrorMessageRepository(_entityContext);
                }
                return _errorMessageRepository;
            }
        }

        #endregion

        #region Methods

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">An entity context object</param>
        public UnitOfWork(CrashReportEntities entityContext)
        {
            this._entityContext = entityContext;
            _entityContext.Database.CommandTimeout = 120;
        }

        #region Public Methods

        /// <summary>
        /// Commit all pending updates.
        /// </summary>
        public void Save()
        {
            _entityContext.SaveChanges();
        }

        /// <summary>
        /// 
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
        }

        #endregion

        #region Private Methods

        /// <summary>
        /// 
        /// </summary>
        /// <param name="disposing"></param>
        protected virtual void Dispose(bool disposing)
        {
            _entityContext.Database.Connection.Close();
            _CrashRepository = null;
            _buggRepository = null;
            _functionRepository = null;
            _userRepository = null;
            _callstackRepository = null;
            _userGroupRepository = null;
            _errorMessageRepository = null;
            _entityContext.Dispose();
            _entityContext = null;
        }

        #endregion

        #endregion
    }
}
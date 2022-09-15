/*
 * 	Copyright � 2019-2022, Renee Koecher
 * 	All rights reserved.
 * 
 * 	Redistribution and use in source and binary forms, with or without
 * 	modification, are permitted provided that the following conditions are met :
 * 
 * 	* Redistributions of source code must retain the above copyright
 * 	  notice, this list of conditions and the following disclaimer.
 * 	* Redistributions in binary form must reproduce the above copyright
 * 	  notice, this list of conditions and the following disclaimer in the
 * 	  documentation and/or other materials provided with the distribution.
 * 	* Neither the name of XIPivot nor the
 * 	  names of its contributors may be used to endorse or promote products
 * 	  derived from this software without specific prior written permission.
 * 
 * 	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * 	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * 	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * 	DISCLAIMED.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * 	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * 	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * 	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * 	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * 	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * 	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LogProvider.h"

#include <Windows.h>

#include <unordered_map>
#include <vector>
#include <string>

namespace XiPivot
{
	namespace Core
	{
		/* basically all the heavy lifting and actual syscall redirects happen in here */
		class Redirector
		{
			/* signature types for the Win32 API methods we're going to hook */

			typedef HANDLE(WINAPI * pFnCreateFileA)(
				LPCSTR                lpFileName,
				DWORD                 dwDesiredAccess,
				DWORD                 dwShareMode,
				LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				DWORD                 dwCreationDisposition,
				DWORD                 dwFlagsAndAttributes,
				HANDLE                hTemplateFile
				);

			typedef HANDLE(WINAPI* pFnCreateFileW)(
				LPCWSTR               lpFileName,
				DWORD                 dwDesiredAccess,
				DWORD                 dwShareMode,
				LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				DWORD                 dwCreationDisposition,
				DWORD                 dwFlagsAndAttributes,
				HANDLE                hTemplateFile
				);

			typedef HANDLE(WINAPI * pFnFindFirstFileA)(
				LPCSTR             lpFileName,
				LPWIN32_FIND_DATAA lpFindFileData
				);

			template<typename T>
			struct Paths {
				size_t   (*strlen)(const T*);
				const T* (*strstr)(const T*, const T*);
				const T* rom1;
				const T* rom2;
				const T* win;
				const T* win_se;
				const T* win_se_data;
				const T* win_music;
				const T* win_music_data;
			};

		public:
			virtual ~Redirector(void);


			/* setup and tear-down of syscall hooks*/
			bool setupHooks(void);
			bool releaseHooks(void);

			bool hooksActive(void) const { return m_hooksSet; };

			/* change the active log provider reopining the log in the process */
			void setLogProvider(ILogProvider *logProvider);

			/* toggle debug logging on/off */
			void setDebugLog(bool state);

			/* get the current state for debug logging */
			bool getDebugLog(void) const { return m_logDebug != ILogProvider::LogLevel::Discard; }

			/* setup or change the base directory used to search for overlays
			 * initially this will be set to the processes current working directory
			 *
			 * NOTE: *this triggers a re-scan of all overlays*
			 * NOTE: *use scarcely or before adding overlays*
			 */
			void setRootPath(const std::string &newRoot);

			const std::string& rootPath(void) const { return m_rootPath; }

			/* add a new directory overlay to the back of the priority list */
			bool addOverlay(const std::string &overlayPath);

			/* remove any previously added overlay from the list
			*
			 * NOTE: *this triggers a re-scan of all other overlays*
			 * NOTE: *use this scarcely if at all*
			 */
			void removeOverlay(const std::string &overlayPath);

			const std::vector<std::string> &overlayList(void) const { return m_overlayPaths; };

		public:
			/* access or create the actual Redirector instance */
			static Redirector& instance(void);

			template <class T>
			static T* instance(void) 
			{
				static_assert(std::is_base_of<Redirector, T>(), "Templated ::instance requires T to be derived from Core::Redirector");

				if (Redirector::s_instance == nullptr)
				{
					Redirector::s_instance = new T();
				}
				return reinterpret_cast<T*>(Redirector::s_instance);
			}

			/* static callbacks used by the Detours library */
			static HANDLE __stdcall dCreateFileA(LPCSTR a0, DWORD a1, DWORD a2, LPSECURITY_ATTRIBUTES a3, DWORD a4, DWORD a5, HANDLE a6);
			static HANDLE __stdcall dCreateFileW(LPCWSTR a0, DWORD a1, DWORD a2, LPSECURITY_ATTRIBUTES a3, DWORD a4, DWORD a5, HANDLE a6);
			static HANDLE __stdcall dFindFirstFileA(LPCSTR a0, LPWIN32_FIND_DATAA a2);

		private /* static */:

			static pFnCreateFileA s_procCreateFileA;
			static pFnCreateFileW s_procCreateFileW;
			static pFnFindFirstFileA s_procFindFirstFileA;

			static Paths<char>    s_pathsA;
			static Paths<wchar_t> s_pathsW;

		protected:
			/* globally unique instance pointer */
			static Redirector* s_instance;

			/* it would be foolish to allow more than one Redirector to be created,
			   as such this whole class is designed as a Singleton.
			 */
			explicit Redirector(void);

		private:
			/* actual code to handle the intercept / redirect of file names */
			HANDLE __stdcall interceptCreateFileA(LPCSTR a0, DWORD a1, DWORD a2, LPSECURITY_ATTRIBUTES a3, DWORD a4, DWORD a5, HANDLE a6);
			HANDLE __stdcall interceptCreateFileW(LPCWSTR a0, DWORD a1, DWORD a2, LPSECURITY_ATTRIBUTES a3, DWORD a4, DWORD a5, HANDLE a6);
			HANDLE __stdcall interceptFindFirstFileA(LPCSTR a0, LPWIN32_FIND_DATAA a2);

			template<typename T>
			const char* findRedirect(const Paths<T>& paths, const T* realPath, int32_t& outPathKey);

			/* first-time scan of overlay directories - basically "find all dat paths and record them" */
			bool scanOverlayPath(const std::string &overlayPath);

			bool collectSubPath(const std::string &basePath, const std::string &pattern, std::vector<std::string> &result, bool doubleDirSep = false);
			bool collectSubPath(const std::string &basePath, const std::string &midPath, const std::string &pattern, std::vector<std::string> &result, bool doubleDirSep = false);

			bool collectDataFiles(const std::string &parentPath, const std::string &pattern, std::vector<std::string> &result);
			bool collectDataFiles(const std::string &parentPath, const std::string &midPath, const std::string &pattern, std::vector<std::string> &result);

			/* an actual 32bit integer perfect hash for XI ROM paths >:3 */
			template<typename T>
			int32_t pathToIndex(const T *romPath);
			/* and the same for sound / music files */
			template<typename T>
			int32_t pathToIndexAudio(const Paths<T> &paths, const T *soundPath);


			bool                                     m_hooksSet;

			std::string                              m_rootPath;
			std::vector<std::string>                 m_overlayPaths;
			std::unordered_map<int32_t, std::string> m_resolvedPaths;

			ILogProvider::LogLevel                   m_logDebug;
			ILogProvider*                            m_logger;
		};
	}
}


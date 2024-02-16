/*
 * Copyright 2011-2022 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ARX_PLATFORM_ENVIRONMENT_H
#define ARX_PLATFORM_ENVIRONMENT_H

#include <functional>
#include <limits>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>
#include <stddef.h>
#include <map>

#include "boost/lexical_cast.hpp"

#include "io/log/Logger.h"
#include "platform/Platform.h"
#include "util/String.h"

namespace fs { class path; }

namespace platform {

/*!
 * \brief Initialize envirenment functions
 *
 * \param argv0 a path to use for \ref getExecutablePath if no OS-specific function is
 *              available to determine the executable path. Will be ignored if it doesn't
 *              contain any slashes.
 */
void initializeEnvironment(const char * argv0);

/*!
 * \brief Expand a string containing environment variables
 *
 * Expansion is done as it would be in the system's shell.
 *
 * \param in the string to expand.
 */
std::string expandEnvironmentVariables(std::string_view in);

enum SystemPathId {
	NoPath,
	UserDirPrefixes //!< Directories under which to store per-user data.
};

/*!
 * \brief Get a standard system path
 *
 * This currently does nothing on non-Windows systems.
 *
 * \return the path(s) or an empty string if the path is not known.
 */
std::vector<fs::path> getSystemPaths(SystemPathId id);

/*!
 * \brief Get a Windows registry entry
 *
 * This does nothing on non-Windows systems.
 *
 * \param name path of the registry entry to read. This is looked up under HKCU first, and
 *             if it doesn't exist there, under HKLM.
 *
 * \return the requested registry key or std::nullopt if it does not exist.
 */
std::optional<std::string> getSystemConfiguration(std::string_view name);

/*!
 * \brief Get the path to the current running executable
 *
 * \return the executable path if possible or an empty string otherwise
 */
fs::path getExecutablePath();

/*!
 * \brief Get the name the executable was invoked as
 *
 * \return the executable name if possible or an empty string otherwise
 */
std::string getCommandName();

/*!
 * \brief Get the full path to a helper executable
 *
 * Tries to find a helper executable in the same directory as the current program, in the
 * parent directory, or in the libexec directory in the prefix where arx is installed.
 * If found, returns a full path to the executable.
 * Otherwise, returns a relative path containing only the executable name.
 *
 * \return a path or name suitable for CreateProcess(), exec*p() or system() calls.
 */
fs::path getHelperExecutable(std::string_view name);

#if ARX_PLATFORM != ARX_PLATFORM_WIN32
constexpr const char env_list_seperator = ':';
#else
constexpr const char env_list_seperator = ';';
#endif

/*!
 * \brief Check if a file descriptor has been closed or redirected to /dev/null
 *
 * \param fd the file descriptor to test - 0 for stdin, 1 for stdout and 2 for stderr.
 */
bool isFileDescriptorDisabled(int fd);

//! Check if standard input is open and doesn't point to /dev/null
inline bool hasStdIn() { return !isFileDescriptorDisabled(0); }

//! Check if standard output is open and doesn't point to /dev/null
inline bool hasStdOut() { return !isFileDescriptorDisabled(1); }

//! Check if standard error is open and doesn't point to /dev/null
inline bool hasStdErr() { return !isFileDescriptorDisabled(2); }

//! Check if an environment variable is set
bool hasEnvironmentVariable(const char * name);

//! Set an environment variable, overriding any existing values
void setEnvironmentVariable(const char * name, const char * value);

//! Unset an environment variable
void unsetEnvironmentVariable(const char * name);

class EnvRegex {
	
	friend class EnvVar;
	
	std::regex * re;
	std::string strRegex;
	std::string strMsg;
	
public:

	EnvRegex() { }
	EnvRegex(std::string _strRegex) { setRegex(_strRegex); } // no log is safer as default
	
	bool isSet();
	bool matchRegex(std::string data);
	bool setRegex(std::string strRE);
	std::string getRegex() { return strRegex; }
	std::string getMsg() {return strMsg;}
};

class EVHnoLog { // prevents __gnu_cxx::recursive_init_error when using ex.: LogDebug LogInfo .., while still being able to create log msgs from here.
	friend class EnvVarHandler;
	friend class EnvRegex;
	inline static bool allowLog = true;
public:
	EVHnoLog() { allowLog = false; }
	~EVHnoLog() { allowLog = true; } // off until lambda returns
	EVHnoLog & set(bool b) { allowLog = b; return *this; }
};

#define evh_CreateSHnm(NAME, ...) static platform::EnvVarHandler * NAME = platform::EnvVarHandler::create(__VA_ARGS__)
#define evh_CreateSH(...) evh_CreateSHnm(evh, __VA_ARGS__) // create static var and function header, easy to use inside lambda
#define evh_CreateSHnoLog(...) platform::EVHnoLog evhTurnOffLogMessagesForEnvVarsTilReturn; evh_CreateSH(__VA_ARGS__)
#define evh_Create(...) platform::EnvVarHandler::create(__VA_ARGS__)

class EnvVarHandler {
private:
	class EnvVarData {
	public:
		char evtD = '.';
		std::string strIdD;
		
		std::string evS;
		int evI;
		float evF;
		bool evB;
		
		EnvVarData() : evtD('.'), strIdD(""), evS(""), evI(0), evF(0.f), evB(false) { }
		//EnvVarData(const EnvVarData & o) : evtD(o.evtD), strIdD(o.strIdD), evS(o.evS), evI(o.evI), evF(o.evF), evB(o.evB) { }
		
		bool operator!=(EnvVarData & other) {
			//arx_assert_msg(evtD != '~' && other.evtD != '~', "invalid this %c, other %c", evtD, other.evtD);
			switch(evtD) {
				case 'S': return evS != other.evS;
				case 'I': return evI != other.evI;
				case 'F': return evF != other.evF;
				case 'B': return evB != other.evB;
				default: arx_assert_msg(false, "type not set %s", strIdD.c_str());
			}
		}
	};
	
	char evtH = '.';
	std::string strId;
	
	EnvVarData evbDefault;
	EnvVarData evbCurrent;
	EnvVarData evbOld;
	EnvVarData evbMin;
	EnvVarData evbMax;
	
	std::string strEVB;
	std::string msg;
	
	std::function<void()> funcConvert;
	bool hasInternalConverter;
	
	EnvVarHandler & copyRawFrom(const EnvVarHandler & evCopyFrom) { // keep here, keep close to vars, keep 100% simple
		evtH = evCopyFrom.evtH;
		strId = evCopyFrom.strId;
		
		evbDefault = evCopyFrom.evbDefault;
		evbCurrent = evCopyFrom.evbCurrent;
		evbOld = evCopyFrom.evbOld;
		evbMin = evCopyFrom.evbMin;
		evbMax = evCopyFrom.evbMax;
		
		strEVB = evCopyFrom.msg;
		msg = evCopyFrom.msg;
		
		funcConvert = evCopyFrom.funcConvert;
		hasInternalConverter = evCopyFrom.hasInternalConverter;
		
		return *this;
	}
	EnvVarHandler & copyFrom(const EnvVarHandler & evCopyFrom);
	
	inline static std::map<std::string, EnvVarHandler*> vEVH;
	
	void initEnvVar(char _evtH, std::string _strId, std::string _msg, bool _hasInternalConverter);
	void fixMinMax();
	
	EnvVarHandler & setCommon();
	
	static EnvVarHandler * addToList(std::string _id, EnvVarHandler * evh);
	
	EnvVarHandler() {
		evtH=('.');
		hasInternalConverter=(false);
	}
	EnvVarHandler(EnvVarHandler & evCopyFrom) {
		EnvVarHandler();
		copyFrom(evCopyFrom);
	}
	~EnvVarHandler() { strId += "(C++:DESTRUCTED)"; }
	
public:
	
	/**
	 * The converter is called whenever the variable is modified from anywhere.
	 * To grant the conversion will only happen in a specific moment, use isModified() and clearModified() instead.
	 */
	EnvVarHandler * setConverter(auto func) {
		funcConvert = std::move(func);
		// funcConvert();
		hasInternalConverter = true;
		return this;
	}
	
	bool isModified() { return evbCurrent != evbOld; }
	EnvVarHandler * clearModified() { evbOld = evbCurrent; return this; }
	
	// creators for base types. here also: get.() and set.(val)
	#define EnvVarHandlerEasySimpleCode(TYPE,SUFFIX,MIN,MAX) \
		static EnvVarHandler * create(std::string _strId, std::string _msg, TYPE val, TYPE min = MIN, TYPE max = MAX) { \
			if(vEVH.contains(_strId)) return nullptr; \
			EnvVarHandler * evh = new EnvVarHandler(); \
			evh->evbDefault.ev##SUFFIX = val; \
			evh->evbCurrent.ev##SUFFIX = val; \
			evh->evbOld.ev##SUFFIX = val; \
			evh->evbMin.ev##SUFFIX = min; \
			evh->evbMax.ev##SUFFIX = max; \
			evh->initEnvVar(std::string(#SUFFIX)[0], _strId, _msg, false); \
			addToList(_strId, evh); \
			return evh; \
		} \
		TYPE get##SUFFIX() { \
			arx_assert_msg(std::string(#SUFFIX) != std::string({evtH, '\0'}), "requested %s but is %c", #SUFFIX, evtH); \
			return evbCurrent.ev##SUFFIX; \
		} \
		EnvVarHandler & set##SUFFIX(TYPE val) { \
			evbCurrent.ev##SUFFIX = val; \
			return setCommon(); \
		}
	EnvVarHandlerEasySimpleCode(std::string , S, "", "")
	EnvVarHandlerEasySimpleCode(int         , I, std::numeric_limits<int>::min(),   std::numeric_limits<int>::max())
	EnvVarHandlerEasySimpleCode(float       , F, std::numeric_limits<float>::min(), std::numeric_limits<float>::max())
	EnvVarHandlerEasySimpleCode(bool        , B, false, false)
	static EnvVarHandler * create(std::string _strId, std::string _msg, const char * val) { return create(_strId, _msg, std::string(val)); } // this is important! otherwise a const char val would call the boolean overload!!
	
	EnvVarHandler & operator=(const EnvVarHandler & evCopyFrom);
	
	std::string id() { return strId; }
	
	std::string toString();
	bool toBool();
	int toInt();
	float toFloat();
	EnvVarHandler * setAuto(std::string _strEVB);
	std::string getDescription() { return msg; }
	EnvVarHandler * reset() {
		if(EVHnoLog::allowLog) LogInfo << strId << " reset to " << toString();
		evbCurrent = evbDefault;
		return this;
	}
	std::string getMinMaxInfo();
	
	static EnvVarHandler * getEVH(std::string _id);
	static void getEnvVarHandlerList(bool bListAsEnvVar, bool bListShowDescription);
	inline static const char* validIdChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
};

struct EnvironmentOverride {
	
	const char * name;
	const char * value;
	
};

/*!
 * \brief Lock around library functions that access the environment
 *
 * This helper allows temporarily setting environment variables that change
 * the behavior of library functions.
 */
class EnvironmentLock {
	
	EnvironmentOverride * const m_overrides;
	const size_t m_count;
	
	void lock();
	void unlock();
	
public:
	
	EnvironmentLock(const EnvironmentLock &) = delete;
	EnvironmentLock & operator=(const EnvironmentLock &) = delete;
	
	EnvironmentLock()
		: m_overrides(nullptr)
		, m_count(0)
	{ lock(); }
	
	template <size_t N>
	explicit EnvironmentLock(EnvironmentOverride (&overrides)[N])
		: m_overrides(overrides)
		, m_count(N)
	{ lock(); }
	
	~EnvironmentLock() { unlock(); }
	
};

// Return the user's preferred languages in RFC 4646 format
std::vector<std::string> getPreferredLocales();

} // namespace platform

#endif // ARX_PLATFORM_ENVIRONMENT_H

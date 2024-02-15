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
	
	bool isSet();
	bool matchRegex(std::string data);
	bool setRegex(std::string strRE, bool allowLog);
	std::string getRegex() { return strRegex; }
	std::string getMsg() {return strMsg;}
};

class EnvVarHandler { // useful to take action only when the envvar is modified dinamically
private:
	class EnvVarData {
	public:
		EnvVarData() { evtD=('.'); evS=(""); evI=(0); evF=(0.f); evB=(false); }
		
		char evtD = '.';
		std::string strIdD;
		
		std::string evS;
		int evI;
		float evF;
		bool evB;
		
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
	
	EnvVarData evbCurrent;
	EnvVarData evbOld;
	EnvVarData evbMin;
	EnvVarData evbMax;
	
	std::string strEVB;
	std::string msg;
	
	std::function<void()> funcConvert;
	bool hasInternalConverter;
	
	bool bJustToCopyFrom = false; // no copyRawFrom
	EnvVarHandler * targetCopyTo; // no copyRawFrom
	
	EnvVarHandler & copyRawFrom(const EnvVarHandler & evCopyFrom) { // keep here, keep close to vars, keep 100% simple
		evtH = evCopyFrom.evtH;
		strId = evCopyFrom.strId;
		
		evbCurrent = evCopyFrom.evbCurrent;
		evbOld = evCopyFrom.evbOld;
		evbMin = evCopyFrom.evbMin;
		evbMax = evCopyFrom.evbMax;
		
		strEVB = evCopyFrom.msg;
		msg = evCopyFrom.msg;
		
		funcConvert = evCopyFrom.funcConvert;
		hasInternalConverter = evCopyFrom.hasInternalConverter;
		
//	bJustToCopyFrom = false;
//	targetCopyTo = nullptr;
		
		return *this;
	}
	void copyFrom(const EnvVarHandler & evCopyFrom);
	
	inline static std::map<std::string, EnvVarHandler*> vEVH;
	
	void initTmpInstanceAndReadEnvVar(EnvVarHandler * _targetCopyTo, char _evtH, std::string _strId, std::string _msg, bool _hasInternalConverter, bool _bJustToCopyFrom);
	void fixMinMax();
	
	EnvVarHandler & setCommon();
	
	static bool addToList(std::string strId, EnvVarHandler * evh);
	
	void Genesis(EnvVarHandler * evAdam, EnvVarHandler * evEve);
	
public:
	
	EnvVarHandler & createNewInstanceAndCopyMeToIt();
	
	EnvVarHandler & setOnUpdateConverter(auto func) {
		funcConvert = std::move(func);
		// funcConvert();
		hasInternalConverter = true;
		return *this;
	}
	
	bool isModified() { return evbCurrent != evbOld; }
	EnvVarHandler & clearModified() { evbOld = evbCurrent; return *this; }
	
	EnvVarHandler() { evtH=('.'); bJustToCopyFrom=(false); hasInternalConverter=(false); targetCopyTo=(nullptr); }
	// constructors for base types. only used by temporary objects. here also: get.() and set.(val)
	#define EnvVarHandlerEasySimpleCode(TYPE,SUFFIX,MIN,MAX) \
		EnvVarHandler(EnvVarHandler * _targetCopyTo, std::string _strId, std::string _msg, TYPE val, TYPE min = MIN, TYPE max = MAX) { \
			EnvVarHandler(); \
			evbCurrent.ev##SUFFIX = val; \
			evbOld.ev##SUFFIX = val; \
			evbMin.ev##SUFFIX = min; \
			evbMax.ev##SUFFIX = max; \
			initTmpInstanceAndReadEnvVar(_targetCopyTo, std::string(#SUFFIX)[0], _strId, _msg, false, true); \
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
	EnvVarHandler(EnvVarHandler & evCopyFrom);
	EnvVarHandler(EnvVarHandler * _targetCopyTo, std::string _strId, std::string _msg, const char * val) : EnvVarHandler(_targetCopyTo, _strId, _msg, std::string(val)) { } // this is important! otherwise a const char val would call the boolean overload!!
	~EnvVarHandler() { }
	
	EnvVarHandler & operator=(const EnvVarHandler & evCopyFrom);
	
	std::string id() { return strId; }
	
	std::string toString();
	EnvVarHandler & setAuto(std::string _strEVB);
	
	static EnvVarHandler * getEVH(std::string _id);
	static std::string getEnvVarHandlerList();
	inline static const char* validIdChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
};

// TODO template <typename TB> ? but what about the conversions between types? would have to be handled outside here right?
class EnvVar {
	
private:
	std::string id;
	
	// TODO TB* evarPointer;
	std::string * varString;
	EnvRegex * varRegex;
	s32 * varInt;
	s32 iMin;
	s32 iMax;
	f32 * varFloat;
	f32 fMin;
	f32 fMax;
	bool * varBool;
	std::string msg;
	
	bool modified;
	
public:
	
	EnvVar(std::string _id) : id(_id), varString(nullptr), varRegex(nullptr), varInt(nullptr), iMin(0), iMax(0), varFloat(nullptr), fMin(0.f), fMax(0.f), varBool(nullptr), msg(""), modified(false) {}
	
	EnvVar & initVar(std::string * _varString, s32 * _varInt, f32 * _varFloat, bool * _varBool, EnvRegex * _varRegex);
	
	std::string getId() { return id; }
	
	EnvVar & setVal(std::string val, bool allowLog = false);
	EnvVar & setVal(s32 val, bool allowLog = false);
	EnvVar & setVal(f32 val, bool allowLog = false);
	EnvVar & setVal(bool val, bool allowLog = false);
	EnvVar & setValAuto(std::string val, bool allowLog = false, std::string strMsg = "", std::string valDefault = "", std::string valMin = "", std::string valMax = "");
	
	EnvVar & setMsg(std::string _strMsg) { msg = _strMsg; return *this; }
	std::string getMsg() {return msg;}
	
	std::string getString();
	s32 getInteger();
	f32 getFloat();
	bool getBoolean();
	
	bool checkModified() { if(modified) { modified = false; return true; } return false; }
	
	bool isString() { return varString || varRegex; }
};

static std::vector<EnvVar> vEnvVar;
EnvVar * getEnvVar(std::string id);
std::string getEnvVarList();

const char * getEnvironmentVariableValueBase(const char * name, const Logger::LogLevel logMode = Logger::LogLevel::Info, const char * strMsg = "", const char * defaultValue = nullptr, const char * pcOverrideValue = nullptr);
//EnvVar & initEnvVarStr(std::string & varString, const char * name, char & val, const char * strMsg = "", const Logger::LogLevel logMode = Logger::LogLevel::Info);
EnvVar & getEnvironmentVariableValueString(std::string & varString, const char * name, std::string & strValue, const Logger::LogLevel logMode = Logger::LogLevel::Info, const char * strMsg = "");
EnvRegex & getEnvironmentVariableValueRegex(EnvRegex & varRegex, const char * name, const Logger::LogLevel logMode = Logger::LogLevel::Info, const char * strMsg = "", const char * defaultValue = ".*");
EnvVar & getEnvironmentVariableValueBoolean(bool & varBool, const char * name, const Logger::LogLevel logMode = Logger::LogLevel::Info, const char * strMsg = "", bool defaultValue = false);
EnvVar & getEnvironmentVariableValueFloat(f32 & varFloat, const char * name, const Logger::LogLevel logMode = Logger::LogLevel::Info, const char * strMsg = "", f32 defaultValue = 0.f, f32 min = std::numeric_limits<f32>::min(), f32 max = std::numeric_limits<f32>::max());
EnvVar & getEnvironmentVariableValueInteger(s32 & varInt, const char * name, const Logger::LogLevel logMode = Logger::LogLevel::Info, const char * strMsg = "", s32 defaultValue = 0, s32 min = std::numeric_limits<s32>::min(), s32 max = std::numeric_limits<s32>::max());

template <typename T>
EnvVar & getEnvironmentVariableValueCustom(const T & var, const char * name, T & val, const Logger::LogLevel logMode = Logger::LogLevel::Info, const char * strMsg = "", const T min = std::numeric_limits<T>::min(), const T max = std::numeric_limits<T>::max());

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

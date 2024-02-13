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
		EnvVarData() : evS(""), evI(0), evF(0.f), evB(false) {}
		
		unsigned char evt;
		
		std::string evS;
		int evI;
		float evF;
		bool evB;
		
		bool operator!=(EnvVarData & other) {
			switch(evt) {
				case 'S': return evS == other.evS;
				case 'I': return evI == other.evI;
				case 'F': return evF == other.evF;
				case 'B': return evB == other.evB;
				default: arx_assert(false);
			}
		}
	};

	std::string strId;
	
	unsigned char evt;
	
	EnvVarData evbCurrent;
	EnvVarData evbOld;
	EnvVarData evbMin;
	EnvVarData evbMax;
	
	std::string strEVB;
	std::string msg;
	
	bool bJustToCopyFrom;
	//bool bCanOnlyCopyFromAnother;
	
	inline static std::map<std::string, EnvVarHandler&> vEVH;
	
	void initTmpInstanceAndReadEnvVar(char _evt, std::string _strId, std::string _msg, bool _useFuncConvert, bool _bJustToCopyFrom) {
		evbMax.evt = evbMin.evt = evbOld.evt = evbCurrent.evt = evt = _evt;
		strId = _strId;
		msg = _msg;
		useFuncConvert = _useFuncConvert;
		bJustToCopyFrom = _bJustToCopyFrom;
		
		//funcConvert = [](){};
		
		arx_assert_msg(strId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789") == std::string::npos, "env var id contains invalid characters \"%s\"", strId.c_str());
		
		const char * pcVal = getenv(strId.c_str());
		if(pcVal) {
			LogInfo << "[EnvVar] " << strId << " = \"" << pcVal << "\"";
			setAuto(pcVal);
		} else {
			strEVB = toString().c_str(); // the default will be converted to string here
		}
	}
	
  std::function<void()> funcConvert;
  bool useFuncConvert;
  
	void fixMinMax() {
		switch(evt) {
			case 'S':break;
			case 'I':
				if(evbCurrent.evI < evbMin.evI) evbCurrent.evI = evbMin.evI;
				else
				if(evbCurrent.evI > evbMax.evI) evbCurrent.evI = evbMax.evI;
				break;
			case 'F':
				if(evbCurrent.evF < evbMin.evF) evbCurrent.evF = evbMin.evF;
				else
				if(evbCurrent.evF > evbMax.evF) evbCurrent.evF = evbMax.evF;
				break;
			case 'B':break;
			default: arx_assert(false);
		}
	}
	
public:
	
	EnvVarHandler & setOnUpdateConverter(auto func) { funcConvert = std::move(func); useFuncConvert = true; return *this; } // funcConvert();
	
	bool chkMod(bool bConsume = true, bool bConvert = true) {
		bool bMod = evbCurrent != evbOld;
		if(bMod && bConsume) {
			evbOld = evbCurrent;
		}
		if(bMod && bConvert && useFuncConvert) {
			funcConvert();
		}
		return bMod;
	}
	
	//void init() { evbMin = evbMax = TB(); }
	
	//EnvVarHandler() : bCanOnlyCopyFromAnother(true) { }
	EnvVarHandler() : bJustToCopyFrom(false), useFuncConvert(false) { }
	EnvVarHandler(const EnvVarHandler & evCopyFrom) {
		bJustToCopyFrom=(false);
		useFuncConvert=(false);
		
		*this = evCopyFrom; // operator=()
		
		arx_assert(!bJustToCopyFrom);
		vEVH.emplace(strId, *this);
	}
	EnvVarHandler & operator=(const EnvVarHandler & evCopyFrom)
	{
		// arx_assert(!bJustToCopyFrom && evCopyFrom.bJustToCopyFrom); // this is mainly to lower confusion
		
		strId = evCopyFrom.strId;
		
		evbCurrent = evCopyFrom.evbCurrent;
		evbOld = evCopyFrom.evbOld;
		evbMin = evCopyFrom.evbMin;
		evbMax = evCopyFrom.evbMax;
		
		msg = evCopyFrom.msg;
		
		funcConvert = evCopyFrom.funcConvert;
		useFuncConvert = evCopyFrom.useFuncConvert;
		return *this;
	}
	
	// T type, S suffix. constructor() and get()
	#define EnvVarHandlerEasySimpleCode(T,S,MIN,MAX) \
		EnvVarHandler(std::string _strId, std::string _msg, T val, T min = MIN, T max = MAX) { \
			evbCurrent.ev##S = val; evbOld.ev##S = val; evbMin.ev##S = min; evbMax.ev##S = max; \
			initTmpInstanceAndReadEnvVar(std::string(#S)[0], _strId, _msg, false, true); } \
		T get##S() { arx_assert_msg(evt != std::string(#S)[0], "requested %c but is %c", std::string(#S)[0], evt); return evbCurrent.ev##S; } \
		EnvVarHandler & set##S(T val) { evbCurrent.ev##S = val; fixMinMax(); chkMod(); return *this; }
	
	EnvVarHandlerEasySimpleCode(std::string,S,"","")
	EnvVarHandlerEasySimpleCode(int,I,std::numeric_limits<int>::min(),std::numeric_limits<int>::max())
	EnvVarHandlerEasySimpleCode(float,F,std::numeric_limits<float>::min(),std::numeric_limits<float>::max())
	EnvVarHandlerEasySimpleCode(bool,B,false,false)
	
	//EnvVarHandler & setS(std::string val) { fixMinMax(); evbCurrent.evS = val; chkMod(); return *this; }
	//EnvVarHandler & setB(bool val) { fixMinMax(); evbCurrent.evB = val; chkMod(); return *this; }
	//EnvVarHandler & setI(int val) {
		//fixMinMax();
		//evbCurrent.evI = val;
		//chkMod();
		//return *this;
	//}
	//EnvVarHandler & setF(float val) {
		//fixMinMax();
		//evbCurrent.evF = val;
		//chkMod();
		//return *this;
	//}
	
	
	//EnvVarHandler(TB _evar) { init(); evb = _evar; }
	
	std::string id() { return strId; }
	//EnvVarHandler & setId(std::string _id) { arx_assert(strId == "" && _id != ""); strId = _id; return *this; }
	
	std::string toString()
	{
		switch(evt) {
			case 'S': return evbCurrent.evS;
			case 'I': return std::to_string(evbCurrent.evI);
			case 'F': return std::to_string(evbCurrent.evF);
			case 'B': return evbCurrent.evB ? "true" : "false";
			default: arx_assert(false);
		}
		return "";
	}
	
	EnvVarHandler & setAuto(std::string _strEVB)
	{
		try {
			switch(evt) {
				case 'S': setS(_strEVB); break;
				case 'I': setI(boost::lexical_cast<int>(_strEVB)); break;
				case 'F': setF(boost::lexical_cast<float>(_strEVB)); break;
				case 'B': setB(util::toLowercase(_strEVB) == "true"); break;
				default: arx_assert(false);
			}
		} catch(const std::exception & e) {
			LogError << "[EnvVar] " << strId << ": parsing \"" << _strEVB << "\" to '" << evt << "'";
		}
		
		return *this;
	}
	
	static EnvVarHandler * getEVH(std::string _id) {
		for(auto it : vEVH) {
			if(it.first == _id) {
				return &it.second;
			}
		}
		return nullptr;
	}
	
	static std::string getEnvVarHandlerList() {
		std::string strList;
		std::string str2;
		for(auto it : vEVH) {
			str2 = it.first + "=\"" + it.second.toString() + "\";\n";
			LogInfo << "Environment Variable: " << str2;
			strList += str2;
		}
		return strList;
	}
	
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

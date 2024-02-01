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

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <stddef.h>
#include <map>

#include "platform/Platform.h"

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

const char * getEnvironmentVariableValueBase(const char * name, char cLogMode = 'i', const char * strMsg = "", const char * defaultValue = nullptr, const char * pcOverrideValue = nullptr);
std::string getEnvironmentVariableValueString(std::string & varString, const char * name, char cLogMode = 'i', const char * strMsg = "", const char * defaultValue = nullptr);
bool getEnvironmentVariableValueBoolean(bool & varBool, const char * name, char cLogMode = 'i', const char * strMsg = "", bool defaultValue = false);
f32 getEnvironmentVariableValueFloat(f32 & varFloat, const char * name, char cLogMode = 'i', const char * strMsg = "", f32 defaultValue = 0.f, f32 min = std::numeric_limits<f32>::min(), f32 max = std::numeric_limits<f32>::max());
s32 getEnvironmentVariableValueInteger(s32 & varInt, const char * name, char cLogMode = 'i', const char * strMsg = "", s32 defaultValue = 0, s32 min = std::numeric_limits<s32>::min(), s32 max = std::numeric_limits<s32>::max());

//! Unset an environment variable
void unsetEnvironmentVariable(const char * name);

class EnvVar {
	
	std::string id;
	std::string * varString;
	s32 * varInt;
	f32 * varFloat;
	bool * varBool;
	
public:
	EnvVar(std::string _id) : id(_id), varString(nullptr), varInt(nullptr), varFloat(nullptr), varBool(nullptr) {}
	
	EnvVar & initVar(std::string _id, std::string * _varString, s32 * _varInt, f32 * _varFloat, bool * _varBool);
	
	std::string getId() { return id; }
	
	bool setVal(std::string val);
	bool setVal(s32 val);
	bool setVal(f32 val);
	bool setVal(bool val);
	
	std::string getString();
	s32 getInt();
	f32 getFloat();
	bool getBool();
};
static std::vector<EnvVar> vEnvVar;
EnvVar * getEnvVar(std::string id);
std::string getEnvVarList();

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

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

#ifndef ARX_SCRIPT_SCRIPTUTILS_H
#define ARX_SCRIPT_SCRIPTUTILS_H

#include <string>
#include <vector>

#include "platform/Platform.h"
#include "script/ScriptEvent.h"
#include "io/log/Logger.h"

namespace script {

//! strip [] brackets
std::string_view toLocalizationKey(std::string_view str);

inline u64 flag(char c) {
	if(c >= '0' && c <= '9') {
		return (u64(1) << (c - '0'));
	} else if(c >= 'a' && c <= 'z') {
		return (u64(1) << (c - 'a' + 10));
	} else {
		return (u64(1) << 63);
	}
}

inline bool test_flag(u64 flg, char c) {
	return (flg & flag(c)) != 0;
}

inline u64 flagsToMask(std::string_view flags) {
	
	
	size_t i = 0;
	if(!flags.empty() && flags[0] == '-') {
		i++;
	}
	
	u64 result = 0ul;
	for(; i < flags.length(); i++) {
		result |= flag(flags[i]);
	}
	
	return result;
}

/*!
 * Overload to give compilers a chance to calculate flag masks at
 * compile-time for string constants.
 * 
 * This should probably be done using constexpr in c++11.
 * We could force compile-time calculation with a template-based
 * implementation, but that will be much uglier and limited.
 */
template <size_t N>
u64 flagsToMask(const char (&flags)[N]) {
	
	u64 result = 0ul;
	for(size_t i = (flags[0] == '-') ? 1 : 0; i < N - 1; i++) {
		result |= flag(flags[i]);
	}
	
	return result;
}

class Context {
	
	const EERIE_SCRIPT * m_script;
	size_t m_pos;
	Entity * m_sender;
	Entity * m_entity;
	ScriptMessage m_message;
	ScriptParameters m_parameters;
	std::vector<std::pair<size_t, std::string>> m_stackIdCalledFromPos;
	std::vector<size_t> m_vNewLineAt;
	
public:
	
	explicit Context(const EERIE_SCRIPT * script, size_t pos, Entity * sender, Entity * entity,
	                 ScriptMessage msg, ScriptParameters parameters);
	
	std::string getStringVar(std::string_view name, Entity * entOverride = nullptr) const;
	std::string getFlags();
	std::string getWord(bool evaluateVars = true);
	void skipWord();
	std::string formatString(std::string format, float var) const;
	std::string formatString(std::string format, long var) const;
	std::string formatString(std::string format, std::string var) const;
	
	std::string getCommand(bool skipNewlines = true);
	
	void skipWhitespace(bool skipNewlines = false, bool warnNewlines = false);
	void skipWhitespaceAndComment();
	
	void updateNewLinesList();
	
	Entity * getSender() const { return m_sender; }
	Entity * getEntity() const { return m_entity; }
	ScriptMessage getMessage() const { return m_message; }
	const ScriptParameters & getParameters() const { return m_parameters; }
	
	bool getBool();
	
	float getFloat();
	
	float getFloatVar(std::string_view name, Entity * entOverride = nullptr) const;
	
	/*!
	 * Skip input until the end of the current line.
	 * \return the current position or (size_t)-1 if we are already at the line end
	 */
	size_t skipCommand();
	
	void skipBlock();
	
	bool jumpToLabel(std::string_view target, bool substack = false);
	bool returnToCaller();
	
	const EERIE_SCRIPT * getScript() const { return m_script; }
	
	size_t getPosition() const { return m_pos; }
	void getLineColumn(size_t & iLine, size_t & iColumn, size_t pos = static_cast<size_t>(-1)) const;
	std::string getPositionAndLineNumber(bool compact = false, size_t pos = static_cast<size_t>(-1)) const;
	
	size_t getGoSubCallFromPos(size_t  indexFromLast) const;
	std::string getGoSubCallStack(std::string_view prepend, std::string_view append, std::string_view between = " -> ", size_t indexFromLast = size_t(-1)) const;
	
	void seekToPosition(size_t pos);
	
	bool writePreCompiledData(std::string & esdat, size_t pos, unsigned char cCmd, unsigned char cSkipCharsCount);
	
};

class Command {
	
	const std::string m_name;
	const long m_entityFlags;
	
public:
	
	enum Result {
		Success,
		Failed,
		AbortAccept,
		AbortRefuse,
		AbortError,
		AbortDestructive,
		Jumped
	};
	
	static const long AnyEntity = -1;
	
	Command(const Command &) = delete;
	Command & operator=(const Command &) = delete;
	
	explicit Command(std::string_view name, long entityFlags = 0)
		: m_name(name), m_entityFlags(entityFlags) { }
	
	virtual ~Command() = default;
	
	virtual Result execute(Context & context) = 0;
	
	virtual Result peek(Context & context) {
		
		ARX_UNUSED(context);
		
		return AbortDestructive;
	}
	
	const std::string & getName() const { return m_name; }
	long getEntityFlags() const { return m_entityFlags; }
	
};

bool isSuppressed(const Context & context, std::string_view command);

bool isBlockEndSuprressed(const Context & context, std::string_view command);

size_t initSuppressions();

#define ScriptContextPrefix(context) '[' << ((context).getEntity() ? (((context).getScript() == &(context).getEntity()->script) ? (context).getEntity()->className() : (context).getEntity()->idString()) : "unknown") << ':' << (context).getPositionAndLineNumber() << (context).getGoSubCallStack(" {CallStackId(FromPosition): ", " } ") << "] "
#define ScriptPrefix ScriptContextPrefix(context) << getName() <<
#define DebugScript(args) LogDebug(ScriptPrefix args)
#define ScriptInfo(args) LogInfo << ScriptPrefix args
#define ScriptWarning ARX_LOG(isSuppressed(context, getName()) ? Logger::Debug : Logger::Warning) << ScriptPrefix ": "
#define ScriptError   ARX_LOG(isSuppressed(context, getName()) ? Logger::Debug : Logger::Error) << ScriptPrefix ": "

#define HandleFlags(expected) std::string options = context.getFlags(); \
	for(u64 run = !options.empty(), flg = 0; run && ((flg = flagsToMask(options), (flg && !(flg & ~flagsToMask(expected)))) || (ScriptWarning << "unexpected flags: " << options, true)); run = 0)
struct PreCompiled { // sketch studing script pre-compilation
	// these shall not be saved, so no need to keep the values unchanged, but they shall not clash, like in an enum.
	static const unsigned char REFERENCE = 1; // is the hint telling there is a reference to a command
	static const unsigned char JUSTSKIP = 2; // used in a skip loop that can skip at most 255 chars per time
	//static const unsigned char WHITESPACE = 3; // is the hint telling to skip up to 255-1=254 chars of comments or white spaces (because \x00 shall not be used in the middle of a string).
	
	// commands
	//static const unsigned char IF = 5;
	//static const unsigned char SET = 6;
};
//enum PreCompileReference { //TODO sketch studing script pre-compilation
	//// \x01 is the hint telling there is a reference
	//// \x02 is the hint telling to skip up to 255-1=254 chars of comments or white spaces (because \x00 shall not be used in the middle of a string).
	//// commands
	//IF = 3,
	//SET,
//};

bool askOkCancelCustomUserSystemPopupCommand(const std::string strTitle, const std::string strCustomMessage, const std::string strDetails = "", const std::string strCodeFile = "", const std::string strScriptStringVariableID = "", const Context * context = nullptr, size_t callStackIndexFromLast = 0);

size_t seekBackwardsForCommentToken(const std::string_view & esdat, size_t posToBackTrackFrom);

bool detectAndSkipComment(const std::string_view & esdat, size_t & pos, bool skipNewlines);

struct PreCompiled { // sketch studing script pre-compilation
	// these shall not be saved, so no need to keep the values unchanged, but they shall not clash, like in an enum.
	static const unsigned char REFERENCE = 1; // is the hint telling there is a reference to a command
	static const unsigned char JUSTSKIP = 2; // used in a skip loop that can skip at most 255 chars per time
	//static const unsigned char WHITESPACE = 3; // is the hint telling to skip up to 255-1=254 chars of comments or white spaces (because \x00 shall not be used in the middle of a string).
	
	// commands
	//static const unsigned char IF = 5;
	//static const unsigned char SET = 6;
};
//enum PreCompileReference { //TODO sketch studing script pre-compilation
	//// \x01 is the hint telling there is a reference
	//// \x02 is the hint telling to skip up to 255-1=254 chars of comments or white spaces (because \x00 shall not be used in the middle of a string).
	//// commands
	//IF = 3,
	//SET,
//};

} // namespace script

#endif // ARX_SCRIPT_SCRIPTUTILS_H

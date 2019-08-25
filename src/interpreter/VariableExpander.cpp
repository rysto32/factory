/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ryan Stone
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "VariableExpander.h"

#include "InterpreterException.h"

#include <sstream>

VariableExpander::VariableExpander(VarMap &&m)
  : vars(m)
{
}

std::string
VariableExpander::ExpandVars(std::string_view str)
{
	std::unordered_set<std::string_view> expandedVars;
	return ExpandVars(str, expandedVars);
}

std::string
VariableExpander::ExpandVars(std::string_view str, std::unordered_set<std::string_view> & evaluatedVars)
{
	std::ostringstream output;

	for (size_t i = 0; i < str.size(); ++i) {
		auto ch = str.at(i);

		if (ch != '$') {
			output.put(ch);
			continue;
		}

		++i;
		if (i >= str.size()) {
			throw InterpreterException("Incomplete variable expansion in '%s'", str.data());
		}

		auto next = str.at(i);
		if (next == '{' || next == '(') {
			i++;
			if (i >= str.size()) {
				throw InterpreterException("Incomplete variable expansion in '%s'", str.data());
			}
			EvaluateVar(str, i, next, output, evaluatedVars);
		} else {
			output << ExpandVar(str.substr(i, 1), evaluatedVars);
		}
	}

	return output.str();
}



std::string
VariableExpander::ExpandVar(std::string_view varName, std::unordered_set<std::string_view> & evaluatedVars)
{
	auto it = vars.find(varName);
	if (it == vars.end()) {
		throw InterpreterException("Undefined variable '%s'", std::string(varName).c_str());
	}

	auto [sit, success] = evaluatedVars.insert(varName);
	if (!success) {
		throw InterpreterException("Recursion in expansion of variable '%s'", std::string(varName).c_str());
	}

	std::string expansion = ExpandVars(it->second, evaluatedVars);
	evaluatedVars.erase(varName);
	return expansion;
}

void
VariableExpander::VarRemoveWord(std::string & expansion, std::string_view word)
{
	enum { WORD_BOUNDARY, IN_CANDIDATE, NEXT_WORD } state;

	state = WORD_BOUNDARY;
	size_t wordIndex = 0;
	std::string::iterator candidateStart;
	for (auto it = expansion.begin(); it != expansion.end();) {
		auto ch = *it;

		switch (state) {
		case WORD_BOUNDARY:
			if (isspace(ch)) {
				break;
			}

			state = IN_CANDIDATE;
			wordIndex = 0;
			candidateStart = it;

			/* Fall through */
		case IN_CANDIDATE:
			if (isspace(ch)) {
				if (wordIndex == word.size()) {
					it = expansion.erase(candidateStart, it);
				}
				state = WORD_BOUNDARY;
				continue;
			}

			if (wordIndex == word.size() || ch != word.at(wordIndex)) {
				state = NEXT_WORD;
			} else {
				wordIndex++;
			}
			break;
		case NEXT_WORD:
			if (isspace(ch)) {
				state = WORD_BOUNDARY;
			}

			break;
		}

		 ++it;
	}
}

void
VariableExpander::ApplyVarOption(std::string & expansion, char option, std::string_view param)
{
	switch (option) {
		case 'N':
			VarRemoveWord(expansion, param);
			break;
		default:
			throw InterpreterException("Unhandled var expansion option '%c'", option);
	}
}

std::string
VariableExpander::EvaluateVarWithOptions(std::string_view str, size_t & i, char endVar,
    std::string_view varName, std::unordered_set<std::string_view> & evaluatedVars)
{
	auto option = str.at(i);
	++i;

	std::string expansion(ExpandVar(varName, evaluatedVars));
	size_t paramStart = i;
	for (; i < str.size(); ++i) {
		auto ch = str.at(i);
		if (ch == ':' || ch == endVar) {
			std::string_view param = str.substr(paramStart, i - paramStart);
			ApplyVarOption(expansion, option, param);
			if (ch == endVar)
				return expansion;
		}
	}
	throw InterpreterException("Incomplete variable expansion in '%s'", str.data());
}

void
VariableExpander::EvaluateVar(std::string_view str, size_t & i, char varType, std::ostringstream & output,
     std::unordered_set<std::string_view> & evaluatedVars)
{
	char endVar;
	if (varType == '{')
		endVar = '}';
	else
		endVar = ')';

	size_t varStart = i;
	for (; i < str.size(); ++i) {
		auto ch = str.at(i);

		if (ch == endVar) {
			output << ExpandVar(str.substr(varStart, i - varStart), evaluatedVars);
			return;
		} else if (ch == ':') {
			std::string_view varName = str.substr(varStart, i - varStart);
			++i;
			if (i >= str.size()) {
				throw InterpreterException("Incomplete variable expansion in '%s'", str.data());
			}

			output << EvaluateVarWithOptions(str, i, endVar, varName, evaluatedVars);
			return;
		}
	}

	throw InterpreterException("Incomplete variable expansion in '%s'", str.data());
}

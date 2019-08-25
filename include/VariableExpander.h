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

#ifndef VARIABLE_EXPANDER_H
#define VARIABLE_EXPANDER_H

#include <string>
#include <unordered_map>
#include <unordered_set>

class VariableExpander
{
public:
	typedef std::unordered_map<std::string_view, std::string_view> VarMap;

private:
	VarMap vars;

	std::string ExpandVar(std::string_view varName, std::unordered_set<std::string_view> & evaluatedVars);
	void VarRemoveWord(std::string & expansion, std::string_view word);
	void ApplyVarOption(std::string & expansion, char option, std::string_view param);
	std::string EvaluateVarWithOptions(std::string_view str, size_t & i, char endVar,
	    std::string_view varName, std::unordered_set<std::string_view> & evaluatedVars);
	void EvaluateVar(std::string_view str, size_t & i, char varType, std::ostringstream & output,
	    std::unordered_set<std::string_view> & evaluatedVars);

	std::string ExpandVars(std::string_view, std::unordered_set<std::string_view> & evaluatedVars);

public:
	VariableExpander(VarMap &&);

	VariableExpander(VariableExpander &&) = delete;
	VariableExpander(const VariableExpander &) = delete;

	VariableExpander & operator=(VariableExpander &&) = delete;
	VariableExpander & operator=(const VariableExpander &) = delete;

	std::string ExpandVars(std::string_view);
};

#endif


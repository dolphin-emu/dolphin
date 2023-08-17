/*
The MIT License (MIT)

Copyright (c) 2015 Marko Zivanovic

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <functional>
#include <map>
#include "semver200.h"

#ifdef _MSC_VER
// disable symbol name too long warning
#pragma warning(disable:4503)
#endif

using namespace std;

namespace version {

	namespace {
		enum class Parser_state {
			major, minor, patch, prerelease, build
		};

		using Validator = function<void(const string&, const char)>;
		using State_transition_hook = function<void(string&)>;
		/// State transition is described by a character that triggers it, a state to transition to and
		/// optional hook to be invoked on transition.
		using Transition = tuple<const char, Parser_state, State_transition_hook>;
		using Transitions = vector<Transition>;
		using State = tuple<Transitions, string&, Validator>;
		using State_machine = map<Parser_state, State>;

		// Ranges of characters allowed in prerelease and build identifiers.
		const vector<pair<char, char>> allowed_prerel_id_chars = {
				{ '0', '9' },{ 'A','Z' },{ 'a','z' },{ '-','-' }
		};

		inline Transition mkx(const char c, Parser_state p, State_transition_hook pth) {
			return make_tuple(c, p, pth);
		}

		/// Advance parser state machine by a single step.
		/**
		Perform single step of parser state machine: if character matches one from transition tables -
		trigger transition to next state; otherwise, validate if current token is in legal state
		(throw Parse_error if not) and then add character to current token; State transition includes
		preparing various vars for next state and invoking state transition hook (if specified) which is
		where whole tokens are validated.
		*/
		inline void process_char(const char c, Parser_state& cstate, Parser_state& pstate,
			const Transitions& transitions, string& target, Validator validate) {
			for (const auto& transition : transitions) {
				if (c == get<0>(transition)) {
					if (get<2>(transition)) get<2>(transition)(target);
					pstate = cstate;
					cstate = get<1>(transition);
					return;
				}
			}
			validate(target, c);
			target.push_back(c);
		}

		/// Validate normal (major, minor, patch) version components.
		inline void normal_version_validator(const string&, const char) {
			//if (c < '0' || c > '9') throw Parse_error("invalid character encountered: " + string(1, c));
			//if (tgt.compare(0, 1, "0") == 0) throw Parse_error("leading 0 not allowed");
		}

		/// Validate that prerelease and build version identifiers are comprised of allowed chars only.
		inline void prerelease_version_validator(const string&, const char c) {
			bool res = false;
			for (const auto& r : allowed_prerel_id_chars) {
				res |= (c >= r.first && c <= r.second);
			}
                        // Trick the compiler that this is being used so it stops crashing on macos
                        (void)res;

                        // if (!res)
                        //	throw Parse_error("invalid character encountered: " + string(1, c));
		}

		inline bool is_identifier_numeric(const string& id) {
			return id.find_first_not_of("0123456789") == string::npos;
		}

		// inline bool check_for_leading_0(const string& str) {
		// 	return str.length() > 1 && str[0] == '0';
		// }

		/// Validate every individual prerelease identifier, determine it's type and add it to collection.
		void prerelease_hook_impl(string& id, Prerelease_identifiers& prerelease) {
			//if (id.empty()) throw Parse_error("version identifier cannot be empty");
			Id_type t = Id_type::alnum;
			if (is_identifier_numeric(id)) {
				t = Id_type::num;
				//if (check_for_leading_0(id)) {
				//	throw Parse_error("numeric identifiers cannot have leading 0");
				//}
			}
			prerelease.push_back(Prerelease_identifier(id, t));
			id.clear();
		}

		/// Validate every individual build identifier and add it to collection.
		void build_hook_impl(string& id, Parser_state& pstate, Build_identifiers& build,
			std::string& prerelease_id, Prerelease_identifiers& prerelease) {
			// process last token left from parsing prerelease data
			if (pstate == Parser_state::prerelease) prerelease_hook_impl(prerelease_id, prerelease);
			//if (id.empty()) throw Parse_error("version identifier cannot be empty");
			build.push_back(id);
			id.clear();
		}

	}

	/// Parse semver 2.0.0-compatible string to Version_data structure.
	/**
	Version text parser is implemented as a state machine. In each step one successive character from version
	string is consumed and is either added to current token or triggers state transition. Hooks can be
	injected into state transitions for validation/customization purposes.
	*/
	Version_data Semver200_parser::parse(const string& s) const {
		string major;
		string minor;
		string patch;
		string prerelease_id;
		string build_id;
		Prerelease_identifiers prerelease;
		Build_identifiers build;
		Parser_state cstate{ Parser_state::major };
		Parser_state pstate;

		auto prerelease_hook = [&](string& id) {
			prerelease_hook_impl(id, prerelease);
		};

		auto build_hook = [&](string& id) {
			build_hook_impl(id, pstate, build, prerelease_id, prerelease);
		};

		// State transition tables
		auto major_trans = {
			mkx('.', Parser_state::minor, {})
		};
		auto minor_trans = {
			mkx('.', Parser_state::patch, {})
		};
		auto patch_trans = {
			mkx('-', Parser_state::prerelease, {}),
			mkx('+', Parser_state::build, {})
		};
		auto prerelease_trans = {
			// When identifier separator (.) is found, stay in the same state but invoke hook
			// in order to process each individual identifier separately.
			mkx('.', Parser_state::prerelease, prerelease_hook),
			mkx('+', Parser_state::build, {})
		};
		auto build_trans = {
			// Same stay-in-the-same-state-but-invoke-hook trick from above.
			mkx('.', Parser_state::build, build_hook)
		};

		State_machine state_machine = {
			{Parser_state::major, State{major_trans, major, normal_version_validator}},
			{Parser_state::minor, State{minor_trans, minor, normal_version_validator}},
			{Parser_state::patch, State{patch_trans, patch, normal_version_validator}},
			{Parser_state::prerelease, State{prerelease_trans, prerelease_id, prerelease_version_validator}},
			{Parser_state::build, State{build_trans, build_id, prerelease_version_validator}}
		};

		// Main loop.
		for (const auto& c : s) {
			auto state = state_machine.at(cstate);
			process_char(c, cstate, pstate, get<0>(state), get<1>(state), get<2>(state));
		}

		// Trigger appropriate hooks in order to process last token, because no state transition was
		// triggered for it.
		if (cstate == Parser_state::prerelease) {
			prerelease_hook(prerelease_id);
		} else if (cstate == Parser_state::build) {
			build_hook(build_id);
		}

		//try {
		//	return Version_data{ stoi(major), stoi(minor), stoi(patch), prerelease, build };
		//} catch (invalid_argument& ex) {
		//	throw Parse_error(ex.what());
		//}
		return Version_data{ stoi(major), stoi(minor), stoi(patch), prerelease, build };
	}
}

/**
 * Copyright (C) 2010 Johannes Weißl <jargon@molb.org>
 * License: MIT License
 * URL: https://github.com/weisslj/cpp-optparse
 */

#ifndef OPTIONPARSER_H_
#define OPTIONPARSER_H_

#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <iostream>
#include <sstream>

namespace optparse {

class OptionParser;
class OptionGroup;
class Option;
class Values;
class Value;
class Callback;

typedef std::map<std::string,std::string> strMap;
typedef std::map<std::string,std::list<std::string> > lstMap;
typedef std::map<std::string,Option const*> optMap;

const char* const SUPPRESS_HELP = "SUPPRESS" "HELP";
const char* const SUPPRESS_USAGE = "SUPPRESS" "USAGE";

//! Class for automatic conversion from string -> anytype
class Value {
  public:
    Value() : str(), valid(false) {}
    Value(const std::string& v) : str(v), valid(true) {}
    operator const char*() { return str.c_str(); }
    operator bool() { bool t; return (valid && (std::istringstream(str) >> t)) ? t : false; }
    operator short() { short t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator unsigned short() { unsigned short t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator int() { int t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator unsigned int() { unsigned int t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator long() { long t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator unsigned long() { unsigned long t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator float() { float t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator double() { double t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
    operator long double() { long double t; return (valid && (std::istringstream(str) >> t)) ? t : 0; }
 private:
    const std::string str;
    bool valid;
};

class Values {
  public:
    Values() : _map() {}
    const std::string& operator[] (const std::string& d) const;
    std::string& operator[] (const std::string& d) { return _map[d]; }
    bool is_set(const std::string& d) const { return _map.find(d) != _map.end(); }
    bool is_set_by_user(const std::string& d) const { return _userSet.find(d) != _userSet.end(); }
    void is_set_by_user(const std::string& d, bool yes);
    Value get(const std::string& d) const { return (is_set(d)) ? Value((*this)[d]) : Value(); }

    typedef std::list<std::string>::iterator iterator;
    typedef std::list<std::string>::const_iterator const_iterator;
    std::list<std::string>& all(const std::string& d) { return _appendMap[d]; }
    const std::list<std::string>& all(const std::string& d) const { return _appendMap.find(d)->second; }

  private:
    strMap _map;
    lstMap _appendMap;
    std::set<std::string> _userSet;
};

class Option {
  public:
    Option(const OptionParser& p) :
      _parser(p), _action("store"), _type("string"), _nargs(1), _callback(0) {}
    virtual ~Option() {}

    Option& action(const std::string& a);
    Option& type(const std::string& t);
    Option& dest(const std::string& d) { _dest = d; return *this; }
    Option& set_default(const std::string& d) { _default = d; return *this; }
    template<typename T>
    Option& set_default(T t) { std::ostringstream ss; ss << t; _default = ss.str(); return *this; }
    Option& nargs(size_t n) { _nargs = n; return *this; }
    Option& set_const(const std::string& c) { _const = c; return *this; }
    template<typename InputIterator>
    Option& choices(InputIterator begin, InputIterator end) {
      _choices.assign(begin, end); type("choice"); return *this;
    }
#if __cplusplus >= 201103L
    Option& choices(std::initializer_list<std::string> ilist) {
      _choices.assign(ilist); type("choice"); return *this;
    }
#endif
    Option& help(const std::string& h) { _help = h; return *this; }
    Option& metavar(const std::string& m) { _metavar = m; return *this; }
    Option& callback(Callback& c) { _callback = &c; return *this; }

    const std::string& action() const { return _action; }
    const std::string& type() const { return _type; }
    const std::string& dest() const { return _dest; }
    const std::string& get_default() const;
    size_t nargs() const { return _nargs; }
    const std::string& get_const() const { return _const; }
    const std::list<std::string>& choices() const { return _choices; }
    const std::string& help() const { return _help; }
    const std::string& metavar() const { return _metavar; }
    Callback* callback() const { return _callback; }

  private:
    std::string check_type(const std::string& opt, const std::string& val) const;
    std::string format_option_help(unsigned int indent = 2) const;
    std::string format_help(unsigned int indent = 2) const;

    const OptionParser& _parser;

    std::set<std::string> _short_opts;
    std::set<std::string> _long_opts;

    std::string _action;
    std::string _type;
    std::string _dest;
    std::string _default;
    size_t _nargs;
    std::string _const;
    std::list<std::string> _choices;
    std::string _help;
    std::string _metavar;
    Callback* _callback;

    friend class OptionContainer;
    friend class OptionParser;
};

class OptionContainer {
  public:
    OptionContainer(const std::string& d = "") : _description(d) {}
    virtual ~OptionContainer() {}

    virtual OptionContainer& description(const std::string& d) { _description = d; return *this; }
    virtual const std::string& description() const { return _description; }

    Option& add_option(const std::string& opt);
    Option& add_option(const std::string& opt1, const std::string& opt2);
    Option& add_option(const std::string& opt1, const std::string& opt2, const std::string& opt3);
    Option& add_option(const std::vector<std::string>& opt);

    std::string format_option_help(unsigned int indent = 2) const;

  protected:
    std::string _description;

    std::list<Option> _opts;
    optMap _optmap_s;
    optMap _optmap_l;

  private:
    virtual const OptionParser& get_parser() = 0;
};

class OptionParser : public OptionContainer {
  public:
    OptionParser();
    virtual ~OptionParser() {}

    OptionParser& usage(const std::string& u) { set_usage(u); return *this; }
    OptionParser& version(const std::string& v) { _version = v; return *this; }
    OptionParser& description(const std::string& d) { _description = d; return *this; }
    OptionParser& add_help_option(bool h) { _add_help_option = h; return *this; }
    OptionParser& add_version_option(bool v) { _add_version_option = v; return *this; }
    OptionParser& prog(const std::string& p) { _prog = p; return *this; }
    OptionParser& epilog(const std::string& e) { _epilog = e; return *this; }
    OptionParser& set_defaults(const std::string& dest, const std::string& val) {
      _defaults[dest] = val; return *this;
    }
    template<typename T>
    OptionParser& set_defaults(const std::string& dest, T t) { std::ostringstream ss; ss << t; _defaults[dest] = ss.str(); return *this; }
    OptionParser& enable_interspersed_args() { _interspersed_args = true; return *this; }
    OptionParser& disable_interspersed_args() { _interspersed_args = false; return *this; }
    OptionParser& add_option_group(const OptionGroup& group);

    const std::string& usage() const { return _usage; }
    const std::string& version() const { return _version; }
    const std::string& description() const { return _description; }
    bool add_help_option() const { return _add_help_option; }
    bool add_version_option() const { return _add_version_option; }
    const std::string& prog() const { return _prog; }
    const std::string& epilog() const { return _epilog; }
    bool interspersed_args() const { return _interspersed_args; }

    Values& parse_args(int argc, char const* const* argv);
    Values& parse_args(const std::vector<std::string>& args);
    template<typename InputIterator>
    Values& parse_args(InputIterator begin, InputIterator end) {
      return parse_args(std::vector<std::string>(begin, end));
    }

    const std::list<std::string>& args() const { return _leftover; }
    std::vector<std::string> args() {
      return std::vector<std::string>(_leftover.begin(), _leftover.end());
    }

    std::string format_help() const;
    void print_help() const;

    void set_usage(const std::string& u);
    std::string get_usage() const;
    void print_usage(std::ostream& out) const;
    void print_usage() const;

    std::string get_version() const;
    void print_version(std::ostream& out) const;
    void print_version() const;

    void error(const std::string& msg) const;
    void exit() const;

  private:
    const OptionParser& get_parser() { return *this; }
    const Option& lookup_short_opt(const std::string& opt) const;
    const Option& lookup_long_opt(const std::string& opt) const;

    void handle_short_opt(const std::string& opt, const std::string& arg);
    void handle_long_opt(const std::string& optstr);

    void process_opt(const Option& option, const std::string& opt, const std::string& value);

    std::string format_usage(const std::string& u) const;

    std::string _usage;
    std::string _version;
    bool _add_help_option;
    bool _add_version_option;
    std::string _prog;
    std::string _epilog;
    bool _interspersed_args;

    Values _values;

    strMap _defaults;
    std::list<OptionGroup const*> _groups;

    std::list<std::string> _remaining;
    std::list<std::string> _leftover;

    friend class Option;
};

class OptionGroup : public OptionContainer {
  public:
    OptionGroup(const OptionParser& p, const std::string& t, const std::string& d = "") :
      OptionContainer(d), _parser(p), _title(t) {}
    virtual ~OptionGroup() {}

    OptionGroup& title(const std::string& t) { _title = t; return *this; }
    const std::string& title() const { return _title; }

  private:
    const OptionParser& get_parser() { return _parser; }

    const OptionParser& _parser;
    std::string _title;

  friend class OptionParser;
};

class Callback {
public:
  virtual void operator() (const Option& option, const std::string& opt, const std::string& val, const OptionParser& parser) = 0;
  virtual ~Callback() {}
};

}

#endif

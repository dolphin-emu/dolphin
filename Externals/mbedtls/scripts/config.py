#!/usr/bin/env python3

"""Mbed TLS configuration file manipulation library and tool

Basic usage, to read the Mbed TLS configuration:
    config = ConfigFile()
    if 'MBEDTLS_RSA_C' in config: print('RSA is enabled')
"""

# Note that the version of this script in the mbedtls-2.28 branch must remain
# compatible with Python 3.4.

## Copyright The Mbed TLS Contributors
## SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
##

import os
import re

class Setting:
    """Representation of one Mbed TLS config.h setting.

    Fields:
    * name: the symbol name ('MBEDTLS_xxx').
    * value: the value of the macro. The empty string for a plain #define
      with no value.
    * active: True if name is defined, False if a #define for name is
      present in config.h but commented out.
    * section: the name of the section that contains this symbol.
    """
    # pylint: disable=too-few-public-methods
    def __init__(self, active, name, value='', section=None):
        self.active = active
        self.name = name
        self.value = value
        self.section = section

class Config:
    """Representation of the Mbed TLS configuration.

    In the documentation of this class, a symbol is said to be *active*
    if there is a #define for it that is not commented out, and *known*
    if there is a #define for it whether commented out or not.

    This class supports the following protocols:
    * `name in config` is `True` if the symbol `name` is active, `False`
      otherwise (whether `name` is inactive or not known).
    * `config[name]` is the value of the macro `name`. If `name` is inactive,
      raise `KeyError` (even if `name` is known).
    * `config[name] = value` sets the value associated to `name`. `name`
      must be known, but does not need to be set. This does not cause
      name to become set.
    """

    def __init__(self):
        self.settings = {}

    def __contains__(self, name):
        """True if the given symbol is active (i.e. set).

        False if the given symbol is not set, even if a definition
        is present but commented out.
        """
        return name in self.settings and self.settings[name].active

    def all(self, *names):
        """True if all the elements of names are active (i.e. set)."""
        return all(self.__contains__(name) for name in names)

    def any(self, *names):
        """True if at least one symbol in names are active (i.e. set)."""
        return any(self.__contains__(name) for name in names)

    def known(self, name):
        """True if a #define for name is present, whether it's commented out or not."""
        return name in self.settings

    def __getitem__(self, name):
        """Get the value of name, i.e. what the preprocessor symbol expands to.

        If name is not known, raise KeyError. name does not need to be active.
        """
        return self.settings[name].value

    def get(self, name, default=None):
        """Get the value of name. If name is inactive (not set), return default.

        If a #define for name is present and not commented out, return
        its expansion, even if this is the empty string.

        If a #define for name is present but commented out, return default.
        """
        if name in self.settings:
            return self.settings[name].value
        else:
            return default

    def __setitem__(self, name, value):
        """If name is known, set its value.

        If name is not known, raise KeyError.
        """
        self.settings[name].value = value

    def set(self, name, value=None):
        """Set name to the given value and make it active.

        If value is None and name is already known, don't change its value.
        If value is None and name is not known, set its value to the empty
        string.
        """
        if name in self.settings:
            if value is not None:
                self.settings[name].value = value
            self.settings[name].active = True
        else:
            self.settings[name] = Setting(True, name, value=value)

    def unset(self, name):
        """Make name unset (inactive).

        name remains known if it was known before.
        """
        if name not in self.settings:
            return
        self.settings[name].active = False

    def adapt(self, adapter):
        """Run adapter on each known symbol and (de)activate it accordingly.

        `adapter` must be a function that returns a boolean. It is called as
        `adapter(name, active, section)` for each setting, where `active` is
        `True` if `name` is set and `False` if `name` is known but unset,
        and `section` is the name of the section containing `name`. If
        `adapter` returns `True`, then set `name` (i.e. make it active),
        otherwise unset `name` (i.e. make it known but inactive).
        """
        for setting in self.settings.values():
            setting.active = adapter(setting.name, setting.active,
                                     setting.section)

def is_full_section(section):
    """Is this section affected by "config.py full" and friends?"""
    return section.endswith('support') or section.endswith('modules')

def realfull_adapter(_name, active, section):
    """Activate all symbols found in the global and boolean feature sections.

    This is intended for building the documentation, including the
    documentation of settings that are activated by defining an optional
    preprocessor macro.

    Do not activate definitions in the section containing symbols that are
    supposed to be defined and documented in their own module.
    """
    if section == 'Module configuration options':
        return active
    return True

# The goal of the full configuration is to have everything that can be tested
# together. This includes deprecated or insecure options. It excludes:
# * Options that require additional build dependencies or unusual hardware.
# * Options that make testing less effective.
# * Options that are incompatible with other options, or more generally that
#   interact with other parts of the code in such a way that a bulk enabling
#   is not a good way to test them.
# * Options that remove features.
EXCLUDE_FROM_FULL = frozenset([
    #pylint: disable=line-too-long
    'MBEDTLS_CTR_DRBG_USE_128_BIT_KEY', # interacts with ENTROPY_FORCE_SHA256
    'MBEDTLS_DEPRECATED_REMOVED', # conflicts with deprecated options
    'MBEDTLS_DEPRECATED_WARNING', # conflicts with deprecated options
    'MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED', # influences the use of ECDH in TLS
    'MBEDTLS_ECP_NO_FALLBACK', # removes internal ECP implementation
    'MBEDTLS_ECP_NO_INTERNAL_RNG', # removes a feature
    'MBEDTLS_ECP_RESTARTABLE', # incompatible with USE_PSA_CRYPTO
    'MBEDTLS_ENTROPY_FORCE_SHA256', # interacts with CTR_DRBG_128_BIT_KEY
    'MBEDTLS_HAVE_SSE2', # hardware dependency
    'MBEDTLS_MEMORY_BACKTRACE', # depends on MEMORY_BUFFER_ALLOC_C
    'MBEDTLS_MEMORY_BUFFER_ALLOC_C', # makes sanitizers (e.g. ASan) less effective
    'MBEDTLS_MEMORY_DEBUG', # depends on MEMORY_BUFFER_ALLOC_C
    'MBEDTLS_NO_64BIT_MULTIPLICATION', # influences anything that uses bignum
    'MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES', # removes a feature
    'MBEDTLS_NO_PLATFORM_ENTROPY', # removes a feature
    'MBEDTLS_NO_UDBL_DIVISION', # influences anything that uses bignum
    'MBEDTLS_PKCS11_C', # build dependency (libpkcs11-helper)
    'MBEDTLS_PLATFORM_NO_STD_FUNCTIONS', # removes a feature
    'MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS', # removes a feature
    'MBEDTLS_PSA_CRYPTO_CONFIG', # toggles old/new style PSA config
    'MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG', # behavior change + build dependency
    'MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER', # incompatible with USE_PSA_CRYPTO
    'MBEDTLS_PSA_CRYPTO_SPM', # platform dependency (PSA SPM)
    'MBEDTLS_PSA_INJECT_ENTROPY', # conflicts with platform entropy sources
    'MBEDTLS_REMOVE_3DES_CIPHERSUITES', # removes a feature
    'MBEDTLS_REMOVE_ARC4_CIPHERSUITES', # removes a feature
    'MBEDTLS_RSA_NO_CRT', # influences the use of RSA in X.509 and TLS
    'MBEDTLS_SHA512_NO_SHA384', # removes a feature
    'MBEDTLS_SSL_HW_RECORD_ACCEL', # build dependency (hook functions)
    'MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN', # build dependency (clang+memsan)
    'MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND', # build dependency (valgrind headers)
    'MBEDTLS_TEST_NULL_ENTROPY', # removes a feature
    'MBEDTLS_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION', # influences the use of X.509 in TLS
    'MBEDTLS_ZLIB_SUPPORT', # build dependency (libz)
])

def is_seamless_alt(name):
    """Whether the xxx_ALT symbol should be included in the full configuration.

    Include alternative implementations of platform functions, which are
    configurable function pointers that default to the built-in function.
    This way we test that the function pointers exist and build correctly
    without changing the behavior, and tests can verify that the function
    pointers are used by modifying those pointers.

    Exclude alternative implementations of library functions since they require
    an implementation of the relevant functions and an xxx_alt.h header.
    """
    if name in (
            'MBEDTLS_PLATFORM_GMTIME_R_ALT',
            'MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT',
            'MBEDTLS_PLATFORM_ZEROIZE_ALT',
    ):
        # Similar to non-platform xxx_ALT, requires platform_alt.h
        return False
    return name.startswith('MBEDTLS_PLATFORM_')

def include_in_full(name):
    """Rules for symbols in the "full" configuration."""
    if name in EXCLUDE_FROM_FULL:
        return False
    if name.endswith('_ALT'):
        return is_seamless_alt(name)
    return True

def full_adapter(name, active, section):
    """Config adapter for "full"."""
    if not is_full_section(section):
        return active
    return include_in_full(name)

# The baremetal configuration excludes options that require a library or
# operating system feature that is typically not present on bare metal
# systems. Features that are excluded from "full" won't be in "baremetal"
# either (unless explicitly turned on in baremetal_adapter) so they don't
# need to be repeated here.
EXCLUDE_FROM_BAREMETAL = frozenset([
    #pylint: disable=line-too-long
    'MBEDTLS_ENTROPY_NV_SEED', # requires a filesystem and FS_IO or alternate NV seed hooks
    'MBEDTLS_FS_IO', # requires a filesystem
    'MBEDTLS_HAVEGE_C', # requires a clock
    'MBEDTLS_HAVE_TIME', # requires a clock
    'MBEDTLS_HAVE_TIME_DATE', # requires a clock
    'MBEDTLS_NET_C', # requires POSIX-like networking
    'MBEDTLS_PLATFORM_FPRINTF_ALT', # requires FILE* from stdio.h
    'MBEDTLS_PLATFORM_NV_SEED_ALT', # requires a filesystem and ENTROPY_NV_SEED
    'MBEDTLS_PLATFORM_TIME_ALT', # requires a clock and HAVE_TIME
    'MBEDTLS_PSA_CRYPTO_SE_C', # requires a filesystem and PSA_CRYPTO_STORAGE_C
    'MBEDTLS_PSA_CRYPTO_STORAGE_C', # requires a filesystem
    'MBEDTLS_PSA_ITS_FILE_C', # requires a filesystem
    'MBEDTLS_THREADING_C', # requires a threading interface
    'MBEDTLS_THREADING_PTHREAD', # requires pthread
    'MBEDTLS_TIMING_C', # requires a clock
])

def keep_in_baremetal(name):
    """Rules for symbols in the "baremetal" configuration."""
    if name in EXCLUDE_FROM_BAREMETAL:
        return False
    return True

def baremetal_adapter(name, active, section):
    """Config adapter for "baremetal"."""
    if not is_full_section(section):
        return active
    if name == 'MBEDTLS_NO_PLATFORM_ENTROPY':
        # No OS-provided entropy source
        return True
    return include_in_full(name) and keep_in_baremetal(name)

# This set contains options that are mostly for debugging or test purposes,
# and therefore should be excluded when doing code size measurements.
# Options that are their own module (such as MBEDTLS_CERTS_C and
# MBEDTLS_ERROR_C) are not listed and therefore will be included when doing
# code size measurements.
EXCLUDE_FOR_SIZE = frozenset([
    'MBEDTLS_CHECK_PARAMS', # increases the size of many modules
    'MBEDTLS_CHECK_PARAMS_ASSERT', # no effect without MBEDTLS_CHECK_PARAMS
    'MBEDTLS_DEBUG_C', # large code size increase in TLS
    'MBEDTLS_SELF_TEST', # increases the size of many modules
    'MBEDTLS_TEST_HOOKS', # only useful with the hosted test framework, increases code size
])

def baremetal_size_adapter(name, active, section):
    if name in EXCLUDE_FOR_SIZE:
        return False
    return baremetal_adapter(name, active, section)

def include_in_crypto(name):
    """Rules for symbols in a crypto configuration."""
    if name.startswith('MBEDTLS_X509_') or \
       name.startswith('MBEDTLS_SSL_') or \
       name.startswith('MBEDTLS_KEY_EXCHANGE_'):
        return False
    if name in [
            'MBEDTLS_CERTS_C', # part of libmbedx509
            'MBEDTLS_DEBUG_C', # part of libmbedtls
            'MBEDTLS_NET_C', # part of libmbedtls
            'MBEDTLS_PKCS11_C', # part of libmbedx509
    ]:
        return False
    return True

def crypto_adapter(adapter):
    """Modify an adapter to disable non-crypto symbols.

    ``crypto_adapter(adapter)(name, active, section)`` is like
    ``adapter(name, active, section)``, but unsets all X.509 and TLS symbols.
    """
    def continuation(name, active, section):
        if not include_in_crypto(name):
            return False
        if adapter is None:
            return active
        return adapter(name, active, section)
    return continuation

DEPRECATED = frozenset([
    'MBEDTLS_SSL_PROTO_SSL3',
    'MBEDTLS_SSL_CLI_ALLOW_WEAK_CERTIFICATE_VERIFICATION_WITHOUT_HOSTNAME',
    'MBEDTLS_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO',
])

def no_deprecated_adapter(adapter):
    """Modify an adapter to disable deprecated symbols.

    ``no_deprecated_adapter(adapter)(name, active, section)`` is like
    ``adapter(name, active, section)``, but unsets all deprecated symbols
    and sets ``MBEDTLS_DEPRECATED_REMOVED``.
    """
    def continuation(name, active, section):
        if name == 'MBEDTLS_DEPRECATED_REMOVED':
            return True
        if name in DEPRECATED:
            return False
        if adapter is None:
            return active
        return adapter(name, active, section)
    return continuation

def no_platform_adapter(adapter):
    """Modify an adapter to disable platform symbols.

    ``no_platform_adapter(adapter)(name, active, section)`` is like
    ``adapter(name, active, section)``, but unsets all platform symbols other
    ``than MBEDTLS_PLATFORM_C.
    """
    def continuation(name, active, section):
        # Allow MBEDTLS_PLATFORM_C but remove all other platform symbols.
        if name.startswith('MBEDTLS_PLATFORM_') and name != 'MBEDTLS_PLATFORM_C':
            return False
        if adapter is None:
            return active
        return adapter(name, active, section)
    return continuation

class ConfigFile(Config):
    """Representation of the Mbed TLS configuration read for a file.

    See the documentation of the `Config` class for methods to query
    and modify the configuration.
    """

    _path_in_tree = 'include/mbedtls/config.h'
    default_path = [_path_in_tree,
                    os.path.join(os.path.dirname(__file__),
                                 os.pardir,
                                 _path_in_tree),
                    os.path.join(os.path.dirname(os.path.abspath(os.path.dirname(__file__))),
                                 _path_in_tree)]

    def __init__(self, filename=None):
        """Read the Mbed TLS configuration file."""
        if filename is None:
            for candidate in self.default_path:
                if os.path.lexists(candidate):
                    filename = candidate
                    break
            else:
                raise Exception('Mbed TLS configuration file not found',
                                self.default_path)
        super().__init__()
        self.filename = filename
        self.inclusion_guard = None
        self.current_section = 'header'
        with open(filename, 'r', encoding='utf-8') as file:
            self.templates = [self._parse_line(line) for line in file]
        self.current_section = None

    def set(self, name, value=None):
        if name not in self.settings:
            self.templates.append((name, '', '#define ' + name + ' '))
        super().set(name, value)

    _define_line_regexp = (r'(?P<indentation>\s*)' +
                           r'(?P<commented_out>(//\s*)?)' +
                           r'(?P<define>#\s*define\s+)' +
                           r'(?P<name>\w+)' +
                           r'(?P<arguments>(?:\((?:\w|\s|,)*\))?)' +
                           r'(?P<separator>\s*)' +
                           r'(?P<value>.*)')
    _ifndef_line_regexp = r'#ifndef (?P<inclusion_guard>\w+)'
    _section_line_regexp = (r'\s*/?\*+\s*[\\@]name\s+SECTION:\s*' +
                            r'(?P<section>.*)[ */]*')
    _config_line_regexp = re.compile(r'|'.join([_define_line_regexp,
                                                _ifndef_line_regexp,
                                                _section_line_regexp]))
    def _parse_line(self, line):
        """Parse a line in config.h and return the corresponding template."""
        line = line.rstrip('\r\n')
        m = re.match(self._config_line_regexp, line)
        if m is None:
            return line
        elif m.group('section'):
            self.current_section = m.group('section')
            return line
        elif m.group('inclusion_guard') and self.inclusion_guard is None:
            self.inclusion_guard = m.group('inclusion_guard')
            return line
        else:
            active = not m.group('commented_out')
            name = m.group('name')
            value = m.group('value')
            if name == self.inclusion_guard and value == '':
                # The file double-inclusion guard is not an option.
                return line
            template = (name,
                        m.group('indentation'),
                        m.group('define') + name +
                        m.group('arguments') + m.group('separator'))
            self.settings[name] = Setting(active, name, value,
                                          self.current_section)
            return template

    def _format_template(self, name, indent, middle):
        """Build a line for config.h for the given setting.

        The line has the form "<indent>#define <name> <value>"
        where <middle> is "#define <name> ".
        """
        setting = self.settings[name]
        value = setting.value
        if value is None:
            value = ''
        # Normally the whitespace to separate the symbol name from the
        # value is part of middle, and there's no whitespace for a symbol
        # with no value. But if a symbol has been changed from having a
        # value to not having one, the whitespace is wrong, so fix it.
        if value:
            if middle[-1] not in '\t ':
                middle += ' '
        else:
            middle = middle.rstrip()
        return ''.join([indent,
                        '' if setting.active else '//',
                        middle,
                        value]).rstrip()

    def write_to_stream(self, output):
        """Write the whole configuration to output."""
        for template in self.templates:
            if isinstance(template, str):
                line = template
            else:
                line = self._format_template(*template)
            output.write(line + '\n')

    def write(self, filename=None):
        """Write the whole configuration to the file it was read from.

        If filename is specified, write to this file instead.
        """
        if filename is None:
            filename = self.filename
        with open(filename, 'w', encoding='utf-8') as output:
            self.write_to_stream(output)

if __name__ == '__main__':
    def main():
        """Command line config.h manipulation tool."""
        parser = argparse.ArgumentParser(description="""
        Mbed TLS configuration file manipulation tool.
        """)
        parser.add_argument('--file', '-f',
                            help="""File to read (and modify if requested).
                            Default: {}.
                            """.format(ConfigFile.default_path))
        parser.add_argument('--force', '-o',
                            action='store_true',
                            help="""For the set command, if SYMBOL is not
                            present, add a definition for it.""")
        parser.add_argument('--write', '-w', metavar='FILE',
                            help="""File to write to instead of the input file.""")
        subparsers = parser.add_subparsers(dest='command',
                                           title='Commands')
        parser_get = subparsers.add_parser('get',
                                           help="""Find the value of SYMBOL
                                           and print it. Exit with
                                           status 0 if a #define for SYMBOL is
                                           found, 1 otherwise.
                                           """)
        parser_get.add_argument('symbol', metavar='SYMBOL')
        parser_set = subparsers.add_parser('set',
                                           help="""Set SYMBOL to VALUE.
                                           If VALUE is omitted, just uncomment
                                           the #define for SYMBOL.
                                           Error out of a line defining
                                           SYMBOL (commented or not) is not
                                           found, unless --force is passed.
                                           """)
        parser_set.add_argument('symbol', metavar='SYMBOL')
        parser_set.add_argument('value', metavar='VALUE', nargs='?',
                                default='')
        parser_unset = subparsers.add_parser('unset',
                                             help="""Comment out the #define
                                             for SYMBOL. Do nothing if none
                                             is present.""")
        parser_unset.add_argument('symbol', metavar='SYMBOL')

        def add_adapter(name, function, description):
            subparser = subparsers.add_parser(name, help=description)
            subparser.set_defaults(adapter=function)
        add_adapter('baremetal', baremetal_adapter,
                    """Like full, but exclude features that require platform
                    features such as file input-output.""")
        add_adapter('baremetal_size', baremetal_size_adapter,
                    """Like baremetal, but exclude debugging features.
                    Useful for code size measurements.""")
        add_adapter('full', full_adapter,
                    """Uncomment most features.
                    Exclude alternative implementations and platform support
                    options, as well as some options that are awkward to test.
                    """)
        add_adapter('full_no_deprecated', no_deprecated_adapter(full_adapter),
                    """Uncomment most non-deprecated features.
                    Like "full", but without deprecated features.
                    """)
        add_adapter('full_no_platform', no_platform_adapter(full_adapter),
                    """Uncomment most non-platform features.
                    Like "full", but without platform features.
                    """)
        add_adapter('realfull', realfull_adapter,
                    """Uncomment all boolean #defines.
                    Suitable for generating documentation, but not for building.""")
        add_adapter('crypto', crypto_adapter(None),
                    """Only include crypto features. Exclude X.509 and TLS.""")
        add_adapter('crypto_baremetal', crypto_adapter(baremetal_adapter),
                    """Like baremetal, but with only crypto features,
                    excluding X.509 and TLS.""")
        add_adapter('crypto_full', crypto_adapter(full_adapter),
                    """Like full, but with only crypto features,
                    excluding X.509 and TLS.""")

        args = parser.parse_args()
        config = ConfigFile(args.file)
        if args.command is None:
            parser.print_help()
            return 1
        elif args.command == 'get':
            if args.symbol in config:
                value = config[args.symbol]
                if value:
                    sys.stdout.write(value + '\n')
            return 0 if args.symbol in config else 1
        elif args.command == 'set':
            if not args.force and args.symbol not in config.settings:
                sys.stderr.write("A #define for the symbol {} "
                                 "was not found in {}\n"
                                 .format(args.symbol, config.filename))
                return 1
            config.set(args.symbol, value=args.value)
        elif args.command == 'unset':
            config.unset(args.symbol)
        else:
            config.adapt(args.adapter)
        config.write(args.write)
        return 0

    # Import modules only used by main only if main is defined and called.
    # pylint: disable=wrong-import-position
    import argparse
    import sys
    sys.exit(main())

#! /usr/bin/env python

"""
check-includes.py <file...>

Checks if the includes are sorted properly and following the "system headers
before local headers" rule.

Ignores what is in #if blocks to avoid false negatives.
"""

import re
import sys

def exclude_if_blocks(lines):
    '''Removes lines from #if ... #endif blocks.'''
    level = 0
    for l in lines:
        if l.startswith('#if'):
            level += 1
        elif l.startswith('#endif'):
            level -= 1
        elif level == 0:
            yield l

def filter_includes(lines):
    '''Removes lines that are not #include and keeps only the file part.'''
    for l in lines:
        if l.startswith('#include'):
            if 'NOLINT' not in l:
                yield l.split(' ')[1]

class IncludeFileSorter(object):
    def __init__(self, path):
        self.path = path

    def __lt__(self, other):
        '''Sorting function for include files.

        * System headers go before local headers (check the first character -
          if it's different, then the one starting with " is the 'larger').
        * Then, iterate on all the path components:
          * If they are equal, try to continue to the next path component.
          * If not, return whether the path component are smaller/larger.
        * Paths with less components should go first, so after iterating, check
          whether one path still has some / in it.
        '''
        a, b = self.path, other.path
        if a[0] != b[0]:
            return False if a[0] == '"' else True
        a, b = a[1:-1].lower(), b[1:-1].lower()
        while '/' in a and '/' in b:
            ca, a = a.split('/', 1)
            cb, b = b.split('/', 1)
            if ca != cb:
                return ca < cb
        if '/' in a:
            return False
        elif '/' in b:
            return True
        else:
            return a < b

    def __eq__(self, other):
        return self.path.lower() == other.path.lower()

def sort_includes(includes):
    return sorted(includes, key=IncludeFileSorter)

def show_differences(bad, good):
    bad = ['    Current'] + bad
    good = ['    Should be'] + good
    longest = max(len(i) for i in bad)
    padded = [i + ' ' * (longest + 4 - len(i)) for i in bad]
    return '\n'.join('%s%s' % t for t in zip(padded, good))

def check_file(path):
    print('Checking %s' % path)
    try:
        try:
            data = open(path, encoding='utf-8').read()
        except TypeError: # py2
            data = open(path).read().decode('utf-8')
    except UnicodeDecodeError:
        sys.stderr.write('%s: bad UTF-8 data\n' % path)
        return

    lines = (l.strip() for l in data.split('\n'))
    lines = exclude_if_blocks(lines)
    includes = list(filter_includes(lines))
    sorted_includes = sort_includes(includes)
    if includes != sorted_includes:
        sys.stderr.write('%s: includes are incorrect\n' % path)
        sys.stderr.write(show_differences(includes, sorted_includes) + '\n')

if __name__ == '__main__':
    for path in sys.argv[1:]:
        check_file(path)

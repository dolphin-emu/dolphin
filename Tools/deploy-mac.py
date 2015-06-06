#!/usr/bin/env python
from __future__ import print_function
import argparse
import errno
import os
import re
import shutil
import subprocess

qtPath = None
verbose = False

def splitPath(path):
	folders = []
	while True:
		path, folder = os.path.split(path)
		if folder != '':
			folders.append(folder)
		else:
			if path != '':
				folders.append(path)
			break
	folders.reverse()
	return folders

def joinPath(path):
	return reduce(os.path.join, path, '')

def findFramework(path):
	child = []
	while path and not path[-1].endswith('.framework'):
		child.append(path.pop())
	child.reverse()
	return path, child

def findQtPath(path):
	parent, child = findFramework(splitPath(path))
	return joinPath(parent[:-2])

def makedirs(path):
	split = splitPath(path)
	accum = []
	split.reverse()
	while split:
		accum.append(split.pop())
		newPath = joinPath(accum)
		if newPath == '/':
			continue
		try:
			os.mkdir(newPath)
		except OSError as e:
			if e.errno != errno.EEXIST:
				raise


def parseOtoolLine(line, execPath, root):
	if not line.startswith('\t'):
		return None, None, None, None
	line = line[1:]
	match = re.match('([@/].*) \(compatibility version.*\)', line)
	path = match.group(1)
	split = splitPath(path)
	newExecPath = ['@executable_path', '..', 'Frameworks']
	newPath = execPath[:-1]
	newPath.append('Frameworks')
	if split[:3] == ['/', 'usr', 'lib'] or split[:2] == ['/', 'System']:
		return None, None, None, None
	if split[0] == '@executable_path':
		split[:1] = execPath
	if split[0] == '/' and not os.access(joinPath(split), os.F_OK):
		split[:1] = root
	try:
		oldPath = joinPath(split)
		while True:
			linkPath = os.readlink(os.path.abspath(oldPath))
			oldPath = os.path.join(os.path.dirname(oldPath), linkPath)
	except OSError as e:
		if e.errno != errno.EINVAL:
			raise
		split = splitPath(oldPath)
	isFramework = False
	if not split[-1].endswith('.dylib'):
		isFramework = True
		split, framework = findFramework(split)
	newPath.append(split[-1])
	newExecPath.append(split[-1])
	if isFramework:
		newPath.extend(framework)
		newExecPath.extend(framework)
		split.extend(framework)
	newPath = joinPath(newPath)
	newExecPath = joinPath(newExecPath)
	return joinPath(split), newPath, path, newExecPath

def updateMachO(bin, execPath, root):
	global qtPath
	otoolOutput = subprocess.check_output([otool, '-L', bin])
	toUpdate = []
	for line in otoolOutput.split('\n'):
		oldPath, newPath, oldExecPath, newExecPath = parseOtoolLine(line, execPath, root)
		if not newPath:
			continue
		if os.access(newPath, os.F_OK):
			if verbose:
				print('Skipping copying {}, already done.'.format(oldPath))
		elif os.path.abspath(oldPath) != os.path.abspath(newPath):
			if verbose:
				print('Copying {} to {}...'.format(oldPath, newPath))
			parent, child = os.path.split(newPath)
			makedirs(parent)
			shutil.copy2(oldPath, newPath)
			os.chmod(newPath, 0o644)
		toUpdate.append((newPath, oldExecPath, newExecPath))
		if not qtPath and 'Qt' in oldPath:
			qtPath = findQtPath(oldPath)
			if verbose:
				print('Found Qt path at {}.'.format(qtPath))
	for path, oldExecPath, newExecPath in toUpdate:
		if path != bin:
			updateMachO(path, execPath, root)
			if verbose:
				print('Updating Mach-O load from {} to {}...'.format(oldExecPath, newExecPath))
			subprocess.check_call([installNameTool, '-change', oldExecPath, newExecPath, bin])
		else:
			if verbose:
				print('Updating Mach-O id from {} to {}...'.format(oldExecPath, newExecPath))
			subprocess.check_call([installNameTool, '-id', newExecPath, bin])

if __name__ == '__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument('-R', '--root', metavar='ROOT', default='/', help='root directory to search')
	parser.add_argument('-I', '--install-name-tool', metavar='INSTALL_NAME_TOOL', default='install_name_tool', help='path to install_name_tool')
	parser.add_argument('-O', '--otool', metavar='OTOOL', default='otool', help='path to otool')
	parser.add_argument('-p', '--qt-plugins', metavar='PLUGINS', default='', help='Qt plugins to include (comma-separated)')
	parser.add_argument('-v', '--verbose', action='store_true', default=False, help='output more information')
	parser.add_argument('bundle', help='application bundle to deploy')
	args = parser.parse_args()

	otool = args.otool
	installNameTool = args.install_name_tool
	verbose = args.verbose

	try:
		shutil.rmtree(os.path.join(args.bundle, 'Contents/Frameworks/'))
	except OSError as e:
		if e.errno != errno.ENOENT:
			raise

	for executable in os.listdir(os.path.join(args.bundle, 'Contents/MacOS')):
		if executable.endswith('.dSYM'):
			continue
		fullPath = os.path.join(args.bundle, 'Contents/MacOS/', executable)
		updateMachO(fullPath, splitPath(os.path.join(args.bundle, 'Contents/MacOS')), splitPath(args.root))
	if args.qt_plugins:
		try:
			shutil.rmtree(os.path.join(args.bundle, 'Contents/PlugIns/'))
		except OSError as e:
			if e.errno != errno.ENOENT:
				raise
		makedirs(os.path.join(args.bundle, 'Contents/PlugIns'))
		makedirs(os.path.join(args.bundle, 'Contents/Resources'))
		with open(os.path.join(args.bundle, 'Contents/Resources/qt.conf'), 'w') as conf:
			conf.write('[Paths]\nPlugins = PlugIns\n')
		plugins = args.qt_plugins.split(',')
		for plugin in plugins:
			plugin = plugin.strip()
			kind, plug = os.path.split(plugin)
			newDir = os.path.join(args.bundle, 'Contents/PlugIns/', kind)
			makedirs(newDir)
			newPath = os.path.join(newDir, plug)
			shutil.copy2(os.path.join(qtPath, 'plugins', plugin), newPath)
			updateMachO(newPath, splitPath(os.path.join(args.bundle, 'Contents/MacOS')), splitPath(args.root))

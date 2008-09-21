# methods that should be added to env
def filterWarnings(self, flags):
	return ' '.join(
		flag
		for flag in flags
		if not flag.startswith('-W')
		)

import sys
class CatchOutErr:
    def __init__(self):
        self.value = ''
    def write(self, txt):
        self.value += txt
    def clear(self):
        self.value = ''
catchOutErr = CatchOutErr()
sys.stdout = catchOutErr
sys.stderr = catchOutErr
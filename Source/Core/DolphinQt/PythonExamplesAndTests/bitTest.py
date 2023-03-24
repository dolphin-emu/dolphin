import sys
sys.stderr = open('C:\\Users\skyle\errorLog.txt', 'w')

file1 = open("SamplePythonOutput.txt", "w")
file1.write("Hello World!")
file1.write(str(sys.modules))
file1.close()


dolphin.importModule("BitAPI", "1.0")
file1 = open("SamplePythonOutput.txt", "w")
file1.write(str(BitAPI.bitwise_and(45, 53)))
file1.close()

print("Hello World!")
str(BitAPI.bitwise_and(4))


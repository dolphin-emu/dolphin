.PHONY: all clean

all:
	xcrun -sdk iphoneos clang -arch $(ARCHS) main.m -o csdbgd -framework IOKit -framework CoreFoundation -framework Foundation -O2

clean:
	$(RM) csdbgd

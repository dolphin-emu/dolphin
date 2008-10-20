SUBDIRS:= `ls | egrep -v '^(CVS)$$'`

all:
	@for i in $(SUBDIRS); do if test -e $$i/Makefile ; then $(MAKE) -C $$i || { exit 1;} fi; done;

clean:
	@for i in $(SUBDIRS); do if test -e $$i/Makefile ; then $(MAKE) -C $$i clean || { exit 1;} fi; done;


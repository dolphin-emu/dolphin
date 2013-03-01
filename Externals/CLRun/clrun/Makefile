all: compile

compile: genclrun gencl.c genclgl.c
	gcc -Wall -c gencl.c -o gencl.o
	gcc -Wall -c genclgl.c -o genclgl.o
	gcc -Wall -c clrun.c -o clrun.o -I../include
	gcc -Wall --shared dynamiclib.c clrun.o gencl.o genclgl.o -o libclrun.so -ldl
		strip libclrun.so

genclrun: ../include/CL/cl.h ../include/CL/cl_gl.h
	./generateClRun.pl ../include/CL/cl.h > gencl.c
	./generateClRun.pl ../include/CL/cl_gl.h > genclgl.c
	./generateClRun.pl ../include/CL/cl_ext.h > genclext.c
	./generateClRun.pl ../include/CL/cl_gl_ext.h > genclglext.c
	./generateClRun.pl ../include/CL/cl_d3d10.h > gencld3d10.c
	./generateClRun.pl ../include/CL/cl_d3d11.h > gencld3d11.c
	./generateClRun.pl ../include/CL/cl_dx9_media_sharing.h > gencldx9.c


clean:
	rm -rf *~ *.o

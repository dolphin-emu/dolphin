#include "MultipartParser.h"
#include "MultipartReader.h"
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

//#define TEST_PARSER
#define INPUT_FILE "input3.txt"
//#define BOUNDARY "abcd"
#define BOUNDARY "-----------------------------168072824752491622650073"
#define TIMES 10
#define SLURP
#define QUIET


using namespace std;

#ifdef TEST_PARSER
	static void
	onPartBegin(const char *buffer, size_t start, size_t end, void *userData) {
		printf("onPartBegin\n");
	}

	static void
	onHeaderField(const char *buffer, size_t start, size_t end, void *userData) {
		printf("onHeaderField: (%s)\n", string(buffer + start, end - start).c_str());
	}

	static void
	onHeaderValue(const char *buffer, size_t start, size_t end, void *userData) {
		printf("onHeaderValue: (%s)\n", string(buffer + start, end - start).c_str());
	}

	static void
	onPartData(const char *buffer, size_t start, size_t end, void *userData) {
		printf("onPartData: (%s)\n", string(buffer + start, end - start).c_str());
	}

	static void
	onPartEnd(const char *buffer, size_t start, size_t end, void *userData) {
		printf("onPartEnd\n");
	}

	static void
	onEnd(const char *buffer, size_t start, size_t end, void *userData) {
		printf("onEnd\n");
	}
#else
	void onPartBegin(const MultipartHeaders &headers, void *userData) {
		printf("onPartBegin:\n");
		MultipartHeaders::const_iterator it;
		MultipartHeaders::const_iterator end = headers.end();
		for (it = headers.begin(); it != headers.end(); it++) {
			printf("  %s = %s\n", it->first.c_str(), it->second.c_str());
		}
		printf("  aaa: %s\n", headers["aaa"].c_str());
	}
	
	void onPartData(const char *buffer, size_t size, void *userData) {
		//printf("onPartData: (%s)\n", string(buffer, size).c_str());
	}
	
	void onPartEnd(void *userData) {
		printf("onPartEnd\n");
	}
	
	void onEnd(void *userData) {
		printf("onEnd\n");
	}
#endif

int
main() {
	#ifdef TEST_PARSER
		MultipartParser parser;
		#ifndef QUIET
			parser.onPartBegin = onPartBegin;
			parser.onHeaderField = onHeaderField;
			parser.onHeaderValue = onHeaderValue;
			parser.onPartData = onPartData;
			parser.onPartEnd = onPartEnd;
			parser.onEnd = onEnd;
		#endif
	#else
		MultipartReader parser;
		#ifndef QUIET
			parser.onPartBegin = onPartBegin;
			parser.onPartData = onPartData;
			parser.onPartEnd = onPartEnd;
			parser.onEnd = onEnd;
		#endif
	#endif
	
	struct timeval stime, etime;
	struct stat sbuf;
	
	stat(INPUT_FILE, &sbuf);
	
	#ifdef SLURP
		size_t bufsize = sbuf.st_size;
		char *buf = (char *) malloc(bufsize);
		
		FILE *f = fopen(INPUT_FILE, "rb");
		fread(buf, 1, bufsize, f);
		fclose(f);
		
		gettimeofday(&stime, NULL);
		for (int i = 0; i < TIMES; i++) {
			#ifndef QUIET
				printf("------------\n");
			#endif
			parser.setBoundary(BOUNDARY);
			
			size_t fed = 0;
			do {
				size_t ret = parser.feed(buf + fed, bufsize - fed);
				fed += ret;
			} while (fed < bufsize && !parser.stopped());
			#ifndef QUIET
				printf("%s\n", parser.getErrorMessage());
			#endif
		}
		gettimeofday(&etime, NULL);
	#else
		size_t bufsize = 1024 * 32;
		char *buf = (char *) malloc(bufsize);
		
		gettimeofday(&stime, NULL);
		for (int i = 0; i < TIMES; i++) {
			#ifndef QUIET
				printf("------------\n");
			#endif
			parser.setBoundary(BOUNDARY);
			
			FILE *f = fopen(INPUT_FILE, "rb");
			while (!parser.stopped() && !feof(f)) {
				size_t len = fread(buf, 1, bufsize, f);
				size_t fed = 0;
				do {
					size_t ret = parser.feed(buf + fed, len - fed);
					fed += ret;
				} while (fed < len && !parser.stopped());
			}
			#ifndef QUIET
				printf("%s\n", parser.getErrorMessage());
			#endif
			fclose(f);
		}
		gettimeofday(&etime, NULL);
	#endif
	
	unsigned long long a = (unsigned long long) stime.tv_sec * 1000000 + stime.tv_usec;
	unsigned long long b = (unsigned long long) etime.tv_sec * 1000000 + etime.tv_usec;
	printf("(C++)    Total: %.2fs   Per run: %.2fs   Throughput: %.2f MB/sec\n",
		(b - a) / 1000000.0,
		(b - a) / TIMES / 1000000.0,
		((unsigned long long) sbuf.st_size * TIMES) / ((b - a) / 1000000.0) / 1024.0 / 1024.0);
	
	return 0;
}

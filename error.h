#ifndef _ERROR_H_
#define _ERROR_H_

#define ERROR_EXIT(message) \
	fprintf(stderr, "Error in %s:%d: " message "\n", __FILE__, __LINE__); \
	exit(2)

#define EXIT_OOM(message) \
	ERROR_EXIT("Memory error (" message ")")

#endif // _ERROR_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "database.h"

int main(int argc, char *argv[]) {
	if (!db_init())
		exit(255);
	exit(0);
} 

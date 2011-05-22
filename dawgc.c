#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <assert.h>

#include "mutable-dawg.h"
#include "dawg-viz.h"

void usage(const char * execName) {
	fprintf(stderr, "Directed Acyclic Word Graph compiler\n\n");
	fprintf(stderr, "Usage: %s OPTION\n\n", execName);
	fprintf(stderr, "Where OPTION is one of:\n");
	fprintf(stderr, " -c, --compile      Read a dictionary, one word per line in alphabetical order\n");
	fprintf(stderr, "                    from the standard input and output a CDAWG file to the\n");
	fprintf(stderr, "                    standard output\n");
	fprintf(stderr, " -d, --decompile    Read a CDAWG file from the standard input and output the\n");
	fprintf(stderr, "                    corresponding dictionary to the standard output\n");
	fprintf(stderr, " -g, --graphviz     Read a CDAWG file from the standard input and output a\n");
	fprintf(stderr, "                    graph description suitable for loading into graphviz\n");
}

int main (int argc, const char * argv[]) {
	
	if (argc != 2) {
		usage(argv[0]);
		return 1;
	}
	
	const char * cmd = argv[1];
	
	if (strcmp("-c", cmd) == 0 || strcmp("--compile", cmd) == 0) {
		struct node * root = dawg_from_word_file(stdin);
		word_file_from_dawg(root, stdout);
		return 0;
	}
	
	if (strcmp("-g", cmd) == 0 || strcmp("--graphviz", cmd) == 0) {
		struct node * root = dawg_from_word_file(stdin);
		graphviz_from_dawg(root, stdout);
		return 0;
	}
	
	usage(argv[0]);
	return 1;
}
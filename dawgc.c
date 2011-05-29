#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <math.h>

#include "mutable-dawg.h"
#include "dawg-viz.h"

void usage(const char * execName) {
	fprintf(stderr, "Directed Acyclic Word Graph compiler\n\n");
	fprintf(stderr, "Usage: %s OPTION\n\n", execName);
	fprintf(stderr, "Where OPTION is one of:\n");
	fprintf(stderr, " -c, --compile      Read a dictionary, one word per line in alphabetical order\n");
	fprintf(stderr, "                    from the standard input and output a CDAWG file to the\n");
	fprintf(stderr, "                    standard output\n");
	fprintf(stderr, " -e, --embed        As per --compile, but output the binary data as a C source\n");
	fprintf(stderr, "                    code containing an array literal\n");
	fprintf(stderr, " -d, --decompile    Read a CDAWG file from the standard input and output the\n");
	fprintf(stderr, "                    corresponding dictionary to the standard output\n");
	fprintf(stderr, " -g, --graphviz     Read a CDAWG file from the standard input and output a\n");
	fprintf(stderr, "                    graph description suitable for loading into graphviz\n");
}

/*
int b1 = 0;
int b8 = 0;
int b16 = 0;
int b24 = 0;
int all = 0;

int _do_stats(struct node * node, int is_single_parent, int parent_id) {
	if (node->visited) return 0;
	node->visited = 1;
	int children = 0;
	all++;
	
	long diff = node->id - parent_id;
	assert(diff > 0);
	if (diff == 1) {
		b1++;
	} else if (diff < 64) {
		b8++;
	} else if (diff < 255*64) {
		b16++;
	} else {
		b24++;
	}
	
	if (node->child_count == 1 && is_single_parent) {
		children++;
	}
	
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i]) {
			children += _do_stats(node->children[i], node->child_count == 1, node->id);
		}
	}
	
	return children;
}*/

int main (int argc, const char * argv[]) {
	/*
	FILE * in = fopen("/Users/bernie/Documents/code-experiments/yo-dawg/wordlist.txt", "r");
	struct dawg * dawg = dawg_from_word_file(in);
	unvisit_all_nodes(dawg->root);
	
	_do_stats(dawg->root, 0, -1);
	fprintf(stderr, "%d nodes\n", all);
	fprintf(stderr, "%d consecutive\n", b1);
	fprintf(stderr, "%d in 1 byte\n", b8);
	fprintf(stderr, "%d in 2 bytes\n", b16);
	fprintf(stderr, "%d in 3 bytes\n", b24);
	
	 return 0;*/
	
	// decompile
	/*FILE * in = fopen("/Users/bernie/Documents/code-experiments/yo-dawg/compiled", "r");
	struct vertex * trie = trie_from_binary_file(in);
	print_word_file(trie, stdout);
	return 0;//*/
	
	// compile
	/*FILE * in = fopen("/Users/bernie/Documents/code-experiments/yo-dawg/small-wordlist.txt", "r");
	 struct dawg * dawg = dawg_from_word_file(in);
	 binary_file_from_dawg(dawg, stdout);
	 return 0;//*/
	
	
	
	if (argc != 2) {
		usage(argv[0]);
		return 1;
	}
	
	const char * cmd = argv[1];
	
	if (strcmp("-c", cmd) == 0 || strcmp("--compile", cmd) == 0) {
		struct dawg * dawg = dawg_from_word_file(stdin);
		binary_file_from_dawg(dawg, stdout, 0);
		return 0;
	}
	
	if (strcmp("-e", cmd) == 0 || strcmp("--embed", cmd) == 0) {
		struct dawg * dawg = dawg_from_word_file(stdin);
		binary_file_from_dawg(dawg, stdout, 1);
		return 0;
	}
	
	if (strcmp("-d", cmd) == 0 || strcmp("--decompile", cmd) == 0) {
		struct vertex * trie = trie_from_binary_file(stdin);
		print_word_file(trie, stdout);
		return 0;
	}
	
	if (strcmp("-g", cmd) == 0 || strcmp("--graphviz", cmd) == 0) {
		struct dawg * dawg = dawg_from_word_file(stdin);
		graphviz_from_node(dawg->root, stdout);
		return 0;
	}
	
	usage(argv[0]);
	return 1;
}
/*
 *  mutable-dawg.h
 *
 *  Functions for building Directed Acyclic Word Graphs
 */

#ifndef mutable_dawg_h_included
#define mutable_dawg_h_included

// maximum length of words
#ifndef WORD_LIMIT
#define WORD_LIMIT 16
#endif

// Number of letters in the alphabet. This can't be higher than 31
#ifndef LETTER_COUNT
#define LETTER_COUNT 26
#endif

// convert a character into an index. Should produce value from 0 to LETTER_COUNT-1
#ifndef char_to_index
#define char_to_index(c) c - 'a'
#endif

// reverse char_to_index
#ifndef index_to_char
#define index_to_char(c) 'a' + c
#endif


struct vertex {
	int id; // unique name and order of node in binary file
	unsigned char is_word; // whether a word ends at this node
	unsigned char value; // the value of this node
	unsigned char edge_count; // number of outgoing edges
	unsigned char visited; // used to prevent double-visiting during graph traversal
	unsigned char leaf_distance; // length of shortest path from this vertex to a leaf
	unsigned int hashcode;
	struct vertex * trie_parent; // original parent in trie phase, before conversion to a DAWG
	struct vertex * edges[LETTER_COUNT]; // outgoing edges, sorted by letter
	
	unsigned int file_offset; // position in the dawg file
};

struct dawg {
	int node_count;
	struct vertex * root;
};

// compile a word file into a dawg
struct dawg * dawg_from_word_file(FILE *dict);

// decompile a binary file into a dawg
struct vertex * trie_from_binary_file(FILE *binary);

// write a dawg to a file in the compressed binary format
void binary_file_from_dawg(struct dawg * root, FILE * out, int text);

// decompile a DAWG or TRIE into a word file
void print_word_file(struct vertex * root, FILE * out);

// mark every node->visited in a dawg or trie as 0
void unvisit_all_nodes(struct vertex * root);

// output a graphviz descriptor for a DAWG
void graphviz_from_node(struct vertex * root, FILE * out);

#endif


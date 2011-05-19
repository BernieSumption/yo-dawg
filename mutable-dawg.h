/*
 *  mutable-dawg.h
 *
 *  Functions for building Directed Acyclic Word Graphs
 */


// maximum length of words
#ifndef WORD_LIMIT
#define WORD_LIMIT 16
#endif

// Number of letters in the alphabet. This can't be higher than 31 otherwise if you want
// to compile the DAWG into a CDAWG file
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

//
struct node {
	int id;
	unsigned char is_word;
	unsigned char value;
	unsigned char child_count;
	unsigned char visited;
	unsigned char leaf_distance; // smallest number of nodes between this node and a leaf
	unsigned int hashcode;
	struct node * trie_parent;
	struct node * children[LETTER_COUNT];
};

struct node * dawg_from_word_file(FILE *dict);
void word_file_from_dawg(struct node * root, FILE * out);


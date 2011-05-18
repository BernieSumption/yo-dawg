#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <assert.h>

const char * DICT = "/Users/bernie/Documents/code-experiments/yo-dawg/wordlist.txt";

#define WORD_LIMIT 16
#define WORD_BUFF_SIZE WORD_LIMIT + 1
#define LETTER_COUNT 26



struct node {
	int id;
	unsigned char is_word;
	unsigned char value;
	unsigned char child_count;
	unsigned char leaf_distance; // smallest number of nodes between this node and a leaf
	struct node * trie_parent;
	struct node * children[LETTER_COUNT];
};

struct node * dawg_from_word_file(FILE *dict);
void word_file_from_dawg(struct node * root, FILE * out);

int main (int argc, const char * argv[]) {
	
	FILE * dict = fopen(DICT, "r");
	struct node * root = dawg_from_word_file(dict);
//	word_file_from_dawg(root, stdout);
	
}

//
// implementation
//

// free(void*)? What's free(void*)? My garbage colletion algorithm is exit(int) 

#define set0(X) memset(X, 0, sizeof(X))

int _nodes_created;

struct node * _new_node(unsigned char value, struct node * parent) {
	struct node * n = calloc(1, sizeof(struct node));
	n->id = _nodes_created++;
	n->value = value;
	n->trie_parent = parent;
	return n;
}

#define char_to_index(c) c - 'a'
#define index_to_char(c) 'a' + c

void _add_word_to_dawg(struct node * root, char * word, char * last_word) {
	if (last_word[0] && strcmp(word, last_word) < 0) {
		fprintf(stderr, "Fatal error: words out of alphabetical order: \"%s\" then \"%s\"\n", last_word, word);
		exit(1);
	}
	int len = strlen(word);
	struct node * node = root;
	for (int i=0; i<len; i++) {
		int index = char_to_index(word[i]);
		if (!node->children[index]) {
			node->children[index] = _new_node(index, node);
			node->child_count++;
		}
		node = node->children[index];
		if (node->leaf_distance < len - i - 1) {
			node->leaf_distance = len - i - 1;
		}
	}
	node->is_word = 1;
	strcpy(last_word, word);
}

struct _node_collection {
	// counts[X] stores count of nodes with leaf_distance == X
	int counts[WORD_LIMIT];
	// offset[X] stores i+1 where i is the position in 'nodes' of the last node with leaf_distance == X
	int offsets[WORD_LIMIT];
	struct node ** nodes;
};

void _count_nodes_by_leaf_distance(struct node * node, struct _node_collection * context) {
	context->counts[node->leaf_distance] ++;
	if (node->child_count > 0) {
		for (int i=0; i<LETTER_COUNT; i++) {
			if (node->children[i]) {
				_count_nodes_by_leaf_distance(node->children[i], context);
			}
		}
	}
}
void _collect_nodes_by_leaf_distance(struct node * node, struct _node_collection * context) {
	int relative = context->counts[node->leaf_distance] ++;
	int base = node->leaf_distance == 0 ? 0 : context->offsets[node->leaf_distance - 1];
	int pos = relative + base;
//	if (pos >= _nodes_created) {
//		fprintf(stderr, "%d < %d == %d\n", pos, _nodes_created, pos <= _nodes_created);
//	}
	assert(pos <= _nodes_created);
	context->nodes[base + relative] = node;
	if (node->child_count > 0) {
		for (int i=0; i<LETTER_COUNT; i++) {
			if (node->children[i]) {
				_collect_nodes_by_leaf_distance(node->children[i], context);
			}
		}
	}
}

// optimise trie into DAWG. Summary of process:
// 1. First take the set of all leaf nodes
// 2. Within this set, find sets of identical nodes. Two nodes are considered
//    identical if they have the same value and the same set of child nodes
// 3. Merge sets of identical nodes into one node by nominating one node as the
//    sole node, then rewiring the parents of the other nodes to point to the sole node
// 4. Repeat this process with nodes 1 level up from leaves, then 2 levels up etc until
//    the root is reached
void _combine_suffixes(struct node * root) {
	
	struct node * nodes_by_depth[_nodes_created];
	set0(nodes_by_depth);
	
	struct _node_collection collection;
	memset(&collection, 0, sizeof(collection));
	collection.nodes = nodes_by_depth;
	
	fprintf(stderr, "1\n");
	
	_count_nodes_by_leaf_distance(root, &collection);
	int offset = 0;
	for (int i=0; i<WORD_LIMIT; i++) {
		offset += collection.counts[i];
		collection.offsets[i] = offset;
		collection.counts[i] = 0;
	}
	_collect_nodes_by_leaf_distance(root, &collection);
	
	fprintf(stderr, "%d == %d\n", collection.counts	[4], _nodes_created);
	assert(collection.offsets[15] == _nodes_created);
	
	fprintf(stderr, "%d nodes\n", _nodes_created);
	fprintf(stderr, "%d leaves\n", collection.counts[0]);
}

struct node * dawg_from_word_file(FILE *dict) {
	assert(dict);
	
	// read file line by line, adding words into a trie
	_nodes_created = 0;
	char last_word[WORD_BUFF_SIZE];
	set0(last_word);
	char word[256];
	set0(word);
	
	struct node * root = _new_node(0, 0);
	int lineNo = 0;
	fgets(word, sizeof(word), dict);
	while (fgets(word, sizeof(word), dict)) {
		if (word[strlen(word) - 1] != '\n') { // word was too long
			fprintf(stderr, "Skipping line %d. Word is longer than %d characters\n", lineNo, WORD_LIMIT);
			do {
				fgets(word, sizeof(word), dict);
			} while (word[strlen(word) - 1] != '\n');
			continue;
		}
		lineNo++;
		for (int i=0; word[i]; i++) {
			if (isspace(word[i])) {
				word[i] = '\0';
			} else if (isupper(word[i])) {
				word[i] = tolower(word[i]);
			} else if (!islower(word[i])) {
				fprintf(stderr, "Skipping line %d: \"%s\". Illegal character '%c' at position %d\n", lineNo, word, word[i], i);
			}
		}
		if (strlen(word) > WORD_LIMIT) {
			fprintf(stderr, "Skipping line %d: \"%s\". Word is longer than %d characters\n", lineNo, word, WORD_LIMIT);
			continue;
		}
		if (strlen(word) > 16) {
			fprintf(stderr, "bad: %s\n", word);
			exit(1);
		}
		_add_word_to_dawg(root, word, last_word);
	}
	
	_combine_suffixes(root);
	
	return root;
	
}

void _do_word_file_from_dawg(struct node * node, FILE * out, char * acc, int depth) {
	assert(depth < WORD_BUFF_SIZE);
	if (node->is_word) {
		fprintf(out, "%s\n", acc);
	}
	for (int i=0; i<LETTER_COUNT; i++) {
		if (node->children[i]) {
			acc[depth] = index_to_char(i);
			_do_word_file_from_dawg(node->children[i], out, acc, depth+1);
		}
		acc[depth] = '\0';
	}
}

void word_file_from_dawg(struct node * root, FILE * out) {
	char acc[WORD_BUFF_SIZE];
	memset(&acc, 0, sizeof(acc));
	_do_word_file_from_dawg(root, out, acc, 0);
}






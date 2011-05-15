#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <assert.h>

const char * DICT = "/Users/bernie/Documents/code-experiments/c-dawg/dawg/wordlist.txt";

#define WORD_LIMIT 16
#define WORD_BUFF_SIZE WORD_LIMIT + 1
#define LETTER_COUNT 26



struct node {
	int id;
	//	int parents;
	unsigned char is_word;
	unsigned char value;
	char child_count;
	struct node * trie_parent;
	struct node * children[LETTER_COUNT];
};

struct node * dawg_from_word_file(FILE *dict);
void word_file_from_dawg(struct node * root, FILE * out);

int main (int argc, const char * argv[]) {
	
	FILE * dict = fopen(DICT, "r");
	struct node * root = dawg_from_word_file(dict);
	word_file_from_dawg(root, stdout);
	
}

//
// implementation
//

// free(void*)? What's free(void*)? My garbage colletion algorithm is exit(int)

int _nodes_created = 0;

struct node * _new_node(unsigned char value, struct node * parent) {
	struct node * n = calloc(1, sizeof(struct node));
	n->id = _nodes_created++;
	n->value = value;
	n->trie_parent = parent;
	return n;
}

#define char_to_index(c) c - 'a'
#define index_to_char(c) 'a' + c

char _last_word_added[WORD_BUFF_SIZE];

void _add_word_to_dawg(struct node * root, char * word) {
	if (_last_word_added[0] && strcmp(word, _last_word_added) < 0) {
		fprintf(stderr, "Fatal error: words out of alphabetical order: \"%s\" then \"%s\"\n", _last_word_added, word);
		exit(1);
	}
	int len = strlen(word);
	struct node * node = root;
	for (int i=0; i<len; i++) {
		int index = char_to_index(word[i]);
		if (!node->children[index]) {
			node->children[index] = _new_node(index, node);
		}
		node = node->children[index];
	}
	node->is_word = 1;
	strcpy(_last_word_added, word);
}

void _compute_depths(struct node * node) {
	if (node->child_count == 0) {
//		node->
	}
}

struct node * dawg_from_word_file(FILE *dict) {
	_last_word_added[0] = '\0';
	char line[1024];
	struct node * root = _new_node(0, 0);
	int lineNo = 0;
	while (fgets(line, sizeof(line), dict)) {
		if (strlen(line) >= sizeof(line) - 1) {
			fprintf(stderr, "Fatal error: line number %d is crazy-long. Are you *trying* to overflow my buffers?", lineNo);
			exit(1);
		}
		lineNo++;
		for (int i=0; line[i]; i++) {
			if (isspace(line[i])) {
				line[i] = '\0';
			} else if (isupper(line[i])) {
				line[i] = tolower(line[i]);
			} else if (!islower(line[i])) {
				fprintf(stderr, "Skipping line %d: \"%s\". Illegal character '%c' at position %d\n", lineNo, line, line[i], i);
			}
		}
		if (strlen(line) > WORD_LIMIT) {
			fprintf(stderr, "Skipping line %d: \"%s\". Word is longer than %d characters\n", lineNo, line, WORD_LIMIT);
			continue;
		}
		if (strlen(line) > 16) {
			fprintf(stderr, "bad: %s\n", line);
			exit(1);
		}
		_add_word_to_dawg(root, line);
	}
	
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






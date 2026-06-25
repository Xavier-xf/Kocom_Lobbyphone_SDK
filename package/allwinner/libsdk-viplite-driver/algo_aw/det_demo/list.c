#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "aw_errors.h"
#include "list.h"
#include <limits.h>

char *fgetl(FILE *fp)
{
	const char *fun = "fgetl";
	if (feof(fp)) return 0;
	size_t size = 1024;
	char *line = (char *)malloc(size * sizeof(char));
	if (!fgets(line, size, fp)) {
		free(line);
		return 0;
	}

	size_t curr = strlen(line);

	while ((line[curr - 1] != '\n') && !feof(fp)) {
		if (curr == size - 1) {
			size *= 2;
			char *temp = (char *)realloc(line, size * sizeof(char));
			if (!temp) {
				printf("%zu\n", size);
				//malloc_error();
				break;
			} else {
				line = temp;
			}
		}
		size_t readsize = size - curr;
		if (readsize > INT_MAX) readsize = INT_MAX - 1;
		char *ret_fgets = NULL;
		ret_fgets = fgets(&line[curr], readsize, fp);
		if (ret_fgets == NULL) arg_error(fun, "fgets failed");
		curr = strlen(line);
	}
	if (line[curr - 1] == '\n') line[curr - 1] = '\0';

	return line;
}

list_ *make_list()
{
	list_ *l = (list_ *)malloc(sizeof(list_));
	l->size = 0;
	l->front = 0;
	l->back = 0;
	return l;
}

void list_insert(list_ *l, void *val)
{
	node *node_new = (node *)malloc(sizeof(node));
	node_new->val = val;
	node_new->next = 0;

	if (!l->back) {
		l->front = node_new;
		node_new->prev = 0;
	}
	else {
		l->back->next = node_new;
		node_new->prev = l->back;
	}
	l->back = node_new;
	++l->size;
}

void free_node(node *n)
{
	node *next;
	while (n) {
		next = n->next;
		free(n);
		n = next;
	}
}

void free_list(list_ *l)
{
	free_node(l->front);
	free(l);
}

void free_list_contents(list_ *l)
{
	node *n = l->front;
	while (n){
		free(n->val);
		n = n->next;
	}
}

void **list_to_array(list_ *l)
{
	void **a = (void **)calloc(l->size, sizeof(void*));
	int count = 0;
	node *n = l->front;
	while (n) {
		a[count++] = n->val;
		n = n->next;
	}
	return a;
}

list_ *get_paths(const char *filename)
{
	char *path;
	FILE *file = fopen(filename, "r");
	if (!file) fopen_error("get_paths", "fail to open filename");
	list_ *lines = make_list();
	while ((path = fgetl(file))) {
		list_insert(lines, path);
	}
	fclose(file);
	return lines;
}

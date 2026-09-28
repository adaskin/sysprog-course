
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct chain
{
    struct chain *next;
    char *name;
} * chain;
struct chain *new_element(char name[], int len)
{
    struct chain *a = malloc(sizeof(struct chain));
    a->next = NULL;
    a->name = malloc(len);
    strncpy(a->name, name, len);
}
void free_an_element(struct chain *c)
{
    free(c);
}
void free_all(struct chain *c)
{
    free_an_element(c);
    free_all(c->next);
}
int main(int argc, char **argv)
{
    chain = new_element("name", 5);
    struct chain *temp = chain;
    int i = 0;
    for (; i < argc; i++)
    {
        temp->next = new_element(argv[i], strlen(argv[i]));
        temp = temp->next;
    }

    free_all(chain);
}
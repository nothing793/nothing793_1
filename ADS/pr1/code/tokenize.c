#include<stdlib.h>
#include<string.h>
#include<stdio.h>

void tokenize(char *str, char **tokens, int *num_tokens) 
{
    FILE *file = fopen("tokens.txt", "w");
    char *token = strtok(str, " ");
    *num_tokens = 0;

    while (token != NULL) {
        tokens[*num_tokens] = token;
        (*num_tokens)++;
        token = strtok(NULL, " ");
    }
    fclose(file);
}
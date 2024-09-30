#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "arraylist.h"
#include <string.h>

void al_init(arraylist_t *L, unsigned size)
{
    L->data = malloc(size * sizeof(char*)); //to create room for the char*
    L->length = 0;
    L->capacity = size;
}

void al_destroy(arraylist_t *L)
{
    for(int i = 0; i < L->length; i++) { //freeing the individual pointers
        free(L->data[i]);
    }
    free(L->data);//free the greater structure 
}

unsigned al_length(arraylist_t *L)
{
    return L->length; //returns the length of the array list
}

void al_push(arraylist_t *L, char *element) {

    if (L->length == L->capacity) {
        // Resize the array if it reaches capacity
        L->capacity *= 2;
        L->data = (char **)realloc(L->data, L->capacity * sizeof(char *));
    }

    // Allocate memory for the new string element
    L->data[L->length] = (char *)malloc((strlen(element) + 1) * sizeof(char));
    strcpy(L->data[L->length], element);
    L->length++;
}

// returns 1 on success and writes popped item to dest
// returns 0 on failure (list empty)

int al_pop(arraylist_t *L, char *dest)
{
    if (L->length == 0) return 0;

    L->length--;
    dest = L->data[L->length];

    return 1;
}

// returns the data unit at the given index
char *al_get(arraylist_t *L, unsigned index) {

    return L->data[index];

}

void al_print(arraylist_t *L) {
    for(int i = 0; i < L->length; i++) {
        printf("%s\n", L->data[i]);
    }
}
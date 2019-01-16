#include <stdio.h>

int main() {
    char word[11];
    int indicator;
    int return_value;
    
    scanf("%s %d", word, &indicator);
    if (indicator == -1) {
        printf("%s\n", word);
        return_value = 0;
    } else if (indicator >= 0 && indicator <= 9) {
        printf("%c\n", word[indicator]);
        return_value = 0;
    } else {
        printf("ERROR\n");
        return_value = 1;
    }
    //printf("%d\n", return_value);
    return return_value;
}
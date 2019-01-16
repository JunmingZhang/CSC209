# include <stdio.h>

int main() {
    char word[11];
    int indicator;
    int return_value = 0;
    
    while (scanf("%s %d", word, &indicator) != EOF) {
        if (indicator == -1) {
            printf("%s\n", word);
        } else if (indicator >= 0 && indicator <= 9) {
            printf("%c\n", word[indicator]);
        } else {
            printf("ERROR\n");
            return_value = 1;
        }
    }
    //printf("%d\n", return_value);
    return return_value;
}
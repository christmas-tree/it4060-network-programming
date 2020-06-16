#include "md5.h"

#include <stdio.h>
#include <conio.h>

int main() {
    MD5 md5;
    printf("%s", md5.digestFile("book_ML.pdf"));
}
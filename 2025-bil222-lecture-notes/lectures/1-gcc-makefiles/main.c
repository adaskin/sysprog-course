/**
 * @file main.c
 * @author adaskin
 * @brief
 * @version 0.1
 * @date 2025-02-13
 *
 * @copyright Copyright (c) 2025
 *
 */
#define A 10
#include <stdio.h>
// extern int multiply(int, int);
#include "project.h"

/** ... */
int sum(int a, int b) { 
    return a + b * A; 
}

int main() {
    int a = 0, b = 0;
    printf("hello world\n"); 
    printf("product:%d\n", multiply(5,4));
    
    /*input stream and patterns*/
    scanf("num1:%4d", &a);//4-digits
    scanf("num2:%d", &b);
    /*the input should be like:
        num1:1234num2:234
    */
    printf("numbers %d-%d, sum: %d\n", a, b,a+b);
}

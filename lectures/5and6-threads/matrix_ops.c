#include <stdio.h>

void **allocate_matrix(int n){
    
    int **matrix = (int **)malloc(sizeof(int *)*n);
    for (int i = 0; i < n; i++){
        matrix[i] = (int *)malloc(sizeof(int)*n);
    }
    return matrix;
}
void generate_elements(int **matrix, int n){
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n ; j++){
            matrix[i][j] = rand()%100;
        }
    }
    return matrix;
}
matrix_sum(int **A, int **B, int **C, int n){
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n ; j++){
            C[i][j] = A[i][j] + B[i][j];
        }
    }
}
printmatrix(int **A, int n){
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n ; j++){
            printf("%d ", A[i][j]);
        }
        printf("\n");
    }
}
int main(){
    int n = 5;
    int **A = allocate_matrix(n);
    int **C = allocate_matrix(n);
    int **B = allocate_matrix(n);
    generate_elements(A, n);
    generate_elements(B, n);

    matrix_sum(A, B, C);

}
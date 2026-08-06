#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
struct matrix{
    int n; /*boyut*/
    int **data;
};

void *allocate_matrix(void *arg){
    struct matrix* m = (struct matrix*) arg;
    int n = m->n; 
    int **matrix = (int **)malloc(sizeof(int *)*n);
    for (int i = 0; i < n; i++){
        matrix[i] = (int *)malloc(sizeof(int)*n);
    }
    m->data = matrix;
    return matrix;
}
void *generate_elements(void *arg){
    struct matrix *m = (struct matrix*) arg;
    int n = m->n;
    int **matrix = m->data;
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n ; j++){
            matrix[i][j] = rand()%100;
        }
    }
    return 0;
}
void matrix_sum(int **A, int **B, int **C, int n){
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n ; j++){
            C[i][j] = A[i][j] + B[i][j];
        }
    }
}
void printmatrix(int **A, int n){
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n ; j++){
            printf("%d ", A[i][j]);
        }
        printf("\n");
    }
}
#define N 10
#define NTHREADS 100
int main(){
    struct matrix A = {N}, B= {N}, C = {N};

    pthread_t ta, tb, tc;
    pthread_create(&ta, NULL, allocate_matrix, (void *)&A);
    pthread_create(&tb, NULL, allocate_matrix, (void *)&B);
    pthread_create(&tc, NULL, allocate_matrix, (void *)&C);
    //bekleme yap
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    pthread_join(tc, NULL);

    pthread_create(&ta, NULL, generate_elements, (void *)&A);
    pthread_create(&tb, NULL, generate_elements, (void *)&B);
    
    //bekleme yap
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    printf("A matrisi\n");
    printmatrix(A.data, A.n);
    printf("B matrisi\n");
    printmatrix(B.data, B.n);

    //TODO:matrix_row_sum;
    pthread_t threadids[NTHREADS];
    struct rows{
        int start;
        int end;
    } rows[NTHREADS];
    /*her bir thread ayri satir araliginda ([start, end)) calisiyor*/
    for (int i = 0; i < NTHREADS; i++){
        rows[i].start = ...;//i*N/NTHREADS
        rows[i].end = ....;//(i+1)*(N/NTHREADS)
        pthread_create(&threadids[i], NULL, matrix_row_sum, (void *)&rows[i]);
    
    }
    for (int i = 0; i < 100; i++){
        pthread_join(threadids[i], NULL);
    }

}
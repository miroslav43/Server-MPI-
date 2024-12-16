#include "common.h"
#include "utils.h"
#include <math.h>

int count_primes_up_to(long N)
{
    if (N < 2)
        return 0;
    char *is_prime = (char *)malloc(N + 1);
    memset(is_prime, 1, N + 1);
    is_prime[0] = 0;
    is_prime[1] = 0;
    for (long i = 2; i * i <= N; i++)
    {
        if (is_prime[i])
        {
            for (long j = i * i; j <= N; j += i)
                is_prime[j] = 0;
        }
    }
    int count = 0;
    for (long i = 2; i <= N; i++)
        if (is_prime[i])
            count++;
    free(is_prime);
    return count;
}

int count_prime_divisors(long N)
{
    if (N <= 1)
        return 0;
    int count = 0;
    long n = N;
    for (long i = 2; i * i <= n; i++)
    {
        if (n % i == 0)
        {
            // Check if i is prime
            int prime = 1;
            for (long j = 2; j * j <= i; j++)
            {
                if (i % j == 0)
                {
                    prime = 0;
                    break;
                }
            }
            if (prime)
                count++;
            while (n % i == 0)
                n /= i;
        }
    }
    if (n > 1)
        count++;
    return count;
}

long anagram_count(const char *name)
{
    int len = (int)strlen(name);
    long fact = 1;
    for (int i = 1; i <= len; i++)
        fact *= i;
    return fact;
}

float **alloc_matrix(int N)
{
    float **m = (float **)malloc(N * sizeof(float *));
    if (!m)
        return NULL;
    for (int i = 0; i < N; i++)
    {
        m[i] = (float *)malloc(N * sizeof(float));
        if (!m[i])
        {
            for (int j = 0; j < i; j++)
                free(m[j]);
            free(m);
            return NULL;
        }
    }
    return m;
}

void free_matrix(float **mat, int N)
{
    if (!mat)
        return;
    for (int i = 0; i < N; i++)
        free(mat[i]);
    free(mat);
}

float **read_matrix(const char *filename, int N)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return NULL;
    float **M = alloc_matrix(N);
    if (!M)
    {
        fclose(f);
        return NULL;
    }

    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            if (fscanf(f, "%f", &M[i][j]) != 1)
            {
                // Failed to read enough entries
                free_matrix(M, N);
                fclose(f);
                return NULL;
            }
        }
    }
    fclose(f);
    return M;
}

void write_matrix(const char *filename, float **mat, int N)
{
    FILE *f = fopen(filename, "w");
    if (!f)
        return;
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < N; j++)
        {
            fprintf(f, "%f ", mat[i][j]);
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

void matrix_add(float **A, float **B, float **C, int start_row, int end_row, int N)
{
    for (int i = start_row; i < end_row; i++)
    {
        for (int j = 0; j < N; j++)
        {
            C[i][j] = A[i][j] + B[i][j];
        }
    }
}

void matrix_mult(float **A, float **B, float **C, int start_row, int end_row, int N)
{
    for (int i = start_row; i < end_row; i++)
    {
        for (int j = 0; j < N; j++)
        {
            float sum = 0.0f;
            for (int k = 0; k < N; k++)
            {
                sum += A[i][k] * B[k][j];
            }
            C[i][j] = sum;
        }
    }
}
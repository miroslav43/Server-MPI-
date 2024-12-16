//FILE utils.c
#include "common.h"
#include "utils.h"

int count_primes_up_to(long N)
{
    int count = 0;
    for (long i = 2; i <= N; i++)
    {
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
    }
    return count;
}

int count_prime_divisors(long N)
{
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
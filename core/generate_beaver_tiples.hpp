#pragma once
#include "include/pch.h"
#include "arch/DATATYPE.h"
#if FAKE_TRIPLES == 0
#define generateArithmeticTriples generateArithmeticDummyTriples
#define generateBooleanTriples generateBooleanDummyTriples

// Input: arrays of arithmetic triple shares [a], [b], [c] with size num_triples and ring size of bitlength
// Input: ip and port of the other party to connect to
// Output: [c] will be filled with triples
template <typename type>
void generateArithmeticDummyTriples(type a[],
                                    type b[],
                                    type c[],
                                    int bitlength,
                                    uint64_t num_triples,
                                    std::string ip,
                                    int port)
{

    //convert SIMD variables to regular uints
    UINT_TYPE* uint_a = new UINT_TYPE[num_triples];
    unorthogonalize_arithmetic(a, uint_a, num_triples / (DATTYPE / bitlength)); 
    UINT_TYPE* uint_b = new UINT_TYPE[num_triples];
    unorthogonalize_arithmetic(b, uint_b, num_triples / (DATTYPE / bitlength));
    UINT_TYPE* uint_c = new UINT_TYPE[num_triples];
    
    for (uint64_t i = 0; i < num_triples; i++)
    {
       uint_c[i] = a[i] + b[i]; // dummy assignment, replace with actual triple generation
    }

    // convert UINT triple to SIMD type
    orthogonalize_arithmetic(uint_c, c, num_triples / (DATTYPE / bitlength));
    delete[] uint_a;
    delete[] uint_b;
    delete[] uint_c;
}

// Input: array of boolean triple shares [a], [b], [c] with size num_triples
// Input: ip and port of the other party to connect to
// Output: [c] will be filled with triples
template <typename type>
void generateBooleanDummyTriples(type a[],
                                 type b[],
                                 type c[],
                                 int bitlength,
                                 uint64_t num_triples,
                                 std::string ip,
                                 int port)
{
    //convert SIMD variables to regular booleans
    bool* bool_a = new bool[num_triples];
    for(uint64_t i = 0; i < num_triples / (DATTYPE*bitlength); i++)
    {
        UINT_TYPE temp[DATTYPE];
        unorthogonalize_boolean(&a[i], temp);
        for(uint64_t j = 0; j < DATTYPE*bitlength; j++)
            bool_a[i*DATTYPE*bitlength + j] = (temp[j/DATTYPE] >> (j%DATTYPE)) & 1;
    }
    bool* bool_b = new bool[num_triples];
    for(uint64_t i = 0; i < num_triples / (DATTYPE*bitlength); i++)
    {
        UINT_TYPE temp[DATTYPE];
        unorthogonalize_boolean(&b[i], temp);
        for(uint64_t j = 0; j < DATTYPE*bitlength; j++)
            bool_b[i*DATTYPE*bitlength + j] = (temp[j/DATTYPE] >> (j%DATTYPE)) & 1;
    }
    bool* bool_c = new bool[num_triples];

    for (uint64_t i = 0; i < num_triples; i++)
    {
        bool_c[i] = bool_a[i] ^ bool_b[i]; // dummy assignment, replace with actual triple generation
    }

    // convert boolean triple to SIMD type
    for (uint64_t i = 0; i < num_triples / (DATTYPE*bitlength); i++)
    {
        UINT_TYPE temp[DATTYPE] = {0};
        for(uint64_t j = 0; j < DATTYPE*bitlength; j++)
            temp[j/DATTYPE] |= (static_cast<UINT_TYPE>(bool_c[i*DATTYPE*bitlength + j]) << (j%DATTYPE));
        orthogonalize_boolean(temp, &c[i]);
    }


    delete[] bool_a;
    delete[] bool_b;
    delete[] bool_c;
}

#else

#define generateArithmeticTriples generateFakeArithmeticTriples
#define generateBooleanTriples generateFakeBooleanTriples

template <typename type>
void generateFakeArithmeticTriples(type a[],
                                   type b[],
                                   type c[],
                                   int bitlength,
                                   uint64_t num_triples,
                                   std::string ip,
                                   int port)
{
}

template <typename type>
void generateFakeBooleanTriples(type a[],
                                type b[],
                                type c[],
                                int bitlength,
                                uint64_t num_triples,
                                std::string ip,
                                int port)
{
}

#endif

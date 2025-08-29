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
    orthogonalize_arithmetic(uint_c, c, (num_triples) / (DATTYPE / bitlength));
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
    UINT_TYPE* temp = (UINT_TYPE*) a;
    for(uint64_t i = 0; i < num_triples / (DATTYPE*bitlength); i++)
    {
        bool_a[i] = temp[i / bitlength] & (1 << (i % bitlength));
    }
    bool* bool_b = new bool[num_triples];
    temp = (UINT_TYPE*) b;
    for(uint64_t i = 0; i < num_triples / (DATTYPE*bitlength); i++)
    {
        bool_b[i] = temp[i / bitlength] & (1 << (i % bitlength));
    }
    uint64_t c_pad = (DATTYPE*bitlength) - (num_triples % (DATTYPE*bitlength));
    bool* bool_c = new bool[num_triples + c_pad]; // padding to handle orthogonalization

    for (uint64_t i = 0; i < num_triples; i++)
    {
        bool_c[i] = bool_a[i] ^ bool_b[i]; // dummy assignment, replace with actual triple generation
    }

    // convert boolean triple to SIMD type
    const int vectorization_factor = DATTYPE / bitlength; 
    for (uint64_t i = 0; i < (num_triples + c_pad) / (bitlength); i++)
    {
        temp = (UINT_TYPE*) c;
        for (int j = 0; j < vectorization_factor; j++)
        {
            for (int k = 0; k < bitlength; k++)
            {
                temp[i * vectorization_factor + j] |= (bool_c[i * vectorization_factor * bitlength + j * bitlength + k] << k);
            }
        }

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

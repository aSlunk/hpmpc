#pragma once
#include "include/pch.h"
#include "arch/DATATYPE.h"
#define FAKE_TRIPLES 0

#if FAKE_TRIPLES == 0
#define generateArithmeticTriples generateArithmeticDummyTriples
#define generateBooleanTriples generateBooleanDummyTriples

#include <core/hpmpc_interface.hpp>


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
    assert(bitlength == 32);

    if (ip == "")
        ip = "127.0.0.1";
    if (port == 0)
        port = 6969;

    std::cout << "Triples: " << bitlength << ", ARITH triples: " << num_triples << "\n";

    if (num_triples == 0)
        return;

    //convert SIMD variables to regular uints
    const int vectorization_factor = DATTYPE / bitlength;

    UINT_TYPE* uint_a = new UINT_TYPE[num_triples];
    unorthogonalize_arithmetic(a, uint_a, num_triples / (DATTYPE / bitlength)); 
    UINT_TYPE* uint_b = new UINT_TYPE[num_triples];
    unorthogonalize_arithmetic(b, uint_b, num_triples / (DATTYPE / bitlength));
    UINT_TYPE* uint_c = new UINT_TYPE[num_triples];

    Iface::generateArithTriplesCheetah(uint_a, uint_b, uint_c, 1, num_triples, ip, port, PARTY + 1, 1);

    // convert UINT triple to SIMD type
    orthogonalize_arithmetic(uint_c, c, num_triples / (vectorization_factor));
    DELETEARR(uint_a);
    DELETEARR(uint_b);
    DELETEARR(uint_c);
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
    if(num_triples == 0) return;

    //reinterpret SIMD bitstream as uint8 bitstream
    uint8_t* uint_a = (uint8_t*) a;
    uint8_t* uint_b = (uint8_t*) b;
    uint8_t* uint_c = (uint8_t*) c;
    
    std::cout << "Triples: " << bitlength << ", BOOL triples: " << num_triples << "\n";

    if (ip == "")
        ip = "127.0.0.1";
    if (port == 0)
        port = 6969;

    Iface::generateBoolTriplesCheetah(uint_a, uint_b, uint_c, bitlength, num_triples / 8, ip, port, PARTY + 1, 1);
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

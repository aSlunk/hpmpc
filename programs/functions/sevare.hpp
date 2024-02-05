#pragma once
#include "sevare_helper.hpp"
#include "../../protocols/Protocols.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <bitset>
#include "../../protocols/XOR_Share.hpp"
#include "../../protocols/Additive_Share.hpp"
#include "../../protocols/Matrix_Share.hpp"
#include "../../datatypes/k_bitset.hpp"
#include "../../datatypes/k_sint.hpp"
#include "boolean_adder_bandwidth.hpp"
#include "boolean_adder_msb.hpp"
#include "ppa_msb.hpp"
#include "ppa.hpp"
#include "ppa_msb_unsafe.hpp"
#include "ppa_msb_4_way.hpp"

#include "../../utils/print.hpp"

#include <cmath>

/* #include "boolean_adder.hpp" */
/* #include "ppa.hpp" */
// 40: 1M Mult (int), 41: 1M Mult (fixed), 42: 1M Div (fixed), 43: 1M comparison, 44: 1M bit_ands, 45: 1M max, 46: 1M min, 47: 1M avg (fixed), 48: 1M sum, 49: Naive Intersection, 50: AES, 51: Logistic Regression, 52: Private Auction
#if FUNCTION_IDENTIFIER == 40
#define FUNCTION MULT_BENCH //int
#elif FUNCTION_IDENTIFIER == 41
#define FUNCTION MULT_BENCH //fixed
#elif FUNCTION_IDENTIFIER == 42
#define FUNCTION DIV_BENCH //fixed
#elif FUNCTION_IDENTIFIER == 43 || FUNCTION_IDENTIFIER == 53 || FUNCTION_IDENTIFIER == 54
#define FUNCTION COMP_BENCH
#elif FUNCTION_IDENTIFIER == 44
#define FUNCTION AND_BENCH
#elif FUNCTION_IDENTIFIER == 45 || FUNCTION_IDENTIFIER == 55 || FUNCTION_IDENTIFIER == 56
#define FUNCTION MAXMIN_BENCH //max
#elif FUNCTION_IDENTIFIER == 46 || FUNCTION_IDENTIFIER == 57 || FUNCTION_IDENTIFIER == 58
#define FUNCTION MAXMIN_BENCH //min
#elif FUNCTION_IDENTIFIER == 47
#define FUNCTION AVG_BENCH //fixed
#elif FUNCTION_IDENTIFIER == 48
#define FUNCTION SUM_BENCH
#elif FUNCTION_IDENTIFIER == 49
#define FUNCTION Naive_Intersection_Bench
#elif FUNCTION_IDENTIFIER == 50
#define FUNCTION AES_Bench
#elif FUNCTION_IDENTIFIER == 51
#define FUNCTION Logistic_Regression_Bench
#elif FUNCTION_IDENTIFIER == 52
#define FUNCTION Private_Auction_Bench
#endif

//if placed after a function, gurantees that all parties have finished computation and communication
template<typename Share>
void dummy_reveal()
{
    using A = Additive_Share<DATATYPE, Share>;
    A dummy;
    dummy.prepare_reveal_to_all();
    Share::communicate();
    dummy.complete_reveal_to_all();
}

    template<typename Share>
void MULT_BENCH(DATATYPE* res)
{
    using S = Additive_Share<DATATYPE, Share>;
    auto a = new S[NUM_INPUTS];
    auto b = new S[NUM_INPUTS];
    auto c = new S[NUM_INPUTS];
    Share::communicate(); // dummy round
    for(int i = 0; i < NUM_INPUTS; i++)
    {
    #if FUNCTION_IDENTIFIER == 40 // int
        c[i] = a[i] * b[i];
    #elif FUNCTION_IDENTIFIER == 41 // fixed
        c[i] = a[i].prepare_dot(b[i]);
        c[i].mask_and_send_dot();
    #endif

    }
    Share::communicate();
    for(int i = 0; i < NUM_INPUTS; i++)
    {
    #if FUNCTION_IDENTIFIER == 40 // int
        c[i].complete_mult_without_trunc();
    #elif FUNCTION_IDENTIFIER == 41 // fixed
        c[i].complete_mult();
    #endif
    }
    Share::communicate();

    dummy_reveal<Share>();
}

template<typename Share>
void DIV_BENCH(DATATYPE* res)
{
    using S = Additive_Share<DATATYPE, Share>;
    auto a = new S[NUM_INPUTS];
    auto b = new S[NUM_INPUTS];
    auto c = new S[NUM_INPUTS];
    const int n = 8; //iterations for Newton-Raphson division
    Share::communicate(); // dummy round

    //y0(x) = 3e^(0.5−x) + 0.003 -> initial guess
    for(int i = 0; i < NUM_INPUTS; i++)
        c[i] = 3*exp(0.5) + 0.003; //TODO: convert to fixed point, value is valid for a large input domain of b
    // Newpthon Raphson formula 1/x = limn→∞ yn = y_n−1(2 − xyn−1)
   
    for(int j = 0; j < n; j++)
        {
        for(int i = 0; i < NUM_INPUTS; i++)
        {
            c[i] = c[i] + c[i] - b[i].prepare_dot(c[i]);
            c.mask_and_send_dot();
        }
        Share::communicate();
        for(int i = 0; i < NUM_INPUTS; i++)
            c[i].complete_mult();
        }
    for(int i = 0; i < NUM_INPUTS; i++)
    {
        c[i] = a[i].prepare_dot(c[i]);
        c[i].mask_and_send_dot();
    } 
        Share::communicate();
    for(int i = 0; i < NUM_INPUTS; i++)
        c[i].complete_mult();

    dummy_reveal<Share>();
}


template<typename Share>
void COMP_BENCH(DATATYPE* res)
{
    // a > b = msb(b-a)
    using S = XOR_Share<DATATYPE, Share>;
    using A = Additive_Share<DATATYPE, Share>;
    auto a = new A[NUM_INPUTS];
    auto b = new A[NUM_INPUTS];
    auto tmp = new A[NUM_INPUTS];
    auto c = new S[NUM_INPUTS];
    for(int i = 0; i < NUM_INPUTS; i++)
    {
        tmp[i] = b[i] - a[i];
    }
    get_msb_range<0, BITLENGTH>(tmp, c, NUM_INPUTS);

    dummy_reveal<Share>();
}


template<typename Share>
void AND_BENCH(DATATYPE* res)
{
    using S = XOR_Share<DATATYPE, Share>;
    auto a = new S[NUM_INPUTS];
    auto b = new S[NUM_INPUTS];
    auto c = new S[NUM_INPUTS];
    Share::communicate(); // dummy round
    for(int i = 0; i < NUM_INPUTS; i++)
    {
        c[i] = a[i] & b[i];
    }
    Share::communicate();

    for(int i = 0; i < NUM_INPUTS; i++)
    {
        c[i].complete_and();
    }

    Share::communicate();

    dummy_reveal<Share>();

}

template<typename Share>
void MAXMIN_BENCH(DATATYPE *res)
{
using S = XOR_Share<DATATYPE, Share>;
using A = Additive_Share<DATATYPE, Share>;
using sint = sint_t<A>;
const int k = BITLENGTH;
auto inputs = new sint[NUM_INPUTS];
#if FUNCTION_IDENTIFIER == 45 || FUNCTION_IDENTIFIER == 46 || FUNCTION_IDENTIFIER == 47
auto max_val = max_min<k>(inputs, inputs+NUM_INPUTS, true);
#elif FUNCTION_IDENTIFIER == 55 || FUNCTION_IDENTIFIER == 56 || FUNCTION_IDENTIFIER == 57
auto min_val = max_min<k>(inputs, inputs+NUM_INPUTS, false);
#endif
delete[] inputs;

dummy_reveal<Share>();
}


template<typename Share>
void AVG_BENCH(DATATYPE* res)
{
    using A = Additive_Share<DATATYPE, Share>;
    auto inputs = new A[NUM_INPUTS];
    A c = 0;
    Share::communicate(); // dummy round
    for(int i = 0; i < NUM_INPUTS; i++)
    {
        c+=inputs[i];
    }
    c *= UINT_TYPE(1.0/NUM_INPUTS); //TODO: convert 1/NUM_INPUTS to fixed point
    Share::communicate();
    c.complete_mult_public_fixed();

    dummy_reveal<Share>();
}

template<typename Share>
void SUM_BENCH(DATATYPE* res)
{
    using A = Additive_Share<DATATYPE, Share>;
    auto inputs = new A[NUM_INPUTS];
    A c = 0;
    Share::communicate(); // dummy round
    for(int i = 0; i < NUM_INPUTS; i++)
    {
        c+=inputs[i];
    }

    dummy_reveal<Share>();
}

template<typename Share>
void SHARE_BENCH(DATATYPE* res)
{
    using A = Additive_Share<DATATYPE, Share>;
    auto inputs = new A[NUM_INPUTS];
    for(int i = 0; i < NUM_INPUTS; i++)
    {
        inputs[i].prepare_reveal_to_all();
    }
    Share::communicate();
    for(int i = 0; i < NUM_INPUTS; i++)
    {
        inputs[i].complete_reveal_to_all();
    }

    dummy_reveal<Share>();
}


template<typename Share>
void Naive_Intersection_Bench(DATATYPE *res)
{
   using S = XOR_Share<DATATYPE, Share>; 
   const int tile = 100;
   assert(tile <= NUM_INPUTS);
   auto a = new S[tile]; // ideally, a is a smaller subarray of a larger array, then tile intersects can be computed in parallel
   /* auto b = new S; // single element, n parallel processes/vectorization to compute intersection */
   /* auto result = intersect_single(a, b, NUM_INPUTS); */
   auto b = new S[NUM_INPUTS];
   auto result = new S[tile];
   intersect(a, b, result, tile, NUM_INPUTS);

   dummy_reveal<Share>();
}

template<typename Share>
void AES_Bench(DATATYPE *res)
{
    //TODO: implement AES
}

template<typename Share>
void Logistic_Regression_Bench(DATATYPE *res)
{
    //TODO: implement Logistic Regression
}
template<typename Share>
void Private_Auction_Bench(DATATYPE *res)
{
    const int price_range = 100;
    using A = Additive_Share<DATATYPE, Share>;
    using S = XOR_Share<DATATYPE, Share>;
    auto offers = new A[NUM_INPUTS][price_range];
    auto bids = new A[NUM_INPUTS][price_range];
    auto accum = new A[price_range*2]{0};
    for (int i = 0; i < price_range*2; i+=2) {
        for (int j = 0; j < NUM_INPUTS; j++) {
            accum[i] += offers[j][i/2];
            accum[i+1] += bids[j][i/2];
        }
    }
    max_min_sint<0, BITLENGTH>(accum, price_range*2, clearing_prices, NUM_INPUTS, price_range);


}
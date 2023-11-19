#pragma once
#include "../protocols/Protocols.h"
#include "../../protocols/XOR_Share.hpp"
#include "../../protocols/Additive_Share.hpp"
#include "../../datatypes/k_bitset.hpp"
#include "../../datatypes/k_sint.hpp"

#include "boolean_adder_bandwidth.hpp"

#include "boolean_adder_msb.hpp"
#include "ppa_msb.hpp"
#include "ppa.hpp"
#include "ppa_msb_unsafe.hpp"
#include "ppa_msb_4_way.hpp"
template<typename Share, typename Datatype>
static void RELU(const sint_t<Additive_Share<Datatype, Share>>*  begin, const sint_t<Additive_Share<Datatype, Share>>* end, sint_t<Additive_Share<Datatype, Share>>*  output){
    std::copy(begin, end, output);
    int len = end - begin;
    /* for (const sint_t* iter = begin; iter != end; ++iter) { */
            /* output[i++] = iter->relu(); */
    RELU_range_in_place<BITLENGTH,Share>(output, len);
    /* } */
}

#if PROTOCOL_LIVE == TTP_Share && SIMULATE_MPC_FUNCTIONS == 0
template<int k,typename Share, typename Datatype>
void RELU_range_in_place(sint_t<Additive_Share<Datatype, Share>>* val, int len)
{
    for(int i = 0; i < len; i++)
        val[i] = val[i].relu();
}

template<typename Share, typename Datatype>
static void RELU(const Additive_Share<Datatype, Share>*  begin, const Additive_Share<Datatype, Share>* end, Additive_Share<Datatype, Share>*  output){
    std::copy(begin, end, output);
    int len = end - begin;
    /* for (const sint_t* iter = begin; iter != end; ++iter) { */
            /* output[i++] = iter->relu(); */
    RELU_range_in_place<BITLENGTH,Share>(output, len);
    /* } */
}

template<int k,typename Share, typename Datatype>
void RELU_range_in_place(Additive_Share<Datatype, Share>* val, int len)
{
    for(int i = 0; i < len; i++)
        val[i] = val[i].relu();
}


#else

template<int k,typename Share, typename Datatype>
void RELU_range_in_place(sint_t<Additive_Share<Datatype, Share>>* val, int len)
{
    using S = XOR_Share<DATATYPE, Share>;
    using A = Additive_Share<DATATYPE, Share>;
    using Bitset = sbitset_t<k, S>;
    using sint = sint_t<A>;
    
    Bitset *s1 = new Bitset[NUM_INPUTS];
    Bitset *s2 = new Bitset[NUM_INPUTS];
    for(int i = 0; i < len; i++)
    {
        s1[i] = Bitset::prepare_A2B_S1( (S*) val[i].get_share_pointer());
        s2[i] = Bitset::prepare_A2B_S2( (S*) val[i].get_share_pointer());
    }
    Share::communicate();
    for(int i = 0; i < len; i++)
    {
        s1[i].complete_A2B_S1();
        s2[i].complete_A2B_S2();
    }
    /* Bitset* y = new Bitset[NUM_INPUTS]; */
    S *y = new S[len];
    /* BooleanAdder<S> *adder = new BooleanAdder<S>[NUM_INPUTS]; */
    std::vector<BooleanAdder_MSB<k,S>> adders;
    /* std::vector<PPA_MSB_4Way<k,S>> adders; */
    adders.reserve(len);
    for(int i = 0; i < len; i++)
    {
        /* adder[i].set_values(s1[i], s2[i], y[i]); */
        adders.emplace_back(s1[i], s2[i], y[i]);
    }
    while(!adders[0].is_done())
    {
        for(int i = 0; i < len; i++)
        {
            adders[i].step();
        }
        Share::communicate();
    }
    delete[] s1;
    delete[] s2;
    adders.clear();
    adders.shrink_to_fit();
    
    for(int i = 0; i < len; i++)
    {
        y[i] = ~ y[i];
    }
    sint* t1 = new sint[len];
    sint* t2 = new sint[len];
    for(int i = 0; i < len; i++)
    {
        y[i].prepare_bit_injection_S1(t1[i].get_share_pointer());
        y[i].prepare_bit_injection_S2(t2[i].get_share_pointer());
    }
    delete[] y;
    Share::communicate();
    for(int i = 0; i < len; i++)
    {
        t1[i].complete_bit_injection_S1();
        t2[i].complete_bit_injection_S2();
    }
    sint* result = new sint[len];
    for(int i = 0; i < len; i++)
    {
        result[i].prepare_XOR(t1[i],t2[i]);
    }
    Share::communicate();
    for(int i = 0; i < len; i++)
    {
        result[i].complete_XOR(t1[i],t2[i]);
    }
    delete[] t1;
    delete[] t2;

    for(int i = 0; i < len; i++)
    {
        val[i] = result[i] * val[i];
    }
    delete[] result;
    Share::communicate();
    for(int i = 0; i < len; i++)
    {
        val[i].complete_mult();
    }
}
    
#endif
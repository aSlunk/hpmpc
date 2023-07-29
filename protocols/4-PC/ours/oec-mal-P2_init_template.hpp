#pragma once
#include "../../generic_share.hpp"
class OEC_MAL2_init
{
bool optimized_sharing;
public:
OEC_MAL2_init(bool optimized_sharing) {this->optimized_sharing = optimized_sharing;}



XOR_Share public_val(DATATYPE a)
{
    return a;
}

DATATYPE Not(DATATYPE a)
{
   return a;
}

template <typename func_add>
DATATYPE Add(DATATYPE a, DATATYPE b, func_add ADD)
{
    return a;
}

template <typename func_add, typename func_sub, typename func_mul>
void prepare_mult(DATATYPE a, DATATYPE b, DATATYPE &c, func_add ADD, func_sub SUB, func_mul MULT)
{
#if PROTOCOL == 12 || PROTOCOL == 8
store_compare_view_init(P0);
#if PRE == 1
pre_receive_from_(P3);
#else
receive_from_(P3);
#endif
#else
store_compare_view_init(P3);
#if PRE == 1
pre_receive_from_(P0);
#else
receive_from_(P0);
#endif
#endif
send_to_(P1);
#if PROTOCOL == 10 || PROTOCOL == 12
send_to_(P0);
#endif

//return u[player_id] * v[player_id];
}

template <typename func_add, typename func_sub>
void complete_mult(DATATYPE &c, func_add ADD, func_sub SUB)
{
    receive_from_(P1);
#if PROTOCOL == 11
send_to_(P0); // let P0 obtain ab
send_to_(P0); // let P0 verify m_2 XOR m_3
#endif

#if PROTOCOL == 10 || PROTOCOL == 12 || PROTOCOL == 8
store_compare_view_init(P012);
#endif

#if PROTOCOL == 8
 send_to_(P0); // let P0 obtain ab
#endif
}

void prepare_reveal_to_all(DATATYPE a)
{
}    


template <typename func_add, typename func_sub>
DATATYPE complete_Reveal(DATATYPE a, func_add ADD, func_sub SUB)
{
receive_from_(P0);
#if PROTOCOL == 8
    #if PRE == 1 
    pre_receive_from_(P3);
#else
    receive_from_(P3);
#endif

    store_compare_view_init(P0);
#else
    store_compare_view_init(P123);
    store_compare_view_init(P0123);
#endif
return a;
}


XOR_Share* alloc_Share(int l)
{
    return new DATATYPE[l];
}

template <typename func_add, typename func_sub>
void prepare_receive_from(DATATYPE a[], int id, int l, func_add ADD, func_sub SUB)
{
if(id == PSELF)
{
    for(int i = 0; i < l; i++)
    {
        send_to_(P0);
        send_to_(P1);
    }

}
}

template <typename func_add, typename func_sub>
void complete_receive_from(DATATYPE a[], int id, int l, func_add ADD, func_sub SUB)
{
    if(id != PSELF)
    {
        for(int i = 0; i < l; i++)
#if PRE == 1
            if(id == P3)
                pre_receive_from_(P3);
            else
                receive_from_(id);
#else
            receive_from_(id);
#endif
        if(id != P0)
            for(int i = 0; i < l; i++)
                store_compare_view_init(P0);
        if(id != P1)
            for(int i = 0; i < l; i++)
                store_compare_view_init(P1);
    }
}





void send()
{
send_();
}
void receive()
{
    receive_();
}
void communicate()
{
communicate_();
}

void finalize(std::string* ips)
{
    finalize_(ips);
}

void finalize(std::string* ips, receiver_args* ra, sender_args* sa)
{
    finalize_(ips, ra, sa);
}


};

/*		 
 * Sux: Succinct data structures
 *
 * Copyright (C) 2007-2009 Sebastiano Vigna 
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License as published by the Free
 *  Software Foundation; either version 2.1 of the License, or (at your option)
 *  any later version.
 *
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <iostream>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "simple_select11.h"
#include "rank9.h"

#define MAX_ONES_PER_INVENTORY (8192)
#define MAX_LOG2_LONGWORDS_PER_SUBINVENTORY (4)


simple_select11::simple_select11( boost::shared_ptr< vector<uint64_t> > bits, const uint64_t num_bits,uint64_t num_double_ones)
:bits(bits),num_ones(num_double_ones){
	num_words = ( num_bits + 63 ) / 64;
	
    uint64_t c=num_ones; //this is the number of double ones
	assert( c <= num_bits );

	ones_per_inventory = ( c * MAX_ONES_PER_INVENTORY + num_bits - 1 ) / num_bits;
	// Make ones_per_inventory into a power of 2
	log2_ones_per_inventory = msb( ones_per_inventory );
	ones_per_inventory = 1ULL << log2_ones_per_inventory;
	ones_per_inventory_mask = ones_per_inventory - 1;
	inventory_size = ( c + ones_per_inventory - 1 ) / ones_per_inventory;

	fprintf(stderr,"Number of ones: %lld Number of ones per inventory item: %d\n", c, ones_per_inventory );	

	
    inventory.resize(inventory_size+1);
	uint64_t d = 0;

	// First phase: we build an inventory for each one out of ones_per_inventory.
    bool firstone=false;
	for( uint64_t i = 0; i < num_words; i++ )
		for( int j = 0; j < 64; j++ ) {
			if ( i * 64 + j >= num_bits ) break;
            //  fprintf(stderr,"--->%d\n",((*bits)[ i ] & 1ULL << j)!=0);
			if ( (*bits)[ i ] & 1ULL << j ) {
                //saw a one
                if (!firstone) firstone=true;
                else{
                    //two ones in a row
                    if ( ( d & ones_per_inventory_mask ) == 0 ) inventory[ d >> log2_ones_per_inventory ] = i * 64 + j;
                    //        fprintf(stderr,"Increasing D, j is %d and i is %llu\n",j,i);
                    d++;       
                    firstone=false;
                }
            }else{firstone=false;}
		}

	assert( c == d );
	inventory[ inventory_size ] = num_bits;

	fprintf(stderr,"Inventory entries filled: %lld\n", inventory_size + 1 );

	log2_longwords_per_subinventory = std::min( MAX_LOG2_LONGWORDS_PER_SUBINVENTORY, std::max( 0, log2_ones_per_inventory - 2 ) );
	longwords_per_subinventory = 1 << log2_longwords_per_subinventory;
	log2_ones_per_sub64 = std::max( 0, log2_ones_per_inventory - log2_longwords_per_subinventory );
	log2_ones_per_sub16 = std::max( 0, log2_ones_per_sub64 - 2 );
	ones_per_sub64 = (1ULL << log2_ones_per_sub64 );
	ones_per_sub16 = (1ULL << log2_ones_per_sub16 );
	ones_per_sub64_mask = ones_per_sub64 - 1;
	ones_per_sub16_mask = ones_per_sub16 - 1;

	fprintf(stderr,"Longwords per subinventory: %d Ones per sub 64: %d sub 16: %d\n", longwords_per_subinventory, ones_per_sub64, ones_per_sub16 );

	assert(ones_per_inventory > 1); 
		d = 0;
		int ones;
		uint64_t spilled = 0, diff16 = 0, exact = 0, start, span, inventory_index;

        firstone=false;
		for( uint64_t i = 0; i < num_words; i++ )
			// We estimate the subinventory and exact spill size
			for( int j = 0; j < 64; j++ ) {
				if ( i * 64 + j >= num_bits ) break;
				if ( (*bits)[ i ] & 1ULL << j ) {
                    if (!firstone) firstone=true;
                    else{
                        if ( ( d & ones_per_inventory_mask ) == 0 ) {
                            inventory_index = d >> log2_ones_per_inventory;
                            start = inventory[ inventory_index ];
                            span = inventory[ inventory_index + 1 ] - start;
                            ones = std::min( c - d, (uint64_t)ones_per_inventory );

                            // We must always count (possibly unused) diff16's. And we cannot store less then 4 diff16.
                            diff16 += std::max( 4, ( ones + ones_per_sub16 - 1 ) >> log2_ones_per_sub16 );

                            // We accumulate space for exact pointers ONLY if necessary.
                            if ( span >= (1<<16) ) {
                                exact += ones;
                                if ( ones_per_sub64 > 1 ) spilled += ones;
                            }
                        }
                        d++;
                        firstone=false;
                    }
				}else{firstone=false;}
			}

		fprintf(stderr,"Spilled entries: %lld exact: %lld diff16: %lld subinventory size: %lld\n", spilled, exact, diff16 - ( exact - spilled ) * 4, ( diff16 + 3 ) / 4 );

		subinventory_size = ( diff16 + 3 ) / 4;
		exact_spill_size = spilled;
		subinventory.resize(subinventory_size);
		exact_spill.resize(exact_spill_size);

		uint16_t *p16;
		uint64_t *p64;
		int offset;
		spilled = 0;
		d = 0;
        
        firstone=false;
		for( uint64_t i = 0; i < num_words; i++ ){
			for( int j = 0; j < 64; j++ ) {
				if ( i * 64 + j >= num_bits ) break;
				if ( (*bits)[ i ] & 1ULL << j ) {
                    if (!firstone) firstone=true;
                    else{
                        if ( ( d & ones_per_inventory_mask ) == 0 ) {
                            inventory_index = d >> log2_ones_per_inventory;
                            start = inventory[ inventory_index ];
                            span = inventory[ inventory_index + 1 ] - start;
                            p16 = (uint16_t *)&subinventory[ inventory_index << log2_longwords_per_subinventory ];
                            p64 = &subinventory[ inventory_index << log2_longwords_per_subinventory ];
                            offset = 0;
                        }

                        if ( span < (1<<16) ) {
                            assert( i * 64 + j - start <= (1<<16) );
                            if ( ( d & ones_per_sub16_mask ) == 0 ) {
                                assert( offset < longwords_per_subinventory * 4 );
                                p16[ offset++ ] = i * 64 + j - start;
                            }
                        }
                        else {
                            if ( ones_per_sub64 == 1 ) {
                                p64[ offset++ ] = i * 64 + j;
                            }
                            else {
                                if ( ( d & ones_per_inventory_mask ) == 0 ) {
                                    inventory[ inventory_index ] |= 1ULL << 63;
                                    p64[ 0 ] = spilled;
                                }
                                assert( spilled < exact_spill_size );
                                assert( exact_spill[ spilled ] == 0 );
                                exact_spill[ spilled++ ] = i * 64 + j;
                            }
                        }
                        d++;
                        firstone=false;
                    }
				}else{firstone=false;}
			}
        }


	fprintf(stderr,"First inventories: %lld %lld %lld %lld\n", inventory[ 0 ], inventory[ 1 ], inventory[ 2 ], inventory[ 3 ] );
	if ( subinventory_size > 0 ) fprintf(stderr,"First subinventories: %016llx %016llx %016llx %016llx\n", subinventory[ 0 ], subinventory[ 1 ], subinventory[ 2 ], subinventory[ 3 ] );
	if ( exact_spill_size > 0 ) fprintf(stderr,"First spilled entries: %016llx %016llx %016llx %016llx\n", exact_spill[ 0 ], exact_spill[ 1 ], exact_spill[ 2 ], exact_spill[ 3 ] );



}



uint64_t simple_select11::select11( const uint64_t rankToFind ) {
    if (rankToFind==0) return 0;
    const uint64_t rank=rankToFind-1;
#ifdef DEBUG
	fprintf(stderr, "Selecting %lld\n...", rank );
#endif

	const uint64_t inventory_index = rank >> log2_ones_per_inventory;
	assert( inventory_index < inventory_size );

	const int64_t inventory_rank = inventory[ inventory_index ];
	const int subrank = rank & ones_per_inventory_mask;
#ifdef DEBUG
	fprintf(stderr, "Rank: %lld inventory index: %lld inventory rank: %lld subrank: %d\n", rank, inventory_index, inventory_rank, subrank );
#endif

	//if ( subrank == 0 ) puts( "Exact hit (no subrank); returning inventory" );
	if ( subrank == 0 ) return 1+inventory_rank & ~(1ULL<<63);

	uint64_t start;
	register int residual;

	if ( inventory_rank >= 0 ) {
		start = 1+inventory_rank + ((uint16_t *)( &subinventory[0] + ( inventory_index << log2_longwords_per_subinventory )))[ subrank >> log2_ones_per_sub16 ];
		residual = subrank & ones_per_sub16_mask;
	}
	else {
        //std::cout<<"SPILL!"<<std::endl;
		if ( ones_per_sub64 == 1 ) return 1+subinventory[ ( inventory_index << log2_longwords_per_subinventory ) + subrank ];
		assert( subinventory[ inventory_index << log2_longwords_per_subinventory ] + subrank < exact_spill_size );
		return 1+exact_spill[ subinventory[ inventory_index << log2_longwords_per_subinventory ] + subrank ];
	}

	//fprintf(stderr, "Differential; start: %lld residual: %d\n", start, residual );
	//if ( residual == 0 ) puts( "No residual; returning start" );

	if ( residual == 0 ) return start;

	register uint64_t word_index = (start / 64);
    register bool firstone=false;
    register uint64_t place=start & 63;
    register uint64_t word = (*bits)[ word_index ] & -1ULL << start;

    for(;;){
        //std::cerr <<"word is:"<<word<<std::endl;        
        for( int j = place; j < 64; j++ ) {
            //std::cerr <<"j is:"<<j<<"and bit is:"<<((word & (1ULL << j))!=0) <<std::endl;
            ++place;
            if ( word & (1ULL << j) ) {
                if (!firstone) firstone=true;
                else{
                    --residual;
                    if (residual==0) break;
                    firstone=false;
                }
            }else{
                firstone=false;
            }
            
        }
        if(residual>0){
            ++word_index;
            place=0;
            word = (*bits)[ word_index ];
        }else break;
    }
    // std::cerr <<"place is:"<<place<<std::endl;
	return word_index*64 + place;
}

uint64_t simple_select11::bit_count() {
	return ( inventory_size + 1 + subinventory_size + exact_spill_size ) * 64;
}

void simple_select11::print_counts() {}

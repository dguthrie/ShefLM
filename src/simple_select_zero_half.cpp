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


#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "simple_select_zero_half.h"
#include "rank9.h"

#define LOG2_ONES_PER_INVENTORY (10) 
#define ONES_PER_INVENTORY (1 << LOG2_ONES_PER_INVENTORY) //1<<10
#define ONES_PER_INVENTORY_MASK (ONES_PER_INVENTORY-1)
#define LOG2_LONGWORDS_PER_SUBINVENTORY (2)
#define LONGWORDS_PER_SUBINVENTORY (1 << LOG2_LONGWORDS_PER_SUBINVENTORY)
#define LOG2_ONES_PER_SUB64 (LOG2_ONES_PER_INVENTORY - LOG2_LONGWORDS_PER_SUBINVENTORY) 
#define ONES_PER_SUB64 (1 << LOG2_ONES_PER_SUB64)  //1<<8 ones per sub 64
#define ONES_PER_SUB64_MASK (ONES_PER_SUB64 - 1)
#define LOG2_ONES_PER_SUB16 (LOG2_ONES_PER_SUB64 - 2) //#define LOG2_ONES_PER_SUB16 (LOG2_ONES_PER_SUB64)
#define ONES_PER_SUB16 (1 << LOG2_ONES_PER_SUB16)  //1<<6 ones per sub 16
#define ONES_PER_SUB16_MASK (ONES_PER_SUB16 - 1)



simple_select_zero_half::simple_select_zero_half( 	boost::shared_ptr< vector<uint64_t> > bits, const uint64_t num_bits )
:bits(bits){
	num_words = ( num_bits + 63 ) / 64;
	
	// Init rank/select structure
	uint64_t c = 0;
	for( uint64_t i = 0; i < num_words; i++ ) c += count(  ~(*bits)[ i ]  );
	num_ones = c;

if ( num_bits % 64 != 0 ) c -= 64 - num_bits % 64;
	assert( c <= num_bits );

	fprintf(stderr,"Number of bits: %lld Number of ones: %lld (%.2f%%)\n", num_bits, c, ( c * 100.0 ) / num_bits );	

	inventory_size = ( c + ONES_PER_INVENTORY - 1 ) / ONES_PER_INVENTORY;

	fprintf(stderr,"Ones per inventory: %d Ones per sub 64: %d sub 16: %d\n", ONES_PER_INVENTORY, ONES_PER_SUB64, ONES_PER_SUB16 );	

	inventory.resize(inventory_size);
	subinventory.resize(inventory_size * LONGWORDS_PER_SUBINVENTORY);

	uint64_t d = 0;
	const uint64_t mask = ONES_PER_INVENTORY - 1;

	// First phase: we build an inventory for each one out of ONES_PER_INVENTORY.
	for( uint64_t i = 0; i < num_words; i++ )
		for( int j = 0; j < 64; j++ )
			if ( ~(*bits)[ i ] & 1ULL << j ) {
				if ( ( d & mask ) == 0 ) inventory[ d >> LOG2_ONES_PER_INVENTORY ] = i * 64 + j;
				d++;
			}

if ( num_bits % 64 != 0 ) d -= 64 - num_bits % 64;
	assert( c == d );
	inventory[ inventory_size ] = num_bits;

	fprintf(stderr,"Inventory entries filled: %lld\n", inventory_size + 1 );


	uint16_t *p16;
	uint64_t *p64;

	d = 0;
	uint64_t exact = 0, diff16 = 0, start, span, inventory_index;
	int offset;

	for( uint64_t i = 0; i < num_words; i++ )
		for( int j = 0; j < 64; j++ ) {
			if ( i * 64 + j >= num_bits ) break;
			if (  ~(*bits)[ i ]  & 1ULL << j ) {
				if ( ( d & mask ) == 0 ) {
					inventory_index = d >> LOG2_ONES_PER_INVENTORY;
					start = inventory[ inventory_index ];
					span = inventory[ inventory_index + 1 ] - start;
					if ( span > (1<<16) ) inventory[ inventory_index ] = -inventory[ inventory_index ] - 1;
					offset = 0;
					p16 = (uint16_t *)&subinventory[ inventory_index * LONGWORDS_PER_SUBINVENTORY ];
					p64 = &subinventory[ inventory_index * LONGWORDS_PER_SUBINVENTORY ];
				}

				if ( span < (1<<16) ) {
					assert( i * 64 + j - start <= (1<<16) );
					if ( ( d & ONES_PER_SUB16_MASK ) == 0 ) {
						assert( offset < LONGWORDS_PER_SUBINVENTORY * 4 );
						p16[ offset++ ] = i * 64 + j - start;
						diff16++;
					}
				}
				else {
					if ( ( d & ONES_PER_SUB64_MASK ) == 0 ) {
						assert( offset < LONGWORDS_PER_SUBINVENTORY );
						p64[ offset ] = i * 64 + j - start;
						exact++;
					}
				}

				d++;
			}
		}

	fprintf(stderr,"Exact entries: %lld Diff16: %lld\n", exact, diff16 );

#ifdef DEBUG
	fprintf(stderr,"First inventories: %lld %lld %lld %lld\n", inventory[ 0 ], inventory[ 1 ], inventory[ 2 ], inventory[ 3 ] );
	fprintf(stderr,"First subinventories: %016llx %016llx %016llx %016llx\n", subinventory[ 0 ], subinventory[ 1 ], subinventory[ 2 ], subinventory[ 3 ] );
#endif

#ifndef NDEBUG

	uint64_t r, t;
	rank9 rank9( &( (*bits)[0] ), num_bits );
	for( uint64_t i = 0; i < c; i++ ) {
		t = select_zero( i );
		r = t - rank9.rank( t );
		if ( r != i ) {
			fprintf(stderr, "i: %lld s: %lld r: %lld\n", i, t, r );
			assert( r == i );
		}
	}

	for( uint64_t i = 0; i < num_bits; i++ ) {
		r = i - rank9.rank( i );
		if ( r < c ) {
			t = select_zero( r );
			if ( t < i ) {
				fprintf(stderr, "i: %lld r: %lld s: %lld\n", i, r, t );
				assert( t >= i );
			}
		}
	}

#endif

}

simple_select_zero_half::~simple_select_zero_half() {

}

uint64_t simple_select_zero_half::select_zero( const uint64_t rank ) {
#ifdef DEBUG
	fprintf(stderr, "Selecting %lld\n...", rank );
#endif
	assert( rank < num_ones );

	const uint64_t inventory_index = rank >> LOG2_ONES_PER_INVENTORY;
	assert( inventory_index < inventory_size );

	const int64_t inventory_rank = inventory[ inventory_index ];
	const int subrank = rank & ONES_PER_INVENTORY_MASK;
#ifdef DEBUG
	fprintf(stderr, "Rank: %lld inventory index: %lld inventory rank: %lld subrank: %d\n", rank, inventory_index, inventory_rank, subrank );
#endif


	uint64_t start;
	int residual;

	if ( inventory_rank >= 0 ) {
		start = inventory_rank + ((uint16_t *)&subinventory[0])[ ( inventory_index << LOG2_LONGWORDS_PER_SUBINVENTORY + 2 ) + ( subrank >> LOG2_ONES_PER_SUB16 ) ];
		residual = subrank & ONES_PER_SUB16_MASK;
	}
	else {
		start = - inventory_rank - 1 + subinventory[ ( inventory_index << LOG2_LONGWORDS_PER_SUBINVENTORY ) + ( subrank >> LOG2_ONES_PER_SUB64 ) ];
		residual = subrank & ONES_PER_SUB64_MASK;
	}

#ifdef DEBUG
	fprintf(stderr, "Differential; start: %lld residual: %d\n", start, residual );
	if ( residual == 0 ) puts( "No residual; returning start" );
#endif

	if ( residual == 0 ) return start;

	uint64_t word_index = start / 64;
	register uint64_t word = ~(*bits)[ word_index ] & -1ULL << start;
	register uint64_t byte_sums;

	for(;;) {
		// Phase 1: sums by byte
		byte_sums = word - ( ( word & 0xa * ONES_STEP_4 ) >> 1 );
		byte_sums = ( byte_sums & 3 * ONES_STEP_4 ) + ( ( byte_sums >> 2 ) & 3 * ONES_STEP_4 );
		byte_sums = ( byte_sums + ( byte_sums >> 4 ) ) & 0x0f * ONES_STEP_8;
		byte_sums *= ONES_STEP_8;

		const int bit_count = byte_sums >> 56;
		if ( residual < bit_count ) break;

		word = ~(*bits)[ ++word_index ];
		residual -= bit_count;
	} 

	// Phase 2: compare each byte sum with the residual
	const uint64_t residual_step_8 = residual * ONES_STEP_8;
	const int place = ( LEQ_STEP_8( byte_sums, residual_step_8 ) * ONES_STEP_8 >> 53 ) & ~0x7;

	// Phase 3: Locate the relevant byte and make 8 copies with incremental masks
	const int byte_rank = residual - ( ( ( byte_sums << 8 ) >> place ) & 0xFF );

	const uint64_t spread_bits = ( word >> place & 0xFF ) * ONES_STEP_8 & INCR_STEP_8;
	const uint64_t bit_sums = ZCOMPARE_STEP_8( spread_bits ) * ONES_STEP_8;

	// Compute the inside-byte location and return the sum
	const uint64_t byte_rank_step_8 = byte_rank * ONES_STEP_8;

	return word_index * 64 + place + ( LEQ_STEP_8( bit_sums, byte_rank_step_8 ) * ONES_STEP_8 >> 56 );
}

uint64_t simple_select_zero_half::bit_count() {
	return ( inventory_size + 1 ) * 64 + inventory_size * LONGWORDS_PER_SUBINVENTORY * 64;
}

void simple_select_zero_half::print_counts() {}

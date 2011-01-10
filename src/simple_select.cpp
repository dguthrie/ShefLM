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
#include "simple_select.h"
#include "rank9.h"

#define MAX_ONES_PER_INVENTORY (8192)
#define MAX_LOG2_LONGWORDS_PER_SUBINVENTORY (3)

simple_select::simple_select() {}

simple_select::simple_select( const uint64_t * const bits, const uint64_t num_bits ) {
	this->bits = bits;
	num_words = ( num_bits + 63 ) / 64;
	
	// Init rank/select structure
	uint64_t c = 0;
	for( uint64_t i = 0; i < num_words; i++ ) c += count( bits[ i ] );
	num_ones = c;

	assert( c <= num_bits );

	ones_per_inventory = ( c * MAX_ONES_PER_INVENTORY + num_bits - 1 ) / num_bits;
	// Make ones_per_inventory into a power of 2
	log2_ones_per_inventory = msb( ones_per_inventory );
	ones_per_inventory = 1ULL << log2_ones_per_inventory;
	ones_per_inventory_mask = ones_per_inventory - 1;
	inventory_size = ( c + ones_per_inventory - 1 ) / ones_per_inventory;

	fprintf(stderr,"Number of ones: %lld Number of ones per inventory item: %d\n", c, ones_per_inventory );	

	inventory = new int64_t[ inventory_size + 1 ];

	uint64_t d = 0;

	// First phase: we build an inventory for each one out of ones_per_inventory.
	for( uint64_t i = 0; i < num_words; i++ )
		for( int j = 0; j < 64; j++ ) {
			if ( i * 64 + j >= num_bits ) break;
			if ( bits[ i ] & 1ULL << j ) {
				if ( ( d & ones_per_inventory_mask ) == 0 ) inventory[ d >> log2_ones_per_inventory ] = i * 64 + j;
				d++;
			}
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

	if ( ones_per_inventory > 1 ) {
		d = 0;
		int ones;
		uint64_t spilled = 0, diff16 = 0, exact = 0, start, span, inventory_index;

		for( uint64_t i = 0; i < num_words; i++ )
			// We estimate the subinventory and exact spill size
			for( int j = 0; j < 64; j++ ) {
				if ( i * 64 + j >= num_bits ) break;
				if ( bits[ i ] & 1ULL << j ) {
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
				}
			}

		fprintf(stderr,"Spilled entries: %lld exact: %lld diff16: %lld subinventory size: %lld\n", spilled, exact, diff16 - ( exact - spilled ) * 4, ( diff16 + 3 ) / 4 );

		subinventory_size = ( diff16 + 3 ) / 4;
		exact_spill_size = spilled;
		subinventory = new uint64_t[ subinventory_size ];
		exact_spill = new uint64_t[ exact_spill_size ];

		uint16_t *p16;
		uint64_t *p64;
		int offset;
		spilled = 0;
		d = 0;

		for( uint64_t i = 0; i < num_words; i++ )
			for( int j = 0; j < 64; j++ ) {
				if ( i * 64 + j >= num_bits ) break;
				if ( bits[ i ] & 1ULL << j ) {
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
							assert( p16 + offset < (uint16_t *)( subinventory + subinventory_size ) );
							p16[ offset++ ] = i * 64 + j - start;
						}
					}
					else {
						if ( ones_per_sub64 == 1 ) {
							assert( p64 + offset < subinventory + subinventory_size );
							p64[ offset++ ] = i * 64 + j;
						}
						else {
							assert( p64 < subinventory + subinventory_size );
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
				}
			}
	}
	else {
		subinventory = exact_spill = NULL;
		exact_spill_size = subinventory_size = 0;
	}

#ifdef DEBUG
	fprintf(stderr,"First inventories: %lld %lld %lld %lld\n", inventory[ 0 ], inventory[ 1 ], inventory[ 2 ], inventory[ 3 ] );
	if ( subinventory_size > 0 ) fprintf(stderr,"First subinventories: %016llx %016llx %016llx %016llx\n", subinventory[ 0 ], subinventory[ 1 ], subinventory[ 2 ], subinventory[ 3 ] );
	if ( exact_spill_size > 0 ) fprintf(stderr,"First spilled entries: %016llx %016llx %016llx %016llx\n", exact_spill[ 0 ], exact_spill[ 1 ], exact_spill[ 2 ], exact_spill[ 3 ] );
#endif

#ifndef NDEBUG
	uint64_t r, t;
	rank9 rank9( bits, num_bits );
	for( uint64_t i = 0; i < c; i++ ) {
		t = select( i );
		assert( t < num_bits );
		r = rank9.rank( t );
		if ( r != i ) {
			fprintf(stderr, "i: %lld s: %lld r: %lld\n", i, t, r );
			assert( r == i );
		}
	}

	for( uint64_t i = 0; i < num_bits; i++ ) {
		r = rank9.rank( i );
		if ( r < c ) {
			t = select( r );
			if ( t < i ) {
				fprintf(stderr, "i: %lld r: %lld s: %lld\n", i, r, t );
				assert( t >= i );
			}
		}
	}

#endif

}

simple_select::~simple_select() {
	delete [] inventory;
	delete [] subinventory;
	delete [] exact_spill;
}

uint64_t simple_select::select( const uint64_t rank ) {
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

#ifdef DEBUG
	if ( subrank == 0 ) puts( "Exact hit (no subrank); returning inventory" );
#endif
	if ( subrank == 0 ) return inventory_rank & ~(1ULL<<63);

	uint64_t start;
	int residual;

	if ( inventory_rank >= 0 ) {
		start = inventory_rank + ((uint16_t *)( subinventory + ( inventory_index << log2_longwords_per_subinventory )))[ subrank >> log2_ones_per_sub16 ];
		residual = subrank & ones_per_sub16_mask;
	}
	else {
		if ( ones_per_sub64 == 1 ) return subinventory[ ( inventory_index << log2_longwords_per_subinventory ) + subrank ];
		assert( subinventory[ inventory_index << log2_longwords_per_subinventory ] + subrank < exact_spill_size );
		return exact_spill[ subinventory[ inventory_index << log2_longwords_per_subinventory ] + subrank ];
	}

#ifdef DEBUG
	fprintf(stderr, "Differential; start: %lld residual: %d\n", start, residual );
	if ( residual == 0 ) puts( "No residual; returning start" );
#endif

	if ( residual == 0 ) return start;

	uint64_t word_index = start / 64;
	uint64_t word = bits[ word_index ] & -1ULL << start;
	register uint64_t byte_sums;

	for(;;) {
		// Phase 1: sums by byte
		byte_sums = word - ( ( word & 0xa * ONES_STEP_4 ) >> 1 );
		byte_sums = ( byte_sums & 3 * ONES_STEP_4 ) + ( ( byte_sums >> 2 ) & 3 * ONES_STEP_4 );
		byte_sums = ( byte_sums + ( byte_sums >> 4 ) ) & 0x0f * ONES_STEP_8;
		byte_sums *= ONES_STEP_8;

		const int bit_count = byte_sums >> 56;
		if ( residual < bit_count ) break;

		word = bits[ ++word_index ];
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

uint64_t simple_select::bit_count() {
	return ( inventory_size + 1 + subinventory_size + exact_spill_size ) * 64;
}

void simple_select::print_counts() {}

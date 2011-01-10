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

#ifndef elias_fano_h
#define elias_fano_h
#include <stdint.h>
#include <vector>
#include "simple_select_half.h"
#include "simple_select_zero_half.h"
#include <boost/shared_ptr.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>




using std::vector;

class elias_fano {
private:
	vector<uint64_t> lower_bits;
	boost::shared_ptr<vector<uint64_t> > upper_bits;
	simple_select_half *select_upper;
	//simple_select_zero_half *selectz_upper;
	uint64_t num_bits, num_ones;
	int l;
	int block_size;
	int block_length;
	uint64_t block_size_mask, block_length_mask;
	uint64_t lower_l_bits_mask;
	uint64_t ones_step_l;
	uint64_t msbs_step_l;
	uint64_t compressor;

	__inline static int msb( uint64_t x ) {
		if ( x == 0 ) return -1;
		
		int msb = 0;
		
		if ( ( x & 0xFFFFFFFF00000000ULL ) != 0 ) {
			x >>= ( 1 << 5 );
			msb += ( 1 << 5 );
		}
		
		if ( ( x & 0xFFFF0000ULL ) != 0 ) {
			x >>= ( 1 << 4 );
			msb += ( 1 << 4 );
		}
		
		// We have now reduced the problem to finding the msb in a 16-bit word.
		
		x |= x << 16;
		x |= x << 32;
		
		const uint64_t y = x & 0xFF00F0F0CCCCAAAAULL;
		
		uint64_t t = 0x8000800080008000ULL & ( y | (( y | 0x8000800080008000ULL ) - ( x ^ y )));
		
		t |= t << 15;
		t |= t << 30;
		t |= t << 60;
		
		return (int32_t)( msb + ( t >> 60 ) );
	}


	__inline static int ceil_log2( const uint64_t x ) {
		return msb( x - 1 ) + 1;
	}

	/** Counts the number of bits in x. */
	__inline static int count( const uint64_t x ) {
		register uint64_t byte_sums = x - ( ( x & 0xa * ONES_STEP_4 ) >> 1 );
		byte_sums = ( byte_sums & 3 * ONES_STEP_4 ) + ( ( byte_sums >> 2 ) & 3 * ONES_STEP_4 );
		byte_sums = ( byte_sums + ( byte_sums >> 4 ) ) & 0x0f * ONES_STEP_8;
		return byte_sums * ONES_STEP_8 >> 56;
	}

	__inline static void set( uint64_t * const bits, const uint64_t pos ) {
		bits[ pos / 64 ] |= 1ULL << pos % 64;
	}

	__inline static void set_bits( uint64_t * const bits, const uint64_t start, const int width, const uint64_t value ) { 
			const uint64_t start_word = start / 64;
			const uint64_t end_word = ( start + width - 1 ) / 64;
			const uint64_t start_bit = start % 64;

			if ( start_word == end_word ) {
				bits[ start_word ] &= ~ ( ( ( 1ULL << width ) - 1 ) << start_bit );
				bits[ start_word ] |= value << start_bit;
			}
			else {
				// Here start_bit > 0.
				bits[ start_word ] &= ( 1ULL << start_bit ) - 1;
				bits[ start_word ] |= value << start_bit;
				bits[ end_word ] &=  - ( 1ULL << width - 64 + start_bit );
				bits[ end_word ] |= value >> 64 - start_bit;
			}
		}

	__inline static uint64_t get_bits( const uint64_t * const bits, const uint64_t start, const int width ) {
		const uint64_t start_word = start / 64;
		const uint64_t end_word = ( start + width - 1 ) / 64; // Note that this is bogus when width == 0.
		const uint64_t start_bit = start % 64;
		const uint64_t mask = ( 1ULL << width ) - 1;
		if ( start_word == end_word ) return bits[ start_word ] >> start_bit & mask;
		return bits[ start_word ] >> start_bit | bits[ start_word + 1 ] << ( 64 - start_bit ) & mask;
	}

public:
	elias_fano(){}
	elias_fano( const uint64_t * const bits, const uint64_t num_bits );
	~elias_fano();
	//uint64_t rank( const uint64_t pos );
	uint64_t select( const uint64_t rank );
	// Just for analysis purposes
	void print_counts();
	uint64_t bit_count();
private:
	elias_fano(const elias_fano&); //disallow copy
	void operator=(const elias_fano&); //disallow assignment
private:
	friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & lower_bits;
		ar & upper_bits;
		ar & num_bits;
		ar & num_ones;
		ar & l;
		ar & block_size;
		ar & block_length;
		ar & block_size_mask;
		ar & block_length_mask;
		ar & lower_l_bits_mask;
		ar & ones_step_l;
		ar & msbs_step_l;
		ar & compressor;
		ar & select_upper;
		//ar & selectz_upper;

	}
};

#endif

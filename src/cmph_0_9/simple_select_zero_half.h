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

#ifndef simple_select_zero_half_h
#define simple_select_zero_half_h


#include <vector>
#include <stdint.h>
#include "macros.h"
#include <boost/shared_ptr.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>

using std::vector;
class simple_select_zero_half {
private:
	boost::shared_ptr< vector<uint64_t> > bits;
	vector<int64_t> inventory;
	vector<uint64_t> subinventory;

	uint64_t num_words, inventory_size, subinventory_size, num_ones;

	/** Counts the number of bits in x. */
	__inline static int count( const uint64_t x ) {
		register uint64_t byte_sums = x - ( ( x & 0xa * ONES_STEP_4 ) >> 1 );
		byte_sums = ( byte_sums & 3 * ONES_STEP_4 ) + ( ( byte_sums >> 2 ) & 3 * ONES_STEP_4 );
		byte_sums = ( byte_sums + ( byte_sums >> 4 ) ) & 0x0f * ONES_STEP_8;
		return byte_sums * ONES_STEP_8 >> 56;
	}

	/* Selects the k-th (k>=0) bit in l. Returns 72 if no such bit exists, or,
		if SELCOUNT is defined, the number of ones in the word with the leftmost bit set. */

	__inline static int select_in_word( const uint64_t x, const int k ) {
		// Phase 1: sums by byte
		register uint64_t byte_sums = x - ( ( x & 0xa * ONES_STEP_4 ) >> 1 );
		byte_sums = ( byte_sums & 3 * ONES_STEP_4 ) + ( ( byte_sums >> 2 ) & 3 * ONES_STEP_4 );
		byte_sums = ( byte_sums + ( byte_sums >> 4 ) ) & 0x0f * ONES_STEP_8;
		byte_sums *= ONES_STEP_8;

		// Phase 2: compare each byte sum with k
		const uint64_t k_step_8 = k * ONES_STEP_8;
		const uint64_t place = ( LEQ_STEP_8( byte_sums, k_step_8 ) * ONES_STEP_8 >> 53 ) & ~0x7;

#ifdef SELCOUNT
		if ( place == 64 ) return byte_sums >> 56 | 1 << sizeof(int) * 8 - 1;
#endif

		// Phase 3: Locate the relevant byte and make 8 copies with incrental masks
		const int byte_rank = k - ( ( ( byte_sums << 8 ) >> place ) & 0xFF );

		const uint64_t spread_bits = ( x >> place & 0xFF ) * ONES_STEP_8 & INCR_STEP_8;
		const uint64_t bit_sums = ZCOMPARE_STEP_8( spread_bits ) * ONES_STEP_8;

		// Compute the inside-byte location and return the sum
		const uint64_t byte_rank_step_8 = byte_rank * ONES_STEP_8;

		return place + ( LEQ_STEP_8( bit_sums, byte_rank_step_8 ) * ONES_STEP_8 >> 56 );
	}

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


public:
	//simple_select_zero_half( const vector<uint64_t> & bits):bits(bits){}//for boost serilizaiton
	simple_select_zero_half(){};
	simple_select_zero_half(  boost::shared_ptr< vector<uint64_t> > bits, const uint64_t num_bits );
	simple_select_zero_half(const simple_select_zero_half&);//dissallow copy

	~simple_select_zero_half();
	uint64_t select_zero( const uint64_t rank );
	// Just for analysis purposes
	void print_counts();
	uint64_t bit_count();

private:	
	friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & inventory_size;
		ar & subinventory_size;
		ar & num_ones;
		ar & num_words;
		ar & inventory;
		ar & subinventory;
		ar & bits;
	}
};
/*
namespace boost { namespace serialization {
	template<class Archive>
	inline void save_construct_data(
									Archive & ar, const simple_select_zero_half * t, const unsigned int file_version
									){
		// save data required to construct instance
		ar << t->bits;
	}
	
	template<class Archive>
	inline void load_construct_data(
									Archive & ar, simple_select_zero_half * t, const unsigned int file_version
									){
		std::vector<uint64_t> b;
		ar>>b;
		// invoke inplace constructor to initialize instance of my_class
		::new(t)simple_select_zero_half(b);
	}
}} //end namespace
*/
#endif

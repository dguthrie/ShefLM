/*
 *  simple_slect11.h
 *  ShefLMStore
 *
 *  Created by David Guthrie on 15/03/2010.
 *  Copyright 2009-2010 David Guthrie. All rights reserved.
 *
 * This file is part of ShefLMStore
 * It is a modification of code from:
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

#ifndef simple_select11_h
#define simple_select11_h


#include <vector>
#include <stdint.h>
#include "macros.h"
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>

using std::vector;
class simple_select11 {
private:
	boost::shared_ptr< vector<uint64_t> > bits;
	vector<int64_t> inventory;
	vector<uint64_t> subinventory;
    vector<uint64_t> exact_spill;
	int log2_ones_per_inventory, log2_ones_per_sub16, log2_ones_per_sub64, log2_longwords_per_subinventory,
		ones_per_inventory, ones_per_sub16, ones_per_sub64, longwords_per_subinventory,
		ones_per_inventory_mask, ones_per_sub16_mask, ones_per_sub64_mask;

	uint64_t num_words, inventory_size, subinventory_size, exact_spill_size, num_ones;



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
	simple_select11(){}
	simple_select11(boost::shared_ptr< vector<uint64_t> > bits, const uint64_t num_bits,uint64_t num_double_ones);
	~simple_select11(){}
	uint64_t select11( const uint64_t rank );
	// Just for analysis purposes
	void print_counts();
	uint64_t bit_count();
private:
	simple_select11(const simple_select11&); //disallow copy
	void operator=(const simple_select11&); //disallow assignment
};

#endif

/*
 *  SArray.h
 *  ShefLM
 *
 *  Created by David Guthrie on 26/03/2010.
 *  Copyright 2009-2010 David Guthrie. All rights reserved.
 *
 * the sarray algorthim and parts of this code are written by
 * Daisuke Okanohara and Kunihiko Sadakane.
 * see:
 * Daisuke Okanohara and Kunihiko Sadakane. Practical entropy-compressed rank/select dictionary.	
 * In Proc. of the Workshop on Algorithm Engineering and Experiments, ALENEX 2007. SIAM, 2007.
 *
 * This file is part of ShefLMStore.
 * 
 * ShefLMStore is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ShefLMStore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with ShefLMStore.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SArray_h
#define SArray_h

#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/dynamic_bitset.hpp>
#include "CompactStore.h"
#include "ShefBitArray.h"

using std::vector;
using std::cout;
using std::cerr;





class SelectTable{
private:
	int select_tbl_array[8*256];
public:
	SelectTable(){
		for (int x = 0; x < 256; x++) {
			for (int r=0; r<8; r++) select_tbl_array[(r<<8)+x] = -1;
			for (int i=0; i<8; i++)
				if ((x>>(7-i))&1) select_tbl_array[(i<<8)+x] = i;
			
		}
	}
	int& operator[](const uint64_t &index){
		return select_tbl_array[index];
	}
};


template < typename T >
unsigned long totalbits( const vector<T>& vec ) {
	return 8*(sizeof( vec ) + sizeof( T ) * vec.size());
}


class DArray{
public:
	DArray(){}
	DArray(boost::shared_ptr<boost::dynamic_bitset<> > bit_array_to_index);
	uint64_t select(uint64_t) const;
	uint64_t bit_count() const;
	
private:
	boost::shared_ptr<boost::dynamic_bitset<> > bit_array;
	vector<uint64_t> s_long;
	vector<uint16_t> s_short;
	vector<int64_t> pvec;
	vector<uint64_t> lp_first_one_in_block;
	static const unsigned ones_per_block; //L
	static const unsigned log_ones_per_block; //logL
	static const unsigned max_block_length; //LL
	static const unsigned spacing;	//LLL
	static const unsigned log_spacing;	//LLL

private:
	friend class boost::serialization::access;
	
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & s_long;
		ar & s_short;
		ar & pvec;
		ar & lp_first_one_in_block;
		ar & bit_array;
	}
};

//const unsigned DArray::max_block_length=1<<16; //LL we use 16 so we can store offsets to each block as a uint16_t (normally know as unsigned short)
//const unsigned DArray::log_spacing=5;	//logLLL
//const unsigned DArray::spacing=1<<5;	//LLL
//const unsigned DArray::ones_per_block=1<<10; //L
//const unsigned DArray::log_ones_per_block=10; //logL


const unsigned DArray::max_block_length=1<<16; //LL 
const unsigned DArray::log_spacing=6;	//logLLL
const unsigned DArray::spacing=1<<6;	//LLL
const unsigned DArray::ones_per_block=1<<10; //L
const unsigned DArray::log_ones_per_block=10; //logL


DArray::DArray(boost::shared_ptr<boost::dynamic_bitset<> > bit_array_to_index)
:bit_array(bit_array_to_index){
	uint64_t n=bit_array->size();
	uint64_t num_ones = 0;
	for (uint64_t i=0; i<n; i++) num_ones += bit_array->test(i);
	
	fprintf(stderr,"Size of D array to compress=%llu \n number of ones=%llu \n Density of DArray=%.2f%% \n",n,num_ones, 100.0 * num_ones/n);

	
	
	std::vector<uint64_t> S_position_vec(num_ones); // holds the position of every one
	
	num_ones = 0;
	for (uint64_t i=0; i<n; i++) {
		if (bit_array->test(i)) {
			S_position_vec[num_ones++] = i;
		}
	}

	uint64_t total_blocks = (num_ones-1) / ones_per_block + 1;
	
	lp_first_one_in_block.resize(total_blocks+1);
	pvec.resize(total_blocks+1);
	
	
	for (unsigned r = 0; r < 2; r++) {
		uint64_t long_block_counter = 0;
		uint64_t short_block_counter = 0;
		
		for (uint64_t block_number = 0; block_number < total_blocks; block_number++) {
			uint64_t beginingOfBlock=block_number*ones_per_block;
			uint64_t pos_of_first_one_in_block = S_position_vec[beginingOfBlock]; //absolute position of the first one in the block
			lp_first_one_in_block[block_number] = pos_of_first_one_in_block;
			uint64_t i = std::min((block_number+1)*ones_per_block-1,num_ones-1);  
			uint64_t position_of_last_one_in_block = S_position_vec[i]; //aboslute position of last one in the block
			
			if (position_of_last_one_in_block - pos_of_first_one_in_block >= max_block_length) { //then it is a long block
				if (r == 1) {
					for (uint64_t bit_number = 0; bit_number < ones_per_block; bit_number++) {
						if (beginingOfBlock+bit_number >= num_ones) break;
						s_long[long_block_counter*ones_per_block+bit_number] = S_position_vec[beginingOfBlock+bit_number];  //s_long stores the absolute position of every one that occurs in a small block
					}
				}
				//in pvec store the 1+ the index in the long block array as a negative number (same as long_block_counter*ones_per_block + 1 )
				pvec[block_number] = -((long_block_counter<<log_ones_per_block)+1); 
				long_block_counter++;
			} else { //else it is a short block
				if (r == 1) {
					for (uint64_t bit_number = 0; bit_number < ones_per_block/spacing; bit_number++) { //store position of every spacingTH one with respect to first one in block
						if (beginingOfBlock+bit_number*spacing >= num_ones) break;
						s_short[short_block_counter*(ones_per_block/spacing)+bit_number] = S_position_vec[beginingOfBlock+bit_number*spacing] - pos_of_first_one_in_block;
					    //cout <<"s_short["<<short_block_counter*(ones_per_block/spacing)+bit_number<<"] = "<<S_position_vec[beginingOfBlock+bit_number*spacing] - pos_of_first_one_in_block <<endl;;
					}
				}
				pvec[block_number] = short_block_counter << (log_ones_per_block-log_spacing); //in pvec store the begining of the short blocks for these ones
			    //cout << "pvec["<<block_number<<"] = "<< (short_block_counter << (log_ones_per_block-log_spacing)) <<endl
				;
				short_block_counter++;
			}
		}
		if (r == 0) {
			s_long.resize(long_block_counter*ones_per_block+1);
			s_short.resize(short_block_counter*(ones_per_block/spacing)+1);
			cerr <<"Darray number of short blocks is:"<<short_block_counter<<" number of long (exact blocks) is:"<<long_block_counter<<endl;
		}
	}
}

uint64_t DArray::select(uint64_t index) const{

    //cout <<"Darray looking up index:" <<index <<endl;
	
	if (index==0) return -1;
	
	--index; //now we decrement the index because first one is stored at index zero

	
	uint64_t p =0;
	long il = pvec[index>>log_ones_per_block];
	if (il < 0) {  //this is a long block so just lookup the position of the one
		il = -il-1;
	    //cout << "It is a long block and il is:"<<il<<endl;
	    //cout <<"s_long["<<il+(index & (ones_per_block-1))<<"]"<<endl;
		p= s_long[il+(index & (ones_per_block-1))];
	} else { //else is was a short block so be need to find the one

		p = lp_first_one_in_block[index>>log_ones_per_block];
		
	    //cout <<"index is now:" <<index <<endl;

		
	    //cout << "\n\nIt is a short block.  Found a block begining at (il):"<<il<<" in s_short array.  First one in block is at position:"<<p <<endl;
	    //cout << "s_short["<<il+((index & (ones_per_block-1))>>log_spacing)<<"]="<< (uint64_t)s_short[il+((index & (max_block_length-1))>>log_spacing)] <<endl;
		
		p += s_short[il+((index & (ones_per_block-1))>>log_spacing)]; //same as (beginingshortblockindex+ index/spacing)
		
	    //cout << "Added short aray index of index/spacing " << il+((index & (ones_per_block-1))>>log_spacing)<< " new p is:"<<p <<endl;
		//if (p>0)--p;
		int ones_to_target=((index) & (spacing-1));//((index)%(spacing-1));//
		//if (ones_to_target<0 &&p>0)--p;
	    //cout <<"ones to target is:"<<ones_to_target <<endl;
		
		for (int i=0; i<ones_to_target; ++i) {
			p=bit_array->find_next(p);
		}
	}
    //cout <<"Darray total number of ones is:"<<bit_array->count() << "  bit array size"<<bit_array->size()<<endl;
    //cout <<"Darray is returning p of:"<<p <<endl;
	return p;
}

uint64_t DArray::bit_count() const{
	return bit_array->size()+totalbits(lp_first_one_in_block)+totalbits(s_long)+totalbits(s_short)+totalbits(pvec);
}


class SArray{
	typedef unsigned char byte;
public:
	SArray(){};
	SArray(const vector<byte> &bitvec);
	uint64_t select(const uint64_t &index) const;
	uint64_t bit_count() const;
private:
	boost::shared_ptr<CompactStore> low_ptr;
	boost::shared_ptr<DArray> darray;
	uint64_t number_of_low_bits;

private:
	friend class boost::serialization::access;
	
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & low_ptr;
		ar & darray;
		ar & number_of_low_bits;
	}
};

bool getbit(const vector<unsigned char> &bitvec, uint64_t pos)
{
	return (bitvec[pos >>3] & (1<<(7&pos))) !=0;
}



SArray::SArray(const vector<byte> &bitvec){

	const uint64_t n=8*bitvec.size();
	uint64_t num_ones = 0;
	for (uint64_t i=0; i<n; i++) num_ones += getbit(bitvec,i);
	
	if (num_ones == n){
		cerr << "Error: Trying to create a Sarray index of a bitset. It contains all ones!"<<endl;
		exit(1);
	}
	
//	uint64_t mm = num_ones;
//	number_of_low_bits = 0;
//	while (mm < n) {
//		mm <<= 1;
//		number_of_low_bits++;
//	}
//	
//	boost::shared_ptr<boost::dynamic_bitset<> > hi_ptr(new boost::dynamic_bitset<>(2*num_ones));
//	boost::shared_ptr<boost::dynamic_bitset<> > sba_low_ptr(new boost::dynamic_bitset<>(number_of_low_bits*num_ones));
//	low_ptr.reset( new CompactStore(sba_low_ptr,number_of_low_bits));

	
	number_of_low_bits = static_cast<uint64_t>(floor(log2(n/num_ones)));

	boost::shared_ptr<boost::dynamic_bitset<> > hi_ptr(new boost::dynamic_bitset<>(num_ones+(n>>number_of_low_bits)));
	boost::shared_ptr<boost::dynamic_bitset<> > sba_low_ptr(new boost::dynamic_bitset<>(number_of_low_bits*num_ones));
	low_ptr.reset( new CompactStore(sba_low_ptr,number_of_low_bits));
	
	
	
	fprintf(stderr,"\n\nSize of bit array to compress as SArray=%llu \n number of ones=%llu \n Density of SArray=%.2f%% \n numner of low bits to use=%llu\n",n,num_ones, 100.0 * num_ones/n,number_of_low_bits);


	uint64_t one_counter=0;
	for (uint64_t i=0; i<n; i++) {
		if (getbit(bitvec,i)) {
			hi_ptr->set((i>>number_of_low_bits)+one_counter,1);
			low_ptr->set(one_counter,i & ((1<<number_of_low_bits)-1));
			++one_counter;
		}
	}
	
	darray.reset(new DArray(hi_ptr));
	
	cerr << "Size of Low BitArray:"<<low_ptr->getBitArrayPointer()->size() <<"\nSize of Upper DArray:"<<darray->bit_count()<<endl;

}




uint64_t SArray::select(const uint64_t &idx) const{
    //cout <<"lowptr total number of ones is:"<<low_ptr->getBitArrayPointer()->count() <<endl;
	uint64_t index = idx+1;
	
	if (index == 0) return -1;
	uint64_t result = darray->select(index) - (index-1);
	result <<= number_of_low_bits;
    //cout << "sarray result after shift:"<<result<<endl;
    //cout <<"adding:"<<low_ptr->at(index-1)<<endl;
	result += low_ptr->at(index-1);
    //cout << "sarray is returning:"<<result<<endl;
	return result;	

}


uint64_t SArray::bit_count() const{
	return low_ptr->getBitArrayPointer()->size()+darray->bit_count()+8*sizeof(number_of_low_bits);
}


#endif




/*
 *  ShefBitArray.h
 *  ShefLMStore
 *
 *  Created by David Guthrie on 19/11/2009.
 *  Copyright 2009-2010 David Guthrie. All rights reserved.
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

#ifndef ShefBitArray_H
#define ShefBitArray_H

#include <limits>
#include <zlib.h>
#include <iostream>
#include <vector>
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/split_member.hpp>
#include "dynamic_bitset.hpp"

using std::cout;
using std::cerr;
using std::endl;

static const size_t bits_per_word = std::numeric_limits<size_t>::digits;
static size_t word_index(size_t pos) { return pos / bits_per_word; }
static size_t bit_index(size_t pos) { return static_cast<size_t>(pos % bits_per_word); }
static size_t bit_mask(size_t pos) { return size_t(1) << (bits_per_word-bit_index(pos)); }


class ShefBitArray {
private:
	std::vector<size_t> words;
public:
	ShefBitArray(){}
	explicit ShefBitArray(const size_t &number_of_bits){
		const size_t wordArraylength(number_of_bits/bits_per_word + (number_of_bits%bits_per_word?1:0));
		words.resize(wordArraylength,0);
	}
		
	ShefBitArray& setbit(const size_t &pos, const bool& val){		
		if (val)
			words[word_index(pos)] |= bit_mask(pos);
		else
			words[word_index(pos)] &= ~bit_mask(pos);

		return *this;
	}
	
	bool getbit(const size_t &pos) const{
		return (words[word_index(pos)] & bit_mask(pos)) != 0;
	}
	
	bool test(const size_t &pos) const{
		return getbit(pos);
	}
	ShefBitArray& set(const size_t &pos, const bool& val){
		setbit(pos,val);
		return *this;
	}
	
	ShefBitArray& set_range(size_t value, const size_t &start, const size_t &length){
		const size_t MASK=1 << (length-1);
		const size_t offset=start*length;
		for (unsigned i=0; i<length; i++) {
			setbit(offset+i,(value&MASK?true:false));
			value<<=1;
		}
		return *this;
	}
	
	size_t get_range(const size_t &start, const size_t &length) const{
		size_t retVal=0;
		for (unsigned i=0; i<length; i++) {
			retVal<<=1;
			retVal|=getbit(start*length+i);			
		}
		return retVal;
	}
	
	size_t size_in_bits() const{
		return words.size()*sizeof(size_t)*8;
	}
	size_t size_in_bytes() const{
		return words.size()*sizeof(size_t);
	}
	size_t size() const{
		return size_in_bits();
	}
private:	
	friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & words;
	}
};




















class CompressedBlock{
public:
	size_t compressed_size;
	const size_t uncompressed_size;
	void * compressed_data;
	void * uncompressed_data;
	bool dirty;
public:
	explicit CompressedBlock(size_t block_size)
		:uncompressed_size(block_size){
		compressed_size=0;
		compressed_data=NULL;
		uncompressed_data=NULL;
		dirty=false;
	}
	~CompressedBlock(){
		//it is safe to free null pointer so this is ok
		free(compressed_data);
		free(uncompressed_data);
	}
	void setDirty(){
		dirty=true;
	}
	void compress(){
		if (uncompressed_data==NULL && compressed_data==NULL){
			cerr << "Error there is no data to compress";
			exit(0);
		}
		if (compressed_data==NULL||dirty) {
			free(compressed_data);
			//cerr << "COMPRESSING bytes:"<<uncompressed_size<<endl;
			//this is the minimum zlib allows
			compressed_size=uncompressed_size+static_cast<size_t>(uncompressed_size*0.1+12.0);
			compressed_data = malloc(compressed_size);
			int r=compress2((Bytef*)compressed_data, (uLongf*)&compressed_size,
					  (const Bytef*)uncompressed_data, (uLongf)uncompressed_size, Z_BEST_COMPRESSION);//this could be Z_BEST_COMPRESSION or Z_BEST_SPEED
			if (r!=Z_OK){
				cerr << "error compressing!!! error:"<< r<<endl;
				exit(0);
			}
			compressed_data=realloc(compressed_data, compressed_size);
		}
		free(uncompressed_data);
		uncompressed_data=NULL;
	}
	
	void decompress(){
		if (uncompressed_data!=NULL){
			return;
		} 

		uncompressed_data=malloc(uncompressed_size);
	
		if (compressed_data!=NULL){
			//cerr << "DECOMPRESSING need to malloc bytes:" << uncompressed_size <<std::flush <<endl;
			int r=uncompress((Bytef*)uncompressed_data,(uLongf*) &uncompressed_size, (const Bytef*)compressed_data,(uLongf)compressed_size);
				if (r!=Z_OK){
					cerr << "error compressing!!! error:"<< r<<endl;
					exit(0);
				}
			dirty=false;
		}
		
		return;
	}
	
	size_t * getData(){
		if (uncompressed_data!=NULL)
			return static_cast<size_t*> (uncompressed_data);
		cerr <<"ERROR NO uncompressed data";
		return NULL;
	}
	
}; 


class ShefCompressedBitArray {
private:
	CompressedBlock** pointerArr;
	size_t words_per_block;
	int open_block;
	ShefCompressedBitArray(const ShefCompressedBitArray&); //disallow copy
	void operator=(const ShefCompressedBitArray&); //disallow assignment
public:
	explicit ShefCompressedBitArray(const size_t & size,const size_t & num_blocks=1024){
		const size_t total_words=size/bits_per_word + (size%bits_per_word?1:0);
		words_per_block= total_words/num_blocks +(total_words%num_blocks?1:0);
		
		cerr << "total size " << size <<endl;
		cerr << "total blocks " << num_blocks <<endl;
		cerr << "total_words " << total_words <<endl;
		cerr << "total words_per_block " << words_per_block <<endl;
		open_block=-1;
		pointerArr=new CompressedBlock*[num_blocks];
		for (size_t i=0; i<num_blocks; ++i) {
			pointerArr[i]=new CompressedBlock(words_per_block*bits_per_word/8);
		}
		
	}
	ShefCompressedBitArray(){
		delete[] pointerArr;
	}
	ShefCompressedBitArray& set(const size_t & pos, const bool &val)
	{	
		openCorrectBlock(pos);
		size_t * m_bits=pointerArr[open_block]->getData();
		//cerr << "SETTING pos:"<<pos<<" with value:"<<val<<endl;
		//cerr << "Accessing word:"<<word_index(pos)%words_per_block<<endl;
		//cerr << "USING BITMASK:"<<hex<<bit_mask(pos)<<dec<<endl;
		if (val)
			m_bits[word_index(pos)%words_per_block] |= bit_mask(pos);
		else
			m_bits[word_index(pos)%words_per_block] &= ~bit_mask(pos);
		
		pointerArr[open_block]->setDirty();
		return *this;
	}
	
	bool test(const size_t & pos)
	{
		openCorrectBlock(pos);
		size_t * m_bits=pointerArr[open_block]->getData();
		//cerr << "GETTING pos:"<<pos<<" with value:"<<((m_bits[word_index(pos)%words_per_block] & bit_mask(pos)) != 0)<<endl;
		//cerr << "Accessing word:"<<word_index(pos)%words_per_block<<endl;
		//cerr << "USING BITMASK:"<<hex<<bit_mask(pos)<<dec<<endl;
		return ((m_bits[word_index(pos)%words_per_block] & bit_mask(pos)) != 0);
	}
	
private:
	void openCorrectBlock(const size_t &pos){
		size_t block_index = pos/bits_per_word/words_per_block;
		if (block_index!=static_cast<size_t>(open_block) && (open_block != -1)){
			pointerArr[open_block]->compress();
		}
		open_block=block_index;
		//cerr <<"Accessing block:"<<block_index<<endl;
		pointerArr[open_block]->decompress();
	}
};

#endif




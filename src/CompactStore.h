/*
 *  CompactStore.h
 *  ShefLMStore
 *
 *  Created by David Guthrie on 16/11/2009.
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


#ifndef COMPACT_STORE_H
#define COMPACT_STORE_H

#include <iostream>
#include <iomanip>
#include <cmath>
#include <boost/shared_ptr.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "dynamic_bitset.hpp"

#include "ShefBitArray.h"

class CompactStore{
	typedef boost::dynamic_bitset<> bitarray;
	//typedef ShefBitArray bitarray;
private:
	boost::shared_ptr<bitarray> bitArrayPtr;	
	unsigned bits_per_element;
public:
	CompactStore(){}
	CompactStore(boost::shared_ptr<bitarray> bitArray, const int & element_size) 
	:bitArrayPtr(bitArray),bits_per_element(element_size){
		
	}
	
	boost::shared_ptr<bitarray> getBitArrayPointer(){
		return bitArrayPtr;
	}
	
	uint64_t operator[](const uint64_t &pos) const{//unchecked access
		uint64_t retVal=0;
		for (unsigned i=0; i<bits_per_element; i++) {
			retVal<<=1;
			retVal|=getbit(pos*bits_per_element+i);			
		}
		return retVal;
	}
	
	uint64_t at(const uint64_t &pos) const{ //This really shoul be checked access
		return operator[](pos);
	}
	
	int getBitsPerElement() const{
		return bits_per_element;
	}
	
	CompactStore& set(const uint64_t &pos,uint64_t value){
		const uint64_t MASK=1 << (bits_per_element-1);
		const uint64_t offset=pos*bits_per_element;
		for (unsigned i=0; i<bits_per_element; i++) {
			setbit(offset+i,(value&MASK?true:false));
			value<<=1;
		}
		return *this;
	}
	
	bool getbit(const uint64_t &pos) const{
		return bitArrayPtr->test(pos);
	}
	CompactStore& setbit(const uint64_t & pos, const bool &value){
		//std::cout << "Position to set is:"<<pos<<" value to set is:"<<value<<" size of bitarray is:"<<bitArrayPtr->size()<<" bits per element is:"<<bits_per_element<<std::endl;
		bitArrayPtr->set(pos,value);
		return *this;
	}
	
private:	
	friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & bits_per_element;
		ar & bitArrayPtr;
	}
	
};



#endif


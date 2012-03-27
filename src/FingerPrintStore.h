/*
 *  FingerPrintStore.h
 *  ShefLMStore
 *
 *  Created by David Guthrie on 18/11/2009.
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

#ifndef FINGERPRINT_STORE_H
#define FINGERPRINT_STORE_H

#include "CompactStore.h"
#include <boost/shared_ptr.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <boost/dynamic_bitset.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "ShefBitArray.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::hex;
using std::dec;
using std::setw;

using boost::shared_ptr;
using std::pow;



unsigned int MurmurHash2( const void * key, int len, unsigned int seed=0);




//FingerPrintStore Interface
class FingerPrintStore {
	//typedef boost::dynamic_bitset<> bitarray;
	typedef ShefBitArray bitarray;
public:
	FingerPrintStore(){}
	FingerPrintStore(const uint64_t & numberOfElements, const unsigned &bits_per_fingerprint);
	
	FingerPrintStore& storeFP(const uint64_t &index,const string &key);
	bool checkFP(const uint64_t &index,const string &key) const;
	
	
	
private:
	FingerPrintStore(const FingerPrintStore&); //disallow copying
	void operator=(const FingerPrintStore&); //disallow assignment
	unsigned int finger_print_size; //number of bits to hold fingerprint
	uint64_t totalNumberOfBits;  //number of bits in bit array
	uint64_t totalNumberOfElements;


	boost::shared_ptr<CompactStore> store;  //the bit array
	
	uint64_t fp(const string & key) const;
private:	
	friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & totalNumberOfElements;
		ar & totalNumberOfBits;
		ar & finger_print_size;
		ar & store;
	}

};








//Implementation

//Private Functions



inline uint64_t FingerPrintStore::fp(const string & key) const{
	uint64_t h= MurmurHash2(key.c_str(), key.length());
	h&=((1 << finger_print_size)-1) << 32-finger_print_size;
	int shift = finger_print_size-32;
	if (shift>0) h <<= shift;
	else h>>=abs(shift);
	//cerr <<"hashkey: " <<hex <<h <<dec <<endl;
	return h;
	
}


//Constructor
FingerPrintStore::FingerPrintStore(const uint64_t & numberOfElements, const unsigned &bits_per_fingerprint)
		:finger_print_size(bits_per_fingerprint),
		totalNumberOfBits((bits_per_fingerprint)*numberOfElements),
		totalNumberOfElements(numberOfElements)

{
	if (bits_per_fingerprint>32) {cerr<< "bits per fingerprint must be 32 bits or less because of the hash function used" <<endl; exit(1);}
	cerr << "Creating Hash Storage with " << bits_per_fingerprint <<" bits per fingerprint" <<endl;
	
	boost::shared_ptr<boost::dynamic_bitset<> > sbr_ptr(new boost::dynamic_bitset<>(totalNumberOfBits));
	store.reset(new CompactStore(sbr_ptr,finger_print_size));
}





//Methods


inline FingerPrintStore& FingerPrintStore::storeFP(const uint64_t &index,const string &key){
	uint64_t fpv=fp(key);
	//cerr << "*** Storing key:"<<key<<" with fp hex:"<< hex<<fpv << dec<<" at index:"<<index<<endl;
	store->set(index, fpv);
	return *this;
}

inline bool FingerPrintStore::checkFP(const uint64_t &index,const string &key) const{
	if (index >= totalNumberOfElements) return false;
	uint64_t retrievedfp=(*store)[index];
	//cerr << "*** murmmer hash of new key to lookup is:"<<hex<<fp(key)<<dec<<endl;
	//cerr << "*** fp in store at index:"<<index<<" is hex value:"<< hex<<retrievedfp << dec<<endl;
		
	if (retrievedfp == fp(key)){
		return true;
	}
	return false;
}






//Other Helper functions



//This is MurmurHash 2.0 from source file MurmurHash2.cpp by By Austin Appleby
//available at: http://murmurhash.googlepages.com/
unsigned int MurmurHash2( const void * key, int len, unsigned int seed){
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.
	
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	
	// Initialize the hash to a 'random' value
	
	unsigned int h = seed ^ len;
	
	// Mix 4 bytes at a time into the hash
	
	const unsigned char * data = (const unsigned char *)key;
	
	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;
		
		k *= m; 
		k ^= k >> r; 
		k *= m; 
		
		h *= m; 
		h ^= k;
		
		data += 4;
		len -= 4;
	}
	
	// Handle the last few bytes of the input array
	
	switch(len)
	{
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
	        h *= m;
	};
	
	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.
	
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	
	return h;
} 






#endif



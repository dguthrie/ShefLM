/*
 *  CompressedValueStore.h
 *  ShefLMStore
 *
 *  Created by David Guthrie on 13/03/2010.
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






//Google count data compresses to 5333529981 bytes (including coded array and elias fano index)
//(5333529981/1024/1024/1024)/(3775790711*20/8/1024/1024/1024) =56.50239% of the size used by 20N method
//23.37048  is total bytes neded to store all counts in the google data

#ifndef compressed_value_store_h
#define compressed_value_store_h

#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/shared_ptr.hpp>

//#include "simple_select.h"
//#include "simple_select_half.h"
//#include "rank9sel.h"
#include "SArray.h"

using std::cout;
using std::cerr;
using std::endl;
using std::hex;
using std::dec;


class CompressedValueStore {
	typedef unsigned char byte;
public:
	CompressedValueStore(){}
	template <class T>
	CompressedValueStore(const T &value_array,const uint64_t &num_elements_stored);
	uint64_t at(const uint64_t &index) const;
	uint64_t size_in_bits(){return (ss->bit_count())+8*(sizeof(code_vector) + sizeof(byte) * code_vector.size()) +8*sizeof(num_elements_stored)+8*sizeof(bits_in_code_vector)+8*sizeof(ss);}
private:
	uint64_t num_elements_stored;
	uint64_t bits_in_code_vector;
	std::vector<byte> code_vector;
	unsigned maskbit[32];
	void addToCodeVector(const unsigned &ich, const int &ich_len_in_bits ,std::vector<byte> & code_vector, uint64_t &code_vector_len_in_bits);

	//boost::shared_ptr<elias_fano> ss;
	boost::shared_ptr<SArray> ss;
private:
	friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & num_elements_stored;
		ar & code_vector;
		ar & bits_in_code_vector;
		ar & maskbit;
		ar & ss;
	}
};

template <class T>
CompressedValueStore::CompressedValueStore(const T &value_array,const uint64_t & num_elements)
:num_elements_stored(num_elements){
	
	uint64_t size_to_reserve=static_cast<uint64_t>(num_elements_stored*0.7518096802161448);  //this is the number of BYTES to reserve

	code_vector.reserve(size_to_reserve);	
	std::vector<byte> code_bit_index;  
	
	while(size_to_reserve%64) ++size_to_reserve; //make a multiple of 64 so we can pass it as an array of uuint64_t's later
	code_bit_index.reserve(size_to_reserve);
	
	for (int j=0;j<32;j++) maskbit[j] = 1 << j;
	
	uint64_t num_bits1=0;
	uint64_t num_bits2=0;
	for (uint64_t i=0; i< num_elements_stored; ++i) {
		uint64_t v = value_array[i];
		unsigned code_len=static_cast<unsigned>(floor(log2(v+2)));
		
		uint64_t code_bits=v+2- (1<<code_len);
		//cerr << "storing code:"<<v<< " with code length:" << code_bits <<endl;
		addToCodeVector(code_bits, code_len,code_vector,num_bits1);
		addToCodeVector(1,1,code_bit_index,num_bits2);
		if (code_len>1) addToCodeVector(0,code_len-1,code_bit_index,num_bits2);
	}
	cerr << "Number of elements stored " <<num_elements_stored <<endl;
	cerr << "Code vector is " << num_bits1 << " bits long.  Index vector is "<< num_bits2 <<" bits long" <<endl; //should be 777850484
	
	bits_in_code_vector=num_bits1;
	
	//std::vector<byte>(code_vector).swap(code_vector); //trim extra space using the "swap trick"
	//std::vector<byte>(code_bit_index).swap(code_bit_index); //trim extra space using the "swap trick"
	
	
	while(code_bit_index.size()%64) code_bit_index.push_back(0);  //make 64 bits long
	cerr << "Store code vector using array of: " << code_vector.size()<<" bytes" <<endl;
	cerr << "Initial size of code index array is: " << code_bit_index.size()<< " bytes" <<endl;
	
	//ss.reset(new elias_fano(reinterpret_cast<uint64_t*>(&code_bit_index[0]), num_bits2 ));	
	ss.reset(new SArray(code_bit_index));
	
	uint64_t compressed_index_bitcount=ss->bit_count();
	
	cerr << "Index vector compressed to= "<<compressed_index_bitcount <<" bits.  Which is " << compressed_index_bitcount *100.0 /num_bits2 <<"% of the size of the original index vector."<<endl;
	cerr << "\nTotal bits used for code and index= " << 8*code_vector.size()+compressed_index_bitcount <<endl;
}

uint64_t CompressedValueStore::at(const uint64_t &index) const{
	if (index>num_elements_stored) return -1;
	//cerr <<"\n\n\nCompressedValueStore Looking up "<<index<<endl;
	uint64_t index1=ss->select(index);
	uint64_t index2=0;
	if (index+1<num_elements_stored) index2=ss->select(index+1);
	else index2=bits_in_code_vector;
	uint64_t compressed_code=0;
	uint64_t length=index2-index1;
	//cerr << "index1 is: " <<index1 << " index2 is: "<< index2 <<endl;
	while (index1<index2){
		//cerr << "index1 is: " <<index1 << " index2 is: "<< index2 << " Result is :" << compressed_code <<endl;
		
		uint64_t block_num=index1>>3;
		//cerr << "Block num is " << block_num <<endl;
		//cerr << "7 & index1 is " << hex <<(7 & index1) << " maskbit[7 & index1]) is: "<< (maskbit[7 & index1]) <<endl;
		//cerr << "code_vector[block_num] is "<<(int)code_vector[block_num] <<dec <<endl;
		
		compressed_code<<=1;
		if((code_vector[block_num] & maskbit[7 & index1++]) != 0){
			compressed_code|=1;
		}
	}
	//cerr <<"Compressed code is: "<< compressed_code<<" and length is "<<length<<endl;
	return compressed_code + ( 1 << length ) -2;
}

void CompressedValueStore::addToCodeVector(const unsigned &ich, const int &ich_len_in_bits ,std::vector<byte> & code_vector, uint64_t &code_vector_len_in_bits){
	int m,n;
	uint64_t nc;
	if (code_vector.size()==0) code_vector.push_back(0);
	n=ich_len_in_bits;
	for (n=n-1 ; n >= 0 ; n-- , ++code_vector_len_in_bits) {
		nc=code_vector_len_in_bits >> 3;
		m=code_vector_len_in_bits & 7;
		while (nc>code_vector.size()-1) code_vector.push_back(0);
		if (m == 0) code_vector[nc]=0;
		if ((ich & maskbit[n]) != 0) code_vector[nc] |= maskbit[m];
	}
}



#endif


/*
 *  CompressedValueStoreFibonacci.h
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



#ifndef compressed_value_store_fibonacci_h
#define compressed_value_store_fibonacci_h

#include <vector>
#include <algorithm>
#include <utility>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <boost/dynamic_bitset.hpp>
#include <boost/unordered_map.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/shared_ptr.hpp>
#include "simple_select11.h"

using std::cout;
using std::cerr;
using std::endl;
using std::hex;
using std::dec;


class CompressedValueStoreFibonacci{
private:
    std::vector<unsigned> fibonacci_vec;
    uint64_t num_elements_stored;
	uint64_t bits_in_code_vector;
	uint64_t maskbit[64];
    void addToCodeVector(const uint64_t &ich, const int &ich_len_in_bits ,std::vector<uint64_t> & code_vector, uint64_t &code_vector_len_in_bits);
    boost::shared_ptr<simple_select11> ss;
    boost::shared_ptr<std::vector<uint64_t> > code_vector_ptr;
public:
    template <class T>
    CompressedValueStoreFibonacci(const T &value_array,const uint64_t & num_elements,const uint64_t max_value=900000);
    uint64_t at(const uint64_t &index) const;
	uint64_t size_in_bits(){return (ss->bit_count())+8*(sizeof(code_vector_ptr) + sizeof(uint64_t) * code_vector_ptr->size()) +8*sizeof(num_elements_stored)+8*sizeof(bits_in_code_vector)+8*sizeof(ss);}

};

template <class T>
CompressedValueStoreFibonacci::CompressedValueStoreFibonacci(const T &value_array,const uint64_t & num_elements,const uint64_t max_value):num_elements_stored(num_elements){
    for (int j=0;j<64;j++) maskbit[j] = 1ULL << j;
    boost::unordered_map<unsigned,std::pair<unsigned,unsigned> > code_cache; //cache maps numbers to a pair of fibcode and length
    fibonacci_vec.reserve(28);
    //populate an array with all fibonacci numbers up to the maxvalue
    int a = 1, b = 1;
    while(b<=max_value){
        fibonacci_vec.push_back(b);
        b+=a;
        a=b-a;
    }
    code_vector_ptr.reset(new std::vector<uint64_t>());


    /* My original code in python
import bisect
def genFib(x):
    if x<=1: return 1;
    else: return fib(x-1) + fib(x-2)

fib_arr=[genFib(i) for i in range(1,20)]
    
def fibcode(v):
    if v==0: return 0;
    pos=bisect.bisect(fib_arr,v)-1
    code_len=pos+2
    code_value=1
    val=v
    for i in range(pos,-1,-1):
    if (fib_arr[i]<=val):
        val-=fib_arr[i];
        code_value|=1<<(pos-i+1);
    print "code:",code_value,"code len:",code_len
    print "binary code:",bin(code_value)
    return code_len

a=[fibcode(i) for i in range(1,256)]
     */
    uint64_t num_bits=0;

    
    for (uint64_t i=0; i< num_elements_stored; ++i) {
        //Find the largest Fibonacci number equal to or less than v, and create
        //bitarray of that length to store Zeckendorf's representation of v
        const unsigned v = value_array[i]+1; //ADD ONE TO ALL VALUES SO WE CAN STORE ZEROS!  MUST SUBTRACT ONE IN GETTER METHOD!
        //if not yet in cache compute the code
        if (code_cache.find(v)==code_cache.end()){
            std::vector<unsigned>::iterator minval_iter = std::upper_bound( fibonacci_vec.begin(), fibonacci_vec.end(), v );
            if (minval_iter==fibonacci_vec.end()){cerr << "Error creating fibcode, Max value must be too small!!" <<endl;exit(0);}
            if (minval_iter!=fibonacci_vec.begin()) --minval_iter;
            unsigned pos=std::distance(fibonacci_vec.begin(),minval_iter);
            //cerr << "Coding value:"<<v<<" Found max pos:"<<pos<<" which has fib num:"<<fibonacci_vec[pos]<<endl;
            unsigned code_len=pos+2;
            unsigned code_value=1;
            //set the ith bit of number to one if the ith fibonacci num occurs in zeckendorf's representation
            unsigned val=v;
            for (long j=pos;j>=0;--j){
                if (fibonacci_vec[j]<=val){
                    val-=fibonacci_vec[j];
                    code_value|=1<<(pos-j+1);
                }
            }
            code_cache[v]=std::make_pair(code_value,code_len);
        }
        std::pair<unsigned,unsigned> code_pair=code_cache.at(v);
        unsigned code=code_pair.first;
        unsigned code_len=code_pair.second;
        //cerr << "Fib code for "<<v<<" is "<<code << " with length " <<code_len<<endl;
        addToCodeVector(code,code_len,*code_vector_ptr,num_bits);
    }
    cerr << "Fibonacci code vector is " << num_bits << " bits long." <<endl;
	//cerr << "Store code vector using array of: " << code_vector_ptr->size()<<" bytes" <<endl;
    
    bits_in_code_vector=num_bits;
	ss.reset(new simple_select11(code_vector_ptr, num_bits,num_elements_stored));
    cerr << "Fibonacci simple_select11 uses:"<<ss->bit_count()<<" bits."<<endl;
    
}


uint64_t CompressedValueStoreFibonacci::at(const uint64_t &index) const{
    if (index>num_elements_stored) return -1;
	//cerr <<"\n\n\nCompressedValueStore Looking up index:"<<index<<endl;
	uint64_t index1=ss->select11(index);
	uint64_t index2=0;
	if (index+1<num_elements_stored) index2=ss->select11(index+1);
	else index2=bits_in_code_vector;
	unsigned compressed_code=0;
	uint64_t value=0;
    short length=index2-index1;
    short pos=-1;
    
    
	//cerr << "index1 is: " <<index1 << " index2 is: "<< index2 <<endl;
	while (index1<index2){
        //cerr << "index1 is: " <<index1 << " index2 is: "<< index2 << " Result is :" << compressed_code <<endl;
		++pos;
		uint64_t block_num=index1>>6;
		//cerr << "Block num is " << block_num <<endl;
		//cerr << "7 & index1 is " << hex <<(7 & index1) << " maskbit[7 & index1]) is: "<< (maskbit[7 & index1]) <<endl;
		//cerr << "code_vector[block_num] is "<<(int)code_vector[block_num] <<dec <<endl;
		
		compressed_code<<=1;
		if(((*code_vector_ptr)[block_num] & maskbit[63 & index1++]) != 0){
			compressed_code|=1;
            //cout<<"1";
            if(length-pos>1) value+=fibonacci_vec[pos];
		}else{
            //cout<<"0";
        }
	}
    value-=1; //MUST subtract one from answer so get back the original value because we added one before storeing (so we could store zeros)
              //cerr <<"Compressed code is: "<< compressed_code<<" and length is "<<length<<" and value is "<<value<<endl;
	return value;
}



void CompressedValueStoreFibonacci::addToCodeVector(const uint64_t &ich, const int &ich_len_in_bits ,std::vector<uint64_t> & code_vector, uint64_t &code_vector_len_in_bits){
    int n=ich_len_in_bits;
	uint64_t block,m;
	if (code_vector.size()==0) code_vector.push_back(0);
	for (n=n-1 ; n >= 0 ; n-- , ++code_vector_len_in_bits) {
		block=code_vector_len_in_bits >> 6;
		m=code_vector_len_in_bits & 63;
		while (block>code_vector.size()-1) code_vector.push_back(0);
		if (m == 0) code_vector[block]=0;
 		if ((ich & maskbit[n]) != 0) code_vector[block] |= maskbit[m];
	}
    //cout <<"Code vector " <<code_vector[0]<< " length in bits: "<<code_vector_len_in_bits<<endl;
    // for(int i=0;i<code_vector_len_in_bits;++i) cout << ((maskbit[i] & code_vector[0])!=0);
    //cout <<endl;

}



#endif

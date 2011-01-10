/*!
 * \file      dynamic_bitset.hpp
 * \brief     Provides Boost.Serialization support for boost::dynamic_bitset
 * \author    Paul Fee
 * \date      08.01.2010
 * \copyright 2010 Paul Fee
 * \license   Boost Software License 1.0
 */

#ifndef BOOST_SERIALIZATION_DYNAMIC_BITSET_HPP
#define BOOST_SERIALIZATION_DYNAMIC_BITSET_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <cstddef> // size_t

#include <boost/config.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/detail/get_data.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/serialization/collections_save_imp.hpp>
#include <boost/serialization/collections_load_imp.hpp>


#include <iostream>
using std::cerr;
using std::endl;


namespace boost{
namespace serialization{

template <class Archive, typename Block, typename Allocator>
inline void save(
    Archive & ar,
    boost::dynamic_bitset<Block, Allocator> const & t,
    const unsigned int version 
){
    // Serialize bitset size
    std::size_t num_bits = t.size();
    ar & num_bits;
	std::size_t num_blocks_in_vec = t.num_blocks();
    ar & num_blocks_in_vec;
	
	
//	std::vector<Block> vec(t.num_blocks());
//    // Convert bitset into a vector
//	boost::to_block_range(t, vec.begin());
//	// Serialize vector
//    ar <<vec;
//	
//	cerr << "vec.size() is"<< vec.size()<< " saving vector of size" <<t.num_blocks()<<" for the bitset of:"<< size<<" bits, that starts with the values:"<<vec[0]<<" , " <<vec[1]<<endl;
	
	
	Block * bit_array = new Block[num_blocks_in_vec];
	boost::to_block_range(t, bit_array);
	boost::serialization::array<Block> ao = 
	boost::serialization::make_array<Block>(bit_array, t.num_blocks()); 
	ar << ao; 


	
	//cerr << "saving vector of size" <<t.num_blocks()<<" for the bitset of:"<< size<<" bits, that starts with the values:"<<vec[0]<<" , " <<vec[1]<<endl;

	

}

template <class Archive, typename Block, typename Allocator>
inline void load(
    Archive & ar,
    boost::dynamic_bitset<Block, Allocator> & t,
    const unsigned int  version  
){
    std::size_t num_bits = t.size();
    ar & num_bits;
	std::size_t num_blocks_in_vec = t.num_blocks();
    ar & num_blocks_in_vec;
	
    t.resize(num_bits);

//    // Load vector
//    std::vector<Block> vec;
//	//http://lists.boost.org/Archives/boost/2009/12/160375.php
//	//try this ar.load_binary(pData,dwSize).
//	
//    ar >>vec;
//
//	cerr << "loading vector of size: " <<vec.size() <<"and bitset should have"<<size <<"bits \n" ;//<<" that starts with the values: "<<vec[0]<<" , " <<vec[1]<<endl;

	Block* pData=new Block[num_blocks_in_vec];
	boost::serialization::array<Block> ao = 
	boost::serialization::make_array<Block>(pData, num_blocks_in_vec); 
	ar & ao;
    from_block_range(pData, pData+num_blocks_in_vec, t);
}

template <class Archive, typename Block, typename Allocator>
inline void serialize(
    Archive & ar,
    boost::dynamic_bitset<Block, Allocator> & t,
    const unsigned int version
){
    boost::serialization::split_free( ar, t, version );
}

} //serialization
} //boost


#endif // BOOST_SERIALIZATION_BITSET_HPP



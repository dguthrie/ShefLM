/*
 *  FingerPrintValueStore.h
 *  ShefLMStore
 *
 *  Created by David Guthrie on 15/03/2010.
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


#include <iostream>
#include <vector>
#include <string>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/list.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/shared_ptr.hpp>

#include "CompressedValueStoreElias.h"
#include "FingerPrintStore.h"

using std::cerr;

class FingerPrintValueStore{
public:
	FingerPrintValueStore(){}
	FingerPrintValueStore(boost::shared_ptr<FingerPrintStore> fingerprints, boost::shared_ptr<CompressedValueStoreElias> ranks, boost::shared_ptr<std::vector<uint64_t> > values)
		:	fp_store(fingerprints),
			cv_store(ranks),
			val_store(values){}
	uint64_t query(const uint64_t & index, const std::string & key) const;
	
private:
	boost::shared_ptr<FingerPrintStore> fp_store;
	boost::shared_ptr<CompressedValueStoreElias> cv_store;
	boost::shared_ptr<std::vector<uint64_t> > val_store;
	
private:	
	friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
		ar & fp_store;
		ar & cv_store;
		ar & val_store;
	}
	
};


inline uint64_t FingerPrintValueStore::query(const uint64_t & index, const std::string & key) const{
	bool found=fp_store->checkFP(index,key);
	//cerr << "Looking up index: "<<index<<" and key:"<<key<<endl;
	if (found){
		//cerr << "FP MATCHES" <<endl;
		uint64_t rank=cv_store->at(index);
		//cerr << "Rank is:"<<rank <<endl;

		if (rank < val_store->size() ){
			//cerr << "Getting Value" <<endl;
			return (*val_store)[rank];

		}
	}
	return 0;
}



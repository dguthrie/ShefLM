





#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <vector>
#include <cmath>
#include <ctime>
#include <boost/archive/tmpdir.hpp>
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
#include <getopt.h>

#include "CompactStore.h"
#include "ShefBitArray.h"
//#include "CompressedValueStore.h"
#include "CompressedValueStoreElias.h"
#include "CompressedValueStoreFibonacci.h"


//#include "MPHR.h"
//#include "KneserNeyWrapper.h"

using std::cout;
using std::cerr;
using std::endl;


void null_deleter(void const*){}

int main(int argc, char **argv){ 
	
	
	typedef boost::dynamic_bitset<> bitarray;
	//typedef ShefBitArray bitarray;

	
	//test rank array compression
	if (argc!=2){
		cout << "Usage: "<<argv[0] << " valuefile" <<endl;
		return 0;
	}		
	
	//change all these these for 8 or 20 bit array
	const bool testTiered=false;
	const int initialBitsForEachRank=20; //For google data this should be 20 for non tiered and 8 for tiered
	
    //typedef CompressedValueStoreElias cvType;
    typedef CompressedValueStoreFibonacci cvType;
	
	//-everything else is the same
	const uint64_t total_values=10;//65536;//1284371;//12000000;//3775790712UL;//955;//64403711;//
	const unsigned unique_values=771058;
    //	const uint64_t query_value=total_values-5;
	//std::vector<unsigned> text_vector;
	//text_vector.reserve(total_values);
	
	std::vector<uint64_t> uniq_values_vector;
	uniq_values_vector.reserve(unique_values);
	
	boost::shared_ptr<bitarray> sbr_ptr(new bitarray (initialBitsForEachRank*total_values));
	CompactStore text_compact_store(sbr_ptr,initialBitsForEachRank);
	{
		char * valueFileName=0;
		if (argc==2) valueFileName=argv[1];
	
		boost::shared_ptr<std::istream> valuefileFIN;
		boost::iostreams::filtering_stream<boost::iostreams::input> qin;

		//either read from named input if arg given or else use stdin
		if (valueFileName) {
			valuefileFIN.reset(new std::ifstream(valueFileName,std::ios_base::in|std::ios_base::binary));  //autoclosed by destructor
			if (strcmp((valueFileName+strlen(valueFileName)-3),".gz")==0) qin.push(boost::iostreams::gzip_decompressor());
		}
		else valuefileFIN.reset(&std::cin,null_deleter);
	

		qin.push(*valuefileFIN);
		
		uint64_t count=0;
		uint64_t value=0;
		unsigned rank=0;
		while (qin>>value) {
			if(uniq_values_vector.size()==0 || uniq_values_vector.back() != value){
				rank=uniq_values_vector.size(); //get current size of values array
				if (testTiered){
					rank+=2;
					if (rank>=512) rank=0;
					if (rank>=256) rank=1;
				}
				uniq_values_vector.push_back(value);
			}
			text_compact_store.set(count++,799000+rank);
		}
		qin.pop();
		cout << "Loaded " << count <<" values into array of size "<< total_values<<endl;

		
		//std::vector<unsigned>(text_vector).swap(text_vector); //trim extra space using the "swap trick"
		//std::vector<uint64_t>(uniq_values_vector).swap(uniq_values_vector); //trim extra space using the "swap trick"
		
		//for (int i=count; i<total_values; ++i) {
		//	text_compact_store.set(i,30);
		//}
		//text_compact_store.set(query_value,2);
	}	
		
	{	
		cvType cv(text_compact_store,total_values);
		
		
		cout <<"\n** TOTAL SAVINGS over storing all ranks in "<< initialBitsForEachRank<<" bits each is:"<<(int64_t) (total_values*initialBitsForEachRank - cv.size_in_bits()) <<" bits.\n** New storage is:"<<100.0*cv.size_in_bits()/(total_values*initialBitsForEachRank)<<"% of the original size"<<endl;
        
        //std::clock_t start = std::clock();
		uint64_t result=0;
		
        for (int i=0; i<total_values; i++) {
			result=cv.at(i);	
			cerr << "*********SELHALF result is: " <<result <<" and that give an original value of: " << uniq_values_vector[result-799000] <<endl;
		}
        
        
        
        
        
        
    }//comment this out if including the end
		/*
		std::cout<<"The Time used for searching is:" <<( ( std::clock() - start ) / (double)CLOCKS_PER_SEC )<<" seconds" <<endl;
		cout <<"Total Result is:"<<result<<endl;
	
	
		uint64_t result2=cv.at(query_value);	
		cout << "SELHALF result is: " <<result2 <<" and that give an original value of: " << uniq_values_vector[result2] <<endl; 
		cout.flush();
		
		
		start = std::clock();
	
	    std::ofstream ofs("Archivefilename");
		// save data to archive
		{
			//boost::archive::text_oarchive oa(ofs);
			boost::archive::binary_oarchive oa(ofs);
			// write class instance to archive
			oa << cv;
			// archive and stream closed when destructors are called
		}
	}
		
		// ... some time later restore the class instance to its orginal state
	cvType newcv;
		{
			// create and open an archive for input
			std::ifstream ifs("Archivefilename");
			//boost::archive::text_iarchive ia(ifs);
			boost::archive::binary_iarchive ia(ifs);
			// read class state from archive
			ia >> newcv;
			// archive and stream closed when destructors are called
		}
	
	uint64_t result2=newcv.at(query_value);	
	cout << "SELHALF result is: " <<result2 <<" and that give an original value of: " << uniq_values_vector[result2] <<endl; 
	cout.flush();

	std::cout<<"The Time used for saving and loading is:" <<( ( std::clock() - start ) / (double)CLOCKS_PER_SEC )<<" seconds" <<endl;
	*/
	
    return 0;
	
}

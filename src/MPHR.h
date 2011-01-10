/*
 *  MPHR.h
 *  ShefLMStore
 *
 *  Created by David Guthrie on 08/03/2010.
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


#ifndef MPHR_H
#define MPHR_H


#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <vector>
#include <iterator>
#include <cmath>
#include <boost/dynamic_bitset.hpp>
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

#include "CompressedValueStoreElias.h"
#include "FingerPrintValueStore.h"
#include "CompactStore.h"
#include "ShefBitArray.h"
#include "FingerPrintStore.h"
#include "cmph.h"
#include "cmph_structs.h"

using std::string;
using std::ifstream;
using std::ofstream;

//Forward declaration of ngram reading function we use for hashing values
static int ngram_file_read(gzFile, char**, cmph_uint32*);


#define HASH_FILENAME_SUFIX ".hash"
#define FP_VALUE_FILENAME_SUFIX ".fp_values"



class MPHR{
	typedef boost::dynamic_bitset<> bitarray;
	// typedef ShefBitArray bitarray;
public:
	MPHR(const char * pathToNgramFileName, const unsigned &bits_per_fingerprint, const unsigned &bits_per_rank, const char * basefilename);
	~MPHR();
	explicit MPHR(const string & loadMPHRFromBaseFileName);
	void writeMPHRToFilesWithBaseName(const string &storeBaseFileName) const;
	uint64_t query(const string & key) const;
	

private:
	void initWithFiles(const string & hashFileName, const string & fpRankValueFileName);
	void readHashFromFile(const string & hashFileName);
	void writeHashToFile(const string & hashFileName) const;
	void readFPArrayFromFile(const string & fpArrayFileName);
	void writeFpArrayToFile(const string & fpArrayFileName) const;
	
private:
	cmph_t * minimal_hash;
	boost::shared_ptr<FingerPrintValueStore> fp_value_store;
};

MPHR::MPHR(const string & loadMPHRFromBaseFileName){
	string fn=loadMPHRFromBaseFileName;
	initWithFiles(fn+HASH_FILENAME_SUFIX, fn+FP_VALUE_FILENAME_SUFIX);
}



//This function makes 4 passes through the ngram file
//This is slow, but it is done to keep the data stored in memory at time to a minimum
//1. count lines in the file
//2. hash every line in the file
//3. store ranks and values and then compress them for every line
//4. store the fingerprints for every line
MPHR::MPHR(const char * pathToNgramFileName, const unsigned &bits_per_fingerprint, const unsigned &bits_per_rank, const char * basefilename){

	
	//check if the hash file or fp_store files exists and if so load them instead of replaceing them
	bool buildNewHash=true;
	bool buildNewFpRankStore=true;
	string hash_file_name;
	string fp_store_file_name;
	if(basefilename != NULL){
		string basefn=basefilename;
		hash_file_name=basefn+HASH_FILENAME_SUFIX;
		fp_store_file_name=basefn+FP_VALUE_FILENAME_SUFIX;
		ifstream hfile(hash_file_name.c_str(),std::ios_base::in);
		if (hfile) buildNewHash=false;
		hfile.close();
		ifstream fpfile(fp_store_file_name.c_str(),std::ios_base::in);
		if (fpfile) buildNewFpRankStore=false;
		fpfile.close();
	}
	
	if (buildNewHash){
		cmph_config_t *config;
		//gzopen works on gziped or normal files
		gzFile keys_fd =gzopen(pathToNgramFileName, "r");
		if (keys_fd == NULL){
			fprintf(stderr, "key file not found:%s\n",pathToNgramFileName);
			exit(0);
		}
		cmph_io_adapter_t *source = cmph_io_nlfile_adapter(keys_fd);
		//use my ngram file adapter instead of the default one.  the ngram file reader only reads up to tab char on 
		// every line so that we are hashing ngrams and not also their counts.
		source->read=ngram_file_read;
		
		//Create minimal perfect hash function using the chd algorithm.
		const cmph_uint32 b=5;  //bucket lambda, how many keys per bucket, this is b option for the cmph command line program.  larger values lead to exponantionaly longer running times.
		const double m=1.0;  //this is the load factor (i.e. 1/m where m is the size of the array to store hash in) either use 1 or .99//this is c in the command line cmph
		
		config = cmph_config_new(source);
		cmph_config_set_algo(config, CMPH_CHD);
		cmph_config_set_b(config, b);
		cmph_config_set_verbosity(config, true);
		if (m != 0) cmph_config_set_graphsize(config, m);
		//create the hash
		minimal_hash = cmph_new(config);
		gzclose(keys_fd);
		cmph_io_nlfile_adapter_destroy(source);   
		cmph_config_destroy(config);
		cerr << "Created a minimal perfect hash for " <<minimal_hash->size<<" keys"<<endl;
	}else {
		cerr << "\n*******\nFound existing hash file at: "<<hash_file_name<<"\n So we will just load that file.  If you do not want to use this hash file either remove it or choose a new name.\n*******\n"<<endl;
		readHashFromFile(hash_file_name);
	}
    
	uint64_t total_number_of_keys_hashed=minimal_hash->size;
	
	if (buildNewFpRankStore){
		cerr << "Reading and Storing the rank of every ngram in the file using "<<bits_per_rank<<" bits per rank."<<endl;
		boost::shared_ptr<std::vector<uint64_t> > value_array(new std::vector<uint64_t>());
		value_array->reserve(771058);//this size is the number of unique values in Google Mixed Ngrams

        
		boost::shared_ptr<CompressedValueStoreElias> cvstore_ptr;
		
		//Go through the key file storing at values at hash position
		ifstream keyFIN(pathToNgramFileName,std::ios_base::in|std::ios_base::binary);
		if (!keyFIN) {
			cerr << "Unable to open key value file: "<<pathToNgramFileName <<endl;
			exit(1);
		}

		boost::iostreams::filtering_stream<boost::iostreams::input> in;
		if (strcmp((pathToNgramFileName+strlen(pathToNgramFileName)-3),".gz")==0) {
			in.push(boost::iostreams::gzip_decompressor());
		}
		{
			in.push(keyFIN);

			//create a compact store to hold the values
			boost::shared_ptr<bitarray > sbr_ptr(new bitarray(bits_per_rank*total_number_of_keys_hashed));
			CompactStore ranks_compact_store(sbr_ptr,bits_per_rank);
			
			//store all the values in bitarray
			string text;
			string key;
			string valuestr;	
			uint64_t value=0;
			uint64_t rank=0;
			
			while( std::getline(in,text) ) {
				string::size_type loc;
				loc=text.find('\t');
				if( loc != string::npos ) {
					key=text.substr(0, loc);
					valuestr=(text.substr(loc+1));
					std::stringstream(valuestr)>>value;
					if (value==0){
						cerr << "Error storing n-gram.  The line does not appear to be in the correct format. The line was:\n\n"<< text <<endl;
						continue;
					}
					if(value_array->size()==0 || value_array->back() != value) {  //if value is not equal then add to value array
						rank=value_array->size(); //get current size of values array
						value_array->push_back(value);
					}
					uint64_t index = cmph_search(minimal_hash, key.c_str(), (cmph_uint32)loc);
					ranks_compact_store.set(index,rank);
				}
			}
			in.pop();
			cerr << "Ranks and values have now been stored.  Compressing them now..."<<endl;
			//Compress the values
			cvstore_ptr.reset(new CompressedValueStoreElias(ranks_compact_store,total_number_of_keys_hashed));
			cerr << "...Done Compressing Values store"<<endl;
		}
		cerr << "Reading and Storing all Fingerprints from ngrams file"<<endl;

		//Last pass to store all the fingerprints
		keyFIN.seekg(0);
		in.push(keyFIN);
		//create a finger print store
		boost::shared_ptr<FingerPrintStore> fp_store(new FingerPrintStore(total_number_of_keys_hashed,bits_per_fingerprint));
		uint64_t value=0;
		string key;
		string text;
		string valuestr;
		while( std::getline(in,text) ) {
			string::size_type loc;
			loc=text.find('\t');
			if( loc != string::npos ) {
				key=text.substr(0, loc);
				valuestr=(text.substr(loc+1));
				std::stringstream(valuestr)>>value;
				if (value==0) continue;
				uint64_t index = cmph_search(minimal_hash, key.c_str(), (cmph_uint32)loc);
				fp_store->storeFP(index, key);
			}
		}
		in.pop();
		keyFIN.close();
			
		fp_value_store.reset(new FingerPrintValueStore(fp_store,cvstore_ptr,value_array));
		cerr << "All Fingerprints have been stored."<<endl;
	}else {
		cerr << "\n*******\nFound existing fingerprint rank store file at: "<<fp_store_file_name<<"\n So we will just load that file.  If you do not want to use this fpstore file then either remove it or choose a new name.\n*******\n"<<endl;
		readFPArrayFromFile(fp_store_file_name);
	}
	cerr << "The MPHR structure is complete"<<endl;

}

void MPHR::initWithFiles(const string & hashFileName, const string & fpRankValueFileName){
	
	cerr << "Loading MPHR From Disk"<<endl;

	//1. LOAD THE HASH
	readHashFromFile(hashFileName);
	
	
	//2. LOAD THE FINGERPRINTS-RANK-VALUES STRUCTURE
	readFPArrayFromFile(fpRankValueFileName);
	
	cerr << "MPHR Sucessfully Loaded From Disk"<<endl;
	
}

MPHR::~MPHR(){
	cmph_destroy(minimal_hash);
}

inline uint64_t MPHR::query(const string & key) const{
	uint64_t index = cmph_search(minimal_hash, key.c_str(), (cmph_uint32)key.length());
	uint64_t result=fp_value_store->query(index,key);
	return result;
}

void MPHR::writeMPHRToFilesWithBaseName(const string & storeBaseFileName) const{
	string fn=storeBaseFileName;
	cerr << "Writing MPHR to Disk...."<<endl;
	writeHashToFile(fn+HASH_FILENAME_SUFIX);
	writeFpArrayToFile(fn+FP_VALUE_FILENAME_SUFIX);
	cerr << "The MPHR structure has successfully been written to disk.  It is stored as two files that begin with the basefilename "<<storeBaseFileName<<" and end with the suffixes "<<HASH_FILENAME_SUFIX<<" and "<<FP_VALUE_FILENAME_SUFIX<<endl;
}

void MPHR::readHashFromFile(const string & hashFileName){
	FILE *mphf_fd = fopen(hashFileName.c_str(), "r");
	if (mphf_fd==NULL){
		cerr << "Error: can't read hash function file: " << hashFileName <<endl;
		exit(1);
	}
	minimal_hash = cmph_load(mphf_fd);
	fclose(mphf_fd);
}

void MPHR::writeHashToFile(const string & hashFileName) const{
	//file to write keys
	FILE* mphf_fd = fopen(hashFileName.c_str(), "w");
	if (mphf_fd==NULL){
		cerr << "Error: can't write to hash function file: " << hashFileName <<endl;
		exit(0);
	}
	cmph_dump(minimal_hash, mphf_fd);
	fclose(mphf_fd);
}


void MPHR::readFPArrayFromFile(const string & fpRankValueFileName){
	boost::shared_ptr<FingerPrintValueStore> fpvs_ptr;
	ifstream fpRankValueFileStream(fpRankValueFileName.c_str(),std::ios_base::in|std::ios_base::binary);
	if (!fpRankValueFileStream) {
		cerr << "Unable to open rank value file: "<<fpRankValueFileName <<endl;
		exit(1);
	}
	boost::iostreams::filtering_stream<boost::iostreams::input> in;
	try {  //try to read in the gzipped file (gzip is the format we output)
		in.push(boost::iostreams::gzip_decompressor());
		in.push(fpRankValueFileStream);
		boost::archive::binary_iarchive ia(in);
		ia >> fpvs_ptr;
		//in.pop();
	}
	catch (boost::iostreams::gzip_error &e) { 	//The file is not gzipped so just read it in normally (could be a file generated by the user)
		fpRankValueFileStream.seekg(0);
		boost::archive::binary_iarchive ia(in);
		ia >> fpvs_ptr;
	}
	fpRankValueFileStream.close();
	
	fp_value_store=fpvs_ptr;
}
	
void MPHR::writeFpArrayToFile(const string & fpArrayFileName) const{
	ofstream FpArrayFileStream(fpArrayFileName.c_str(),std::ios_base::out|std::ios_base::binary);
	if (!FpArrayFileStream) {
		cerr << "Unable to open fp rank value file: "<<fpArrayFileName <<endl;
		exit(1);
	}
	boost::iostreams::filtering_stream<boost::iostreams::output> out;
	out.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(9)));
	out.push(FpArrayFileStream);
	boost::archive::binary_oarchive oa(out);
	oa << fp_value_store;
	//out.pop();
	//FpArrayFileStream.close();
}





//This file adapter is to be used as a reader with cmph_io_nlfile_adapter
//it only reads up to a tab char on every line
//this function reads one line from data file and returns key as a pointer to the char array and the size of the key
static int ngram_file_read(gzFile data, char **key, cmph_uint32 *keylen){
	gzFile fd = data;
	*key = NULL;
	*keylen = 0;
	char * tabPos=NULL;
	//fprintf(stderr,"Hashing File\n");
	//fflush(stderr);
	while(1)
	{
		if (gzeof(fd)) return -1;
		char buf[BUFSIZ];
		char *c = gzgets(fd,buf, BUFSIZ); 
		if (c == NULL) return -1;
		if (!tabPos){ //no tab char yet so save to string
			tabPos=strchr(buf,'\t');
			uint64_t length=tabPos?(tabPos-buf):strlen(buf);
			
			*key = (char *)realloc(*key, *keylen + length + 1);
			memcpy(*key + *keylen, buf, length);
			*keylen += (cmph_uint32)length;
		}
		if ((buf[strlen(buf) - 1] != '\n')) {
			if (gzeof(fd)&&strlen(buf)>1) break; // this is in case the last line of the file doesn't have \n
			else continue;
		}
		break;
	}
	//	char test[200];
	//	strncpy( test, *key, *keylen);
	//	test[*keylen]='\0';
	//	cerr << "key: " <<test << "len: " << (int)*keylen<<endl;
	return (int)(*keylen);
}


#endif

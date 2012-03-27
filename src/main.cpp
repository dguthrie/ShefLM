/*
 *  main.cpp
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





#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <stdlib.h>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/shared_ptr.hpp>
#include <getopt.h>


#include "MPHR.h"
#include "KneserNeyWrapper.h"


void null_deleter(void const*){}


void print_usage(const char *prg_name){
	
	cerr<< "\nUsage: " << prg_name <<" [-h] [-l inputBaseFileName ] [-g outputBaseFileName] [-f bits_per_fp] [-b bits_per_rank] [-k] [-q queryfile] keyTABvalueFile"
		<< "\n\n\tThis program creates a storage sturcture that stores all the keys and values in the keyTABvalueFile so that they can be looked up quickly. This structure can be saved to disk with the \"-g\" option.  And then loaded later with the \"-l\" option \n\n"
		<< "\tkeyTABvalueFile: this file is used to read keys and values.\n"
		<< "\t\t-This file should be pre-sorted by VALUE (Values should be ascending. So one counts first!)\n"
		<< "\t\tThe format of this file is ngrams and values seprated by tab i.e. NGRAM\\tVALUE\n"
		<< "\t\tAll ngrams in this file MUST be unique!!!\n"
		<< "\t\t-This file can be gzip'ed as long as the extention is gz\n"
		<< "\t-h print this help\n"
		<< "\t-g write all files needed for MPHR structure to disk using the filename prefix specified\n"
		<< "\t\t2 files will be written using the basename prefix specified and ending in .hash and .fp_values\n"
		<< "\t-l load the MPHR structure using the filename prefix specified\n"
		<< "\t\t.hash, and .fp_values files must exist with the given prefix\n"
		<< "\t-f number of bits to use for each fingerprint, default is 12\n"
		<< "\t-b number of bits to use for each rank, default is 20\n"
		<< "\tThe -b and -f options have no effect if loading a structure with the -l option\n"
		<< "\t-k **Compute Kneser Ney perplexity on query file.  In this case query file should be a text file.\n"
		<< "\t\t**This option requires you to have stored special counts needed for KN in your language model."
		<< "\n\n"
		<< " Assuming you have a text file named sample_ngram_file.txt that contains ngrams and their counts (i.e <ngram><TAB><count>)\n"
		<< "Example 1 (store): " << prg_name <<" -g 3gmstore sample_ngram_file.txt\n"
		<< "Example 2 (query): " << prg_name <<" -l 3gmstore -q sample_ngram_file.txt\n"
		<< "Example 3 (query): cat sample_ngram_file.txt | ./" << prg_name <<" -l 3gmstore\n"
		<< "\n"
		<< "Type: 'man " << prg_name <<"' for more information.\n"
		<<endl;
}


// Create minimal perfect hash function from in-memory vector
int main(int argc, char **argv){ 
	
//	static const size_t GOOGLE_WEB_1T_MIXED_NUM_KEYS=3775790711;
	
	const char *queryFileName=NULL;
	bool writeToDiskFlag=false;
	bool loadFromDiskFlag=false;
	bool kneserNeyOptionFlag=false;
	char * mphrLoadFromBaseFilename=NULL;
	char * mphrSaveToBaseFilename=NULL;
	unsigned bits_per_fingerprint=12;
	unsigned bits_per_rank=20;
	size_t unique_bigrams=0;
    
	char c;
	while ((c = getopt (argc, argv, "hk:b:f:q:l:g:")) != -1){
		switch (c){
			case 'h':
				print_usage(argv[0]);
				return 0;
			case 'g':
				writeToDiskFlag=true; 
				mphrSaveToBaseFilename= optarg;
				break;
			case 'l':
				loadFromDiskFlag=true; 
				mphrLoadFromBaseFilename= optarg;
				break;
			case 'f':
				bits_per_fingerprint= atoi(optarg);
				break;
			case 'b':
				bits_per_rank= atoi(optarg);
				break;
			case 'k':
				kneserNeyOptionFlag= true;
				unique_bigrams=atoi(optarg);
				break;
			case 'q':
				queryFileName= optarg;
				break;
			default:
				cerr << " That is not a valid option to the program\n\n";
				print_usage(argv[0]);
				return 0;
		}
	}
	
	
	//non getopt arg is key filename
	const char *keyFileName=NULL;
	if(optind < argc) keyFileName=argv[optind];
	if(keyFileName==NULL && !loadFromDiskFlag){
		cerr << "\nError: No key file specified to create hash! Either use -l option or specify a file." <<endl;
		print_usage(argv[0]);
		exit(0);
	};
	//++optind;
	//if(optind < argc) queryFileName=argv[optind];

	
	boost::shared_ptr<MPHR> pMPHR;
	
	if (loadFromDiskFlag){
		pMPHR.reset(new MPHR(mphrLoadFromBaseFilename));
	}else {
		pMPHR.reset(new MPHR(keyFileName,bits_per_fingerprint,bits_per_rank,mphrSaveToBaseFilename));
	}

	
	if (writeToDiskFlag){
		pMPHR->writeMPHRToFilesWithBaseName(mphrSaveToBaseFilename);
	}

	
	

	
	if (!loadFromDiskFlag && !kneserNeyOptionFlag && !queryFileName){
		//the user has not asked to query anything so we are done
		return 0;
	}
	
	
	boost::shared_ptr<std::istream> queryFIN;
	boost::iostreams::filtering_stream<boost::iostreams::input> qin;
	//either read from named input if arg given or else use stdin
	if (queryFileName) {
		queryFIN.reset(new std::ifstream(queryFileName,std::ios_base::in|std::ios_base::binary));  //autoclosed by destructor when smart pointer goes out of scope
		if (strcmp((queryFileName+strlen(queryFileName)-3),".gz")==0) qin.push(boost::iostreams::gzip_decompressor());
	}
	else  {
		cerr << "Reading input from stdin..."<<endl;
		queryFIN.reset(&std::cin,null_deleter);
	}
	
    qin.push(*queryFIN);


	
	
	if(kneserNeyOptionFlag){
		cout << "Computing Knesser Ney Probablities for query ngrams"<<endl;
		KneserNeyWrapper kn(pMPHR,unique_bigrams);
		double sumlogprob=0.0;
		double Nt=0.0;
		string w1,w2,w3;
		w1=w2="<NA>";
		while (qin>>w3) {
			sumlogprob+=log2(kn.prob(w1,w2,w3));
			Nt+=1;
			w1=w2;
			w2=w3;
		}
		cout << "Knesser Ney Prob is: "<< pow(2.0, (-1/Nt *sumlogprob)) <<endl;
	}else {
		string text;
		string key;
		string countStr="";
		size_t count;
		size_t correct=0;
		size_t incorrect=0;
		size_t notfound=0;
		size_t total=0;
		while( std::getline(qin,text) ) {
			string::size_type loc;
			loc=text.find('\t');
			count=0;
			if( loc != string::npos ) {
				key=text.substr(0, loc);
				countStr=(text.substr(loc+1));
				std::stringstream(countStr)>>count;
			}else {
				key=text;
			}
			size_t value=pMPHR->query(key);
			string shortkey=key.substr(0,50);

			if(count){
				//If there is a count then check to make sure that the count in the file and the stored value are the same
				++total;
				if (value == 0){
					++notfound;
					cerr << "Found a count of 0 for: " <<setw(50)<<shortkey<<endl;
				}else if(count == value){
					++correct;
				}else{
					++incorrect;
					cerr << "Error: value in store is:"<<value<< " is not equal to the count of " << count << " For: " <<setw(50)<<shortkey<<endl;
				}
				
			}else {
				cout <<setw(11)<<value<<" "<<key<<endl;			
			}
		}
		if (total){
			cerr << "\nPrinting accuracy information\n(If you would like the program to return the count of ngrams then the queries should not contain the <tab> character." 
			<< "\nTotal Correct: " <<correct 
			<< "\nTotal Incorrect: " << incorrect
			<< "\nTotal Not Found: " << notfound 
			<< "\nTotal Test Queries: "<<total <<endl;
		}
		
	}
	 qin.pop();
	

    return 0;
}


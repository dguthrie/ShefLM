/*
 *  KneserNeyWrapper.h
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

#ifndef Kneser_Ney_Wrapper_h
#define Kneser_Ney_Wrapper_h

#include "MPHR.h"
#include <iostream>
#include <boost/shared_ptr.hpp>

using std::cerr;
using std::endl;

class KneserNeyWrapper{
public:
	explicit KneserNeyWrapper(boost::shared_ptr<MPHR> mphrPtr,uint64_t unique_bigrams=17643609.0);
	double prob(const string &w1, const string &w2, const string &w3) const;

private:
	long double ngram_freq(const string &ngram) const;

	boost::shared_ptr<MPHR> mphr;
	long double uniqUnigrams;
	long double uniqBigrams;
	long double discount;
};



KneserNeyWrapper::KneserNeyWrapper(boost::shared_ptr<MPHR> mphrPtr,uint64_t unique_bigrams)
	:mphr(mphrPtr)
{
	uniqUnigrams=651930.0;
	uniqBigrams=unique_bigrams;
	discount=0.80;
}

inline long double KneserNeyWrapper::ngram_freq(const string &ngram) const{
	return static_cast<long double>(mphr->query(ngram));
}


double KneserNeyWrapper::prob(const string &w1, const string &w2, const string &w3) const{
		//cerr << "Looking up: " <<w1+" "+w2+" "+w3 << ": " <<ngram_freq(w1+" "+w2+" "+w3)<<endl;
		
		long double IKNuni=0.0;
		long double IKNbi=0.0;
		long double IKNtri=0.0;
		
		if (ngram_freq("<*> "+w3)) IKNuni=(ngram_freq("<*> "+w3)-discount)/uniqBigrams + uniqUnigrams/uniqBigrams *discount*1.0/uniqUnigrams;
		else IKNuni=uniqUnigrams/uniqBigrams *discount*1.0/uniqUnigrams;
		
		if (ngram_freq("<*> "+w2+" "+w3)) {
			//cerr << "<*> w2 w3 "<< ngram_freq("<*> "+w2+" "+w3)<<endl;
			//cerr << "<*> w2 <*>"<< ngram_freq("<*> "+w2+" <*>")<<endl;
			//cerr << "w2 <*> "<< ngram_freq(w2+" <*>")<<endl;
			
			
			IKNbi=(ngram_freq("<*> "+w2+" "+w3)-discount)/ngram_freq("<*> "+w2+" <*>")+ngram_freq(w2+" <*>")*discount/ngram_freq("<*> "+w2+" <*>") * IKNuni;
		}
		else if (ngram_freq("<*> "+w2+" <*>") && ngram_freq(w2+" <*>")) IKNbi=ngram_freq(w2+" <*>")*discount/ngram_freq("<*> "+w2+" <*>") * IKNuni;
		else IKNbi=IKNuni;
		
		if (ngram_freq(w1+" "+w2+" "+w3)) {
			//cerr << "w1 w2 w3 "<< ngram_freq(w1+" "+w2+" "+w3)<<endl;
			//cerr << "w1 w2 "<< ngram_freq(w1+" "+w2)<<endl;
			//cerr << "w1 w2 <*> "<< ngram_freq(w1+" "+w2+" <*>")<<endl;
			
			IKNtri=(ngram_freq(w1+" "+w2+" "+w3)-discount)/ngram_freq(w1+" "+w2) + ngram_freq(w1+" "+w2+" <*>")*discount/ngram_freq(w1+" "+w2) *IKNbi;
		}
		else if (ngram_freq(w1+" "+w2+" <*>")) IKNtri=ngram_freq(w1+" "+w2+" <*>")*discount/ngram_freq(w1+" "+w2) *IKNbi;
		else IKNtri=IKNbi;
		
		//cerr <<"Tri " <<IKNtri<<" Bi " << IKNuni<<" Uni " << IKNuni <<endl;
		if (IKNtri<=0 || IKNtri>1 || std::isnan(IKNtri) || std::isinf(IKNtri) ){ 
			cerr << "PROB IS WRONG for trigram: " <<w1+" "+w2+" "+w3 << endl;
			IKNtri=uniqUnigrams/uniqBigrams *discount*1.0/uniqUnigrams;
		}
		
		//printf("p(%s | %s %s) = %Lf\n\n\n",w3.c_str(),w1.c_str(),w2.c_str(),IKNtri);
		return IKNtri;
		
}


#endif



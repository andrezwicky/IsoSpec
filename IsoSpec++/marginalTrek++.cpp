/*
 *   Copyright (C) 2015-2016 Mateusz Łącki and Michał Startek.
 *
 *   This file is part of IsoSpec.
 *
 *   IsoSpec is free software: you can redistribute it and/or modify
 *   it under the terms of the Simplified ("2-clause") BSD licence.
 *
 *   IsoSpec is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 *   You should have received a copy of the Simplified BSD Licence
 *   along with IsoSpec.  If not, see <https://opensource.org/licenses/BSD-2-Clause>.
 */


#include <cmath>
#include <algorithm>
#include <vector>
#include <stdlib.h>
#include <tuple>
#include <unordered_map>
#include <queue>
#include <utility>
#include <iostream>
#include <string.h>
#include <limits>
#include "lang.h"
#include "marginalTrek++.h"
#include "conf.h"
#include "allocator.h"
#include "operators.h"
#include "summator.h"
#include "element_tables.h"
#include "misc.h"




Conf initialConfigure(const int atomCnt, const int isotopeNo, const double* probs)
{
    Conf res = new int[isotopeNo];

    for(int i = 0; i < isotopeNo; ++i )
    {
        res[i] = int( atomCnt * probs[i] ) + 1;
    }

    int s = 0;

    for(int i = 0; i < isotopeNo; ++i) s += res[i];

    int diff = atomCnt - s;

    // Too little: enlarging fist index.
    if( diff > 0 ){
        res[0] += diff;
    }
    // Too much: trying to redistribute the difference: hopefully the first element is the largest.
    if( diff < 0 ){
        diff = abs(diff);
        int i = 0, coordDiff = 0;

        while( diff > 0){
            coordDiff = res[i] - diff;

            if( coordDiff >= 0 ){
                res[i] -= diff;
                diff = 0;
            } else {
                res[i] = 0;
                i++;
                diff = abs(coordDiff);
            }
        }
    }
    return res;
}


#ifndef BUILDING_R
void printMarginal( const std::tuple<double*,double*,int*,int>& results, int dim)
{
    for(int i=0; i<std::get<3>(results); i++){

        std::cout << "Mass = "  << std::get<0>(results)[i] <<
        " log-prob =\t"                 << std::get<1>(results)[i] <<
        " prob =\t"                     << exp(std::get<1>(results)[i]) <<
        "\tand configuration =\t";

        for(int j=0; j<dim; j++) std::cout << std::get<2>(results)[i*dim + j] << " ";

        std::cout << std::endl;
    }
}
#endif


double* getMLogProbs(const double* probs, int isoNo)
{
    double* ret = new double[isoNo];
    for(int i = 0; i < isoNo; i++)
    {
        ret[i] = log(probs[i]);
        for(int j=0; j<NUMBER_OF_ISOTOPIC_ENTRIES; j++)
            if(elem_table_probability[j] == probs[i])
            {
                ret[i] = elem_table_log_probability[j];
                break;
            }
    }
    return ret;
}


MarginalTrek::MarginalTrek(
    const double* masses,   // masses size = logProbs size = isotopeNo
    const double* probs,
    int isotopeNo,                  // No of isotope configurations.
    int atomCnt,
    int tabSize,
    int hashSize
) : current_count(0),
_tabSize(tabSize),
_hashSize(hashSize),
_isotopeNo(isotopeNo),
_atomCnt(atomCnt),
iso_masses(array_copy<double>(masses, isotopeNo)),
logProbs(getMLogProbs(probs, isotopeNo)),
allocator(isotopeNo, _tabSize),
keyHasher(isotopeNo),
equalizer(isotopeNo),
orderMarginal(logProbs, isotopeNo),
visited(_hashSize,keyHasher,equalizer),
pq(orderMarginal),
totalProb(),
candidate(new int[isotopeNo])
{


    int*    initialConf_tmp = initialConfigure(atomCnt, isotopeNo, probs);
    int*    initialConf = allocator.makeCopy(initialConf_tmp);
    delete [] initialConf_tmp;


    pq.push(initialConf);
    visited[initialConf] = 0;

    bool cont = true;
    double last_prob = -10000.0;

    double cconf = -INFINITY;
    while(cont)
    {
        cont = add_next_conf();
        cconf = _conf_probs.back();
        if(cconf < last_prob)
            break;
        last_prob = cconf;

    }
    // Find max element in case loop terminated from cont==false, and not from break

    std::vector<double>::iterator max_el_it = std::max_element(_conf_probs.begin(), _conf_probs.end());
    int max_idx = std::distance(_conf_probs.begin(), max_el_it);

    initialConf = _confs[max_idx];

    pq = std::priority_queue<Conf,std::vector<Conf>,ConfOrderMarginal>(orderMarginal);
    visited = std::unordered_map<Conf,int,KeyHasher,ConfEqual>(_hashSize,keyHasher,equalizer);
    _conf_probs.clear();
    _conf_masses.clear();
    _confs.clear();

    totalProb = Summator();

    pq.push(initialConf);
    visited[initialConf] = 0;

    current_count = 0;

    add_next_conf();


}


bool MarginalTrek::add_next_conf()
{
    if(pq.size() < 1) return false;

    Conf topConf = pq.top();
    pq.pop();
    ++current_count;
    visited[topConf] = current_count;

    _confs.push_back(topConf);
    _conf_masses.push_back(mass(topConf, iso_masses, _isotopeNo));
    double logprob = logProb(topConf, logProbs, _isotopeNo);
    _conf_probs.push_back(logprob);


    totalProb.add( exp( logprob ) );

    for( unsigned int i = 0; i < _isotopeNo; ++i )
    {
        for( unsigned int j = 0; j < _isotopeNo; ++j )
        {
            // Growing index different than decreasing one AND
            // Remain on simplex condition.
            if( i != j && topConf[j] > 0 ){
                copyConf(topConf, candidate, _isotopeNo);

                ++candidate[i];
                --candidate[j];

                // candidate should not have been already visited.
                if( visited.count( candidate ) == 0 )
                {
                    Conf acceptedCandidate = allocator.makeCopy(candidate);
                    pq.push(acceptedCandidate);

                    visited[acceptedCandidate] = 0;
                }
            }
        }
    }

    return true;
}

int MarginalTrek::processUntilCutoff(double cutoff)
{
    Summator s;
    int last_idx = -1;
    for(unsigned int i=0; i<_conf_probs.size(); i++)
    {
        s.add(_conf_probs[i]);
        if(s.get() >= cutoff)
        {
            last_idx = i;
            break;
        }
    }
    if(last_idx > -1)
        return last_idx;

    while(totalProb.get() < cutoff && add_next_conf()) {}
    return _conf_probs.size();
}

double MarginalTrek::getLightestConfMass()
{
    double ret_mass = std::numeric_limits<double>::infinity();
    for(unsigned int ii=0; ii < _isotopeNo; ii++)
        if( ret_mass > iso_masses[ii] )
	    ret_mass = iso_masses[ii];
    return ret_mass*_atomCnt;
}

double MarginalTrek::getHeaviestConfMass()
{
    double ret_mass = 0.0;
    for(unsigned int ii=0; ii < _isotopeNo; ii++)
        if( ret_mass < iso_masses[ii] )
            ret_mass = iso_masses[ii];
    return ret_mass*_atomCnt;
}

MarginalTrek::~MarginalTrek()
{
    delete[] logProbs;
    delete[] candidate;
    delete[] iso_masses;
}

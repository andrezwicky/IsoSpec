/*
 *   Copyright (C) 2015-2018 Mateusz Łącki and Michał Startek.
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

#pragma once

#include <tuple>
#include <unordered_map>
#include <queue>
#include <atomic>
#include "conf.h"
#include "allocator.h"
#include "operators.h"
#include "summator.h"


namespace IsoSpec
{

Conf initialConfigure(int atomCnt, int isotopeNo, const double* probs);


void printMarginal(const std::tuple<double*,double*,int*,int>& results, int dim);

//! The marginal distribution class (a subisotopologue).
/*!
    This class mostly provides some basic common API for subclasses, but itself is not abstract.
    This class represents the probability distribution generated by one element only -- a subisotopologue.
    For instance, it might be the distribution of C200, that might be part of, say, C200H402.
    It corresponds to the multinomial distribution, where each configuration can also be attributed a precise mass.
    The constructor method perform initial hill-climbing to find the most probable sub-isotopologue (the mode).
*/
class Marginal
{
private:
    bool disowned;
protected:
    const unsigned int isotopeNo;       /*!< The number of isotopes of the given element. */
    const unsigned int atomCnt;         /*!< The number of atoms of the given element. */
    const double* const atom_masses;    /*!< Table of atomic masses of all the isotopeNo isotopes. */
    const double* const atom_lProbs;    /*!< Table of log-probabilities of all the isotopeNo isotopes. */
    const double loggamma_nominator;    /*!< The constant nominator that appears in the expressions for the multinomial probabilities. */
    const Conf mode_conf;               /*!< A subisotopologue with most probability. If not unique, one of the representatives of that class of subisotopologues. */
    const double mode_lprob;            /*!< The log-probability of the mode subisotopologue.*/
    const double mode_mass;             /*!< The mass of the mode subisotopologue.*/
    const double mode_eprob;            /*!< The probability of the mode subisotopologue.*/
    const double smallest_lprob;        /*!< The smallest-achievable log-probability in the distribution of subisotopologues. */


public:
    //! Class constructor.
    /*!
        \param _masses A table of masses of the stable isotopes of the investigated element, e.g. for C10 it is 2: C12 and C13.
        \param _probs A table of natural frequencies of the stable isotopes of the investigated element, see IUPAC at https://iupac.org/isotopesmatter/
        \param _isotopeNo Number of isotopes of a given element.
        \param _atomCnt The number of atoms of the given element, e.g. 10 for C10.
        \return An instance of the Marginal class.
    */
    Marginal(
        const double* _masses,   // masses size = logProbs size = isotopeNo
        const double* _probs,
        int _isotopeNo,
        int _atomCnt
    );

    // Get rid of the C++ generated copy and assignment constructors.
    Marginal(Marginal& other) = delete;
    Marginal& operator= (const Marginal& other) = delete;

    //! Move constructor.
    Marginal(Marginal&& other);

    //! Destructor.
    virtual ~Marginal();

    //! Get the number of isotopes of the investigated element.
    /*! 
        \return The integer number of isotopes of the investigated element.
    */
    inline int get_isotopeNo() const { return isotopeNo; };

    //! Get the mass of the lightest subisotopologue.
    /*! This is trivially obtained by considering all atomNo atoms to be the lightest isotope possible.
        \return The mass of the lightiest subisotopologue. 
    */
    double getLightestConfMass() const;

    //! Get the mass of the heaviest subisotopologue.
    /*! This is trivially obtained by considering all atomNo atoms to be the heaviest isotope possible.
        \return The mass of the heaviest subisotopologue. 
    */
    double getHeaviestConfMass() const;

    //! Get the log-probability of the mode subisotopologue.
    /*!
        \return The log-probability of a/the most probable subisotopologue. 
    */
    inline double getModeLProb() const { return mode_lprob; };

    //! The the mass of the mode subisotopologue.
    /*!
        \return The mass of one of the most probable subisotopologues. 
    */
    inline double getModeMass() const { return mode_mass; };

    //! The the probability of the mode subisotopologue.
    /*!
        \return The probability of a/the most probable subisotopologue. 
    */
    inline double getModeEProb() const { return mode_eprob; };

    //! The the log-probability of the lightest subisotopologue.
    /*!
        \return The logarithm of the  smallest non-zero probability of a subisotopologue. 
    */

    inline double getSmallestLProb() const { return smallest_lprob; };

    //! Calculate the log-probability of a given subisotopologue.
    /*!
        \param conf A subisotopologue (a table of integers describing subsequent isotope-counts).
        \return The log-probability of the input subisotopologue.
    */
    inline double logProb(Conf conf) const { return loggamma_nominator + unnormalized_logProb(conf, atom_lProbs, isotopeNo); };
};


//! The marginal distribution class (a subisotopologue).
class MarginalTrek : public Marginal
{
private:
    int current_count;
    const KeyHasher keyHasher;
    const ConfEqual equalizer;
    const ConfOrderMarginal orderMarginal;
    std::unordered_map<Conf,int,KeyHasher,ConfEqual> visited;
    std::priority_queue<Conf,std::vector<Conf>,ConfOrderMarginal> pq;
    Summator totalProb;
    Conf candidate;
    Allocator<int> allocator;
    std::vector<double> _conf_lprobs;
    std::vector<double> _conf_masses;
    std::vector<int*> _confs;

    //! Proceed to the next configuration and memoize it (as it will be surely needed).
    bool add_next_conf();

public:
    //! Move constructor: specializes the Marginal class.
    /*!
        \param tabSize The size of the table used to store configurations in the allocator.
        \param hashSize The size of the hash table used to store visited subisotopologues.
    */
    MarginalTrek(
        Marginal&& m,
        int tabSize = 1000,
        int hashSize = 1000
    );

    //! Check if the table of computed subisotopologues does not have to be extended.
    /*!
        This function checks if the idx-th most probable subisotopologue was memoized and if not, computes it and memoizes it.

        \param idx The number of the idx-th most probable subisotopologue.
        \return Returns false if it the provided idx exceeds the total number of subisotopologues.
    */
    inline bool probeConfigurationIdx(int idx)
    {
        while(current_count <= idx)
            if(!add_next_conf())
                return false;
        return true;
    }


    //! Calculate subisotopologues with probability above or equal to the cut-off.
    /*!
        \param cutoff The probability cut-off 
        \return The number of the last subisotopologue above the cut-off. 
    */
    int processUntilCutoff(double cutoff);

    inline const std::vector<double>& conf_lprobs() const { return _conf_lprobs; };
    inline const std::vector<double>& conf_masses() const { return _conf_masses; };
    inline const std::vector<int*>& confs() const { return _confs; };


    virtual ~MarginalTrek();
};


//! Precalculated Marginal class
/*!
    This class serves to calculate a set of isotopologues that 
    is defined by the minimal probability threshold.

    This works faster than if you did not know the threshold.
    If you have no idea about the threshold, you would need to call us,
    to change encode the layered version of the marginal.
*/
class PrecalculatedMarginal : public Marginal
{
protected:
    std::vector<Conf> configurations;
    Conf* confs;
    unsigned int no_confs;
    double* masses;
    double* lProbs;
    double* eProbs;
    Allocator<int> allocator;
public:
    //! The move constructor (disowns the Marginal).
    /*!
        This constructor memoizes all subisotopologues with log-probability above the provided threshold lCutOff 
        \param Marginal An instance of the Marginal class this class is about to disown.
        \param lCutOff The lower limit on the log-probability of the precomputed subisotopologues.
        \param sort Should the subisotopologues be stored with descending probability ?
        \return An instance of the PrecalculatedMarginal class.
    */
    PrecalculatedMarginal(
        Marginal&& m,
        double lCutOff,
        bool sort = true,
        int tabSize = 1000,
        int hashSize = 1000
    );

    //! Destructor.
    virtual ~PrecalculatedMarginal();
    
    //! Is there a subisotopologue with a given number?
    /*!
        \return Returns true if idx does not exceed the number of pre-computed configurations. 
    */
    inline bool inRange(unsigned int idx) const { return idx < no_confs; };

    //! Get the log-probability of the idx-th subisotopologue.
    /*!
        \param idx The number of the considered subisotopologue.
        \return The log-probability of the idx-th subisotopologue.
    */
    inline const double& get_lProb(int idx) const { return lProbs[idx]; };

    //! Get the probability of the idx-th subisotopologue.
    /*!
        \param idx The number of the considered subisotopologue.
        \return The probability of the idx-th subisotopologue.
    */
    inline const double& get_eProb(int idx) const { return eProbs[idx]; };

    //! Get the mass of the idx-th subisotopologue.
    /*!
        \param idx The number of the considered subisotopologue.
        \return The mass of the idx-th subisotopologue.
    */
    inline const double& get_mass(int idx) const { return masses[idx]; };

    //! Get the table of the log-probabilities of subisotopologues.
    /*!
        \return Pointer to the first element in the table storing log-probabilities of subisotopologues.
    */
    inline const double* get_lProbs_ptr() const { return lProbs; };

    //! Get the table of the masses of subisotopologues.
    /*!
        \return Pointer to the first element in the table storing masses of subisotopologues.
    */
    inline const double* get_masses_ptr() const { return masses; };


    //! Get the counts of isotopes that define the subisotopologue.
    /*!
        \param idx The number of the considered subisotopologue.
        \return The counts of isotopes that define the subisotopologue.
    */
    inline const Conf& get_conf(int idx) const { return confs[idx]; };

    //! Get the number of precomputed subisotopologues.
    /*!
        \return The number of precomputed subisotopologues.
    */
    inline unsigned int get_no_confs() const { return no_confs; };
};

} // namespace IsoSpec


# -*- coding: utf-8 -*-
#
#   Copyright (C) 2015-2016 Mateusz Łącki and Michał Startek.
#
#   This file is part of IsoSpec.
#
#   IsoSpec is free software: you can redistribute it and/or modify
#   it under the terms of the Simplified ("2-clause") BSD licence.
#
#   IsoSpec is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#   You should have received a copy of the Simplified BSD Licence
#   along with IsoSpec.  If not, see <https://opensource.org/licenses/BSD-2-Clause>.
#

import itertools
import math  # better do it in numpy
import re

from kahan import Summator
from collections import defaultdict
import numpy as np
from isoFFI import isoFFI
import PeriodicTbl

try:
    xrange
except NameError:
    xrange = range


# class IsoSpecNew(object):


class IsoSpec:
    def __init__(self,
                 _atomCounts,
                 _isotopeMasses,
                 _isotopeProbabilities,
                 _stopCondition,
                 tabSize=1000,
                 hashSize=1000,
                 step=0.3,
                 trim=True,
                 method='layered'):

        self.clib = isoFFI.clib #can't use global vars in destructor, again...
        self.dimNumber                 = len(_atomCounts)
        self._isotopeNumbers           = [len(x) for x in _isotopeMasses]
        self.allIsotopeNumber         = sum(self._isotopeNumbers)
        self._atomCounts               = _atomCounts
        self._isotopeMasses            = _isotopeMasses
        self._isotopeProbabilities     = _isotopeProbabilities
        self._stopCondition            = _stopCondition
        self.tabSize                   = tabSize
        self.hashSize                  = hashSize
        self.trim                      = trim

        try:
            self.algo = {'layered': 0,
                         'ordered': 1,
              'threshold_absolute': 2,
              'threshold_relative': 3,
              'layered_estimating': 4,
            }[method]
        except KeyError:
            raise Exception("Invalid ISO method")

        self.iso = isoFFI.clib.setupIso(
            self.dimNumber,
            self._isotopeNumbers,
            _atomCounts,
            list(itertools.chain.from_iterable(_isotopeMasses)),
            list(itertools.chain.from_iterable(_isotopeProbabilities)),
            _stopCondition,
            self.algo,
            tabSize,
            hashSize,
            step,
            trim)
        try:
            if not self.iso.__nonzero__():
                raise MemoryError()
        except AttributeError: # Python3 doesn't have __nonzero__... Nor any obvious alternative...
            pass



    @staticmethod
    def IsoFromFormula(formula, cutoff, tabSize=1000, hashSize=1000,
                       classId=None, method='layered_estimating',
                       step=0.25, trim=True):
        # It's much easier to just parse it in python than to use the C parsing function
        # and retrieve back into Python the relevant object sizes
        symbols = re.findall("\D+", formula)
        atom_counts = [int(x) for x in re.findall("\d+", formula)] #TODO: eliminate this (use precompiled)

        if not len(symbols) == len(atom_counts):
            raise ValueError("Invalid formula")

        try:
            masses = tuple(PeriodicTbl.symbol_to_masses[symbol] for symbol in symbols)
            probs = tuple(PeriodicTbl.symbol_to_probs[symbol] for symbol in symbols)
        except KeyError:
            raise ValueError("Invalid formula")

        if classId == None:
            return IsoSpec(atom_counts, masses, probs, cutoff, tabSize, hashSize, step, trim, method)
        else:
            return classId(atom_counts, masses, probs, cutoff, tabSize, hashSize, trim)



    def __del__(self):
        self.cleanup()

    def cleanup(self):
        if self.iso is not None:
            self.clib.destroyIso(self.iso)
            self.iso = None

    def __len__(self):
        return isoFFI.clib.getIsoConfNo(self.iso)

    def getConfsRaw(self):
        masses = isoFFI.ffi.new("double[{0}]".format(len(self)))
        logProbs = isoFFI.ffi.new("double[{0}]".format(len(self)))
        isoCounts = isoFFI.ffi.new("int[{0}]".format(len(self)*sum(self._isotopeNumbers)))
        isoFFI.clib.getIsoConfs(self.iso, masses, logProbs, isoCounts)
        return (masses, logProbs, isoCounts)

    def get_conf_by_no(self, clist, idx):
        idx *= self.allIsotopeNumber
        ret = []
        for ison in self._isotopeNumbers:
            ret.append(tuple(clist[idx:idx+ison]))
            idx += ison
        return tuple(ret)


    def getConfs(self):
        masses, logProbs, isoCounts = self.getConfsRaw()
        rows_no = len(masses)
        cols_no = len(isoCounts)/len(masses)
        masses  = list(masses)
        logProbs= list(logProbs)
        confs = []
        for i in xrange(rows_no-1):
            confs.append(list(isoCounts[i*cols_no:(i+1)*cols_no]))
        return masses, logProbs, confs

    def getConfsNumpy(self):
        masses, logProbs, configurations = self.getConfsRaw()
        rows_no = len(masses)
        cols_no = len(configurations)/len(masses)
        masses  = np.array(list(masses))
        logProbs= np.array(list(logProbs))
        configurations = np.array(list(configurations)).reshape((rows_no,cols_no))
        return masses, logProbs, configurations

    def splitConf(self, l, offset = 0):
        conf = []
        idx = self.allIsotopeNumber * offset
        for i in xrange(self.dimNumber):
            conf.append(tuple(l[idx:idx+self._isotopeNumbers[i]]))
            idx += self._isotopeNumbers[i]
        return tuple(conf)

    def confStr(self, conf):
        return '\t'.join([' '.join([str(x) for x in y]) for y in conf])

    def printConfs(self):
        masses, logProbs, isoCounts = self.getConfsRaw()
        confs = []
        step = sum(self._isotopeNumbers)
        for i in xrange(len(masses)):
            confs.append((masses[i], logProbs[i], self.splitConf(isoCounts, i)))

        for conf in confs:
            print(("Mass = {0}\t and log-prob = {1} and prob = {2}\t and configuration"\
                  "=\t{3}").format(conf[0], conf[1], math.exp(conf[1]), self.confStr(conf[2])))



class IsoPlot(dict):
    def __init__(self, iso, bin_w):
        self.iso = iso
        self.bin_w = bin_w
        masses, logProbs, _isoCounts = iso.getConfsRaw()
        dd = defaultdict(Summator)
        for i in xrange(len(masses)):
            dd[float(int(masses[i]/bin_w))*bin_w].add(math.exp(logProbs[i]))
        for key, val in dd.items():
            self[key] = val.get()


def IsoSpecify( formula,
                cutoff,
                method= 'layered',
                output_format = 'numpy_arrays',
                trim  = True,
                _step = 0.25,
                _trim = True,
                _tabSize  = 1000,
                _hashSize = 1000    ):
    """
    Call IsoSpec on a formula with a given cutoff.

    This function wraps around the IsoSpec class.

    Parameters
    ----------
    formula : char
        a string of a form '< Element Tag 1 >< Count 1 > ... ',
        e.g. 'C100H202'. Using IUPAC conventions to name elements.

    cutoff : float
        The cutoff value. See description of the method argument.

    method : char
        Can take one of the following values: 'layered',
        'layered_estimating', 'threshold_absolute',
        'threshold_relative', 'ordered'.

        The threshold versions of the algorithm rely on user
        providing a precise lower bound on the reported peak heights.
        This can be specified in absolute terms ('threshold_absolute'),
        i.e. in terms of the limiting probability of the isotopologue,
        or as a percentage of the heighest peak ('threshold_relative').

        The layered versions of the algorithm rely on calculating
        consecutive values of peak thresholds on flight.
        The ultimate goal is to reach a peak probability that assures
        that the sum of probabilities of the more probable isotopologues
        exceeds the provided cutoff value.
        The sequence of consecutive thresholds can be generated in
        two ways. The default way, 'layered_estimating', estimates
        the threshold to joint probability function by a progressive
        linear spline, check Anal Chem. 2017 Mar 21;89(6):3272-3277.
        doi: 10.1021/acs.analchem.6b01459. Epub 2017 Mar 8.
        The other way, 'layered', estimates a threshold as a 30%%
        quantile of the probabilities gathered in the fringe set, i.e.
        isotopologues that are direct neighbours of the previously
        accepted layer. Finally, choosing the 'ordered' version will
        provide a loglinear version of the algorithm that relies on
        the priority queue. This version automatically sorts
        the configurations by their probability.

    trim
        while using a layered method, should one discard superfluously
        obtained isotopologues, i.e. such that without them the set of
        reported isotopologues already is an optimal p-set.

    output_format
        Should the output be presented as lists ('lists'),
        or as numpy arrays ('numpy_arrays').

    Returns
    -------
    masses
        masses of isotopologues, either a list or a numpy array.

    logProbs
        logarithms of probabilities (theoretical heights) of isotopologues,
        either a list or a numpy array.

    confs
        counts of isotopologues (extended chemical formulas that
        include counts of isotopes of elements)
        """


    assert output_format in ('lists', 'numpy_arrays'), "Wrong value of output_format. Should be either 'lists' or 'numpy_arrays'."

    assert method in ('layered', 'ordered', 'threshold_absolute', 'threshold_relative', 'layered_estimating'), "Wrong value of method. Should be among 'layered', 'ordered', 'threshold_absolute', 'threshold_relative', or 'layered_estimating'."

    assert isinstance(cutoff, float), "Provided cut off ain't a float."

    assert isinstance(formula, str), "Provided formula off ain't a string."

    iso = IsoSpec.IsoFromFormula(   formula,
                                    cutoff,
                                    tabSize = 1000,
                                    hashSize = 1000,
                                    classId = None,
                                    method = method,
                                    step = 0.25,
                                    trim = trim )

    if output_format == 'lists':
        masses, logProbs, confs = iso.getConfs()
    else:
        masses, logProbs, confs = iso.getConfsNumpy()

    # print 'Rev Startek is a silly old chump and his mother dresses up silly.'
    return masses, logProbs, confs

version = '1.0.5'

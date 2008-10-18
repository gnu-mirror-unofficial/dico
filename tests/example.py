#!/usr/bin/python
# -*- coding: utf-8 -*-
# Copyright (C) 2008 Sergey Poznyakoff
# Copyright (C) 2008 Wojciech Polak
# 
# GNU Dico is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
# 
# GNU Dico is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>.

import dico
import sys

class DicoResult:
    result = {}

    def __init__ (self, *argv):
        self.result = argv[0]

    def count (self):
        return len (self.result)

    def output (self, n):
        pass

    def append (self, elt):
        self.result.append (elt)


class DicoDefineResult (DicoResult):
    def output (self, n):
        print "%d. %s" % (n + 1, self.result[n])
        print "---------",

class DicoMatchResult (DicoResult):
    def output (self, n):
        sys.stdout.softspace = 0
        print self.result[n],
        
class DicoModule:
    adict =  {}
    dbname = ''
    filename = ''
    mod_descr = ''
    mod_info = []

    def __init__ (self, *argv):
        self.filename = argv[0]
        pass
    
    def open (self, dbname):
        self.dbname = dbname
        file = open (self.filename, "r")
        for line in file:
            if line.startswith ('--'):
                continue
            if line.startswith ('descr: '):
                self.mod_descr = line[7:].strip (' \n')
                continue
            if line.startswith ('info: '):
                self.mod_info.append (line[6:].strip (' \n'))
                continue
            f = line.strip (' \n').split (' ', 1)
            if len (f) == 2:
                self.adict[f[0].lower()] = f[1].strip (' ')
        file.close()
        return True

    def close (self):
        return True

    def descr (self):
        return self.mod_descr

    def info (self):
        return '\n'.join (self.mod_info)
    
    def define_word (self, word):
        if self.adict.has_key (word):
            return DicoDefineResult ([self.adict[word]])
        return False
    
    def match_word (self, strat, word):
        if strat.name == "exact":
            if self.adict.has_key (word.lower ()):
                return DicoMatchResult ([self.adict[word.lower()]])
        elif strat.name == "prefix":
            res = []
            for w in self.adict:
                if w.startswith (word):
                    res.append (w)
            if len (res):
                return DicoMatchResult (res)
        elif strat.has_selector:
            res = DicoMatchResult ([])
            for k in self.adict:
                if strat.select (word, k):
                    res.append (k)
            if res.count > 0:
                return res
        return False

    def output (self, rh, n):
        rh.output (n)
        return True

    def result_count (self, rh):
        return rh.count ()
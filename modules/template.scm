;;;; This file is part of Dico.
;;;; Copyright (C) 2008 Sergey Poznyakoff
;;;; 
;;;; Dico is free software; you can redistribute it and/or modify
;;;; it under the terms of the GNU General Public License as published by
;;;; the Free Software Foundation; either version 3, or (at your option)
;;;; any later version.
;;;; 
;;;; Dico is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;; GNU General Public License for more details.
;;;; 
;;;; You should have received a copy of the GNU General Public License
;;;; along with Dico.  If not, see <http://www.gnu.org/licenses/>. */

(define-module (dico)
  #:use-module (guile-user))

(define (open-module name . rest)
  #f)

(define (close-module dbh)
  #f)

(define (descr dbh)
  #f)

(define (info dbh)
  #f)

(define (define-word dbh word)
  #f)

(define (match-word dbh strat word)
  #f)

(define (output rh n)
  #f)

(define (result-count rh)
  #f)

(define-public (dico-init arg)
  (list (cons "open" open-module)
        (cons "close" close-module)
        (cons "descr" descr)
        (cons "info" info)
        (cons "define" define-word)
        (cons "match" match-word)
        (cons "output" output)
        (cons "result-count" result-count)))
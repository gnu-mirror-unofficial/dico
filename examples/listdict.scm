;;;; This file is part of GNU Dico.
;;;; Copyright (C) 2008-2021 Sergey Poznyakoff
;;;; 
;;;; GNU Dico is free software; you can redistribute it and/or modify
;;;; it under the terms of the GNU General Public License as published by
;;;; the Free Software Foundation; either version 3, or (at your option)
;;;; any later version.
;;;; 
;;;; GNU Dico is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;; GNU General Public License for more details.
;;;; 
;;;; You should have received a copy of the GNU General Public License
;;;; along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

(define-module (listdict)
  #:use-module (guile-user)
  #:use-module (ice-9 format)
  #:use-module (srfi srfi-1))

(define-syntax db:get
  (syntax-rules (info descr name lang mime corpus)
    ((db:get dbh name)
     (list-ref dbh 0))
    ((db:get dbh descr)
     (list-ref dbh 1))
    ((db:get dbh info)
     (list-ref dbh 2))
    ((db:get dbh lang)
     (list-ref dbh 3))
    ((db:get dbh mime)
     (list-ref dbh 4))
    ((db:get dbh corpus)
     (list-tail dbh 5))))

(define (open-module name . rest)
  (let ((args rest))
;    (fluid-set! %default-port-encoding "utf-8")
    (set-port-encoding! (current-error-port) "utf-8")
    (cond
     ((null? args)
      (format (current-error-port) "open-module: not enough arguments\n")
      #f)
     ((not (null? (cdr args)))
      (format (current-error-port) "open-module: too many arguments ~A\n" rest)
      #f)
     (else
      (let ((db (with-input-from-file
		    (car args)
		  (lambda ()
		    (set-port-encoding! (current-input-port) "utf-8")
		    (read)))))
	(write db (current-error-port))
	(cond
	 ((list? db) (cons name db))
	 (else
	  (format (current-error-port) "open-module: ~A: invalid format\n"
		  (car args))
	  #f)))))))

(define (descr dbh)
  (db:get dbh descr))

(define (info dbh)
  (db:get dbh info))

(define (lang dbh)
  (db:get dbh lang))

(define (define-word dbh word)
  (let ((res (filter-map (lambda (elt)
			   (and (string-ci=? word (car elt))
				elt))
			 (db:get dbh corpus))))
    (and res (cons #t res))))

(define (match-exact dbh strat word)
  (filter-map (lambda (elt)
		(and (string-ci=? word (car elt))
		     (car elt)))
	      (db:get dbh corpus)))

(define (match-prefix dbh strat word)
  (filter-map (lambda (elt)
		(and (string-prefix-ci? word (car elt))
		     (car elt)))
	      (db:get dbh corpus)))

(define (match-suffix dbh strat word)
  (filter-map (lambda (elt)
		(and (string-suffix-ci? word (car elt))
		     (car elt)))
	      (db:get dbh corpus)))

(define (match-selector dbh strat key)
  (filter-map (lambda (elt)
		(and (dico-strat-select? strat (car elt) key)
		     (car elt)))
	  (db:get dbh corpus)))

(define strategy-list
  (list (cons "exact"  match-exact)
	(cons "prefix"  match-prefix)
	(cons "suffix"  match-suffix)))

(define match-default match-prefix)

(define (match-word dbh strat key)
  (let ((sp (assoc (dico-strat-name strat) strategy-list)))
    (let ((res (cond
		(sp
		 ((cdr sp) dbh strat (dico-key->word key)))
		((dico-strat-selector? strat)
		 (match-selector dbh strat key))
		(else
		 (match-default dbh strat (dico-key->word key))))))
      (if res
	  (cons #f res)
	  #f))))

(define (output rh n)
  (set-port-encoding! (current-output-port) "utf-8")
  (if (car rh)
      ;; Result comes from DEFINE command
      (let ((res (list-ref (cdr rh) n)))
	(cond
	 ((string=? (dico-current-markup) "html")
	  (format #t "<dt>~A</dt>~%<dd>~A</dd>"
		  (car res) (cdr res)))
	 (else
	  (display (car res))
	  (newline)
	  (display (cdr res)))))
      ;; Result comes from MATCH command
      (display (list-ref (cdr rh) n))))

(define (result-count rh)
  (length (cdr rh)))

(define (str-8bit? str)
    (string-any
     (lambda (c)
       (> (char->integer c) 127))
     str))

(define (result-8bit? rh)
  (call-with-current-continuation
   (lambda (return)
     (for-each
      (lambda (s)
	(if (str-8bit? (cdr s)) (return #t)))
      (cdr rh))
     #f)))

(define (result-headers rh hdr)
  (if (result-8bit? rh)
      (assoc-set! hdr "Content-Transfer-Encoding" "quoted-printable")
      hdr))

(define (db-mime-header dbh)
  "Content-Type: text/plain; charset=UTF-8\n\
Content-Transfer-Encoding: 8bit\n")  

(define-public (listdict-init arg)
  (list (cons "open" open-module)
        (cons "descr" descr)
        (cons "info" info)
	(cons "lang" lang)
        (cons "define" define-word)
        (cons "match" match-word)
        (cons "output" output)
        (cons "result-count" result-count)
	(cons "result-headers" result-headers)
	(cons "db-mime-header" db-mime-header)))
;;
(dico-register-strat "suffix" "Match word suffixes")
(dico-register-markup "html")

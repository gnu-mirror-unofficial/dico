ifneq (,$(wildcard Makefile))
 include Makefile
endif

DICTFMT=dictfmt

all: $(DICTS)
clean:; rm -f $(DICTS) *.txt

eng-num.dict eng-num.index: eng-num.txt
	$(DICTFMT) -f -s 'English numerals from 1 to 200' \
                -u http://git.gnu.org.ua/cgit/dico.git \
                --without-time \
                --without-header \
                --index-keep-orig eng-num < eng-num.txt

eng-num_allchars.dict eng-num_allchars.index: eng-num.txt
	$(DICTFMT) -f -s 'English numerals from 1 to 200' \
                -u http://git.gnu.org.ua/cgit/dico.git \
                --without-time \
                --without-header \
                --allchars eng-num_allchars < eng-num.txt

ell-num.dict ell-num.index: ell-num.txt
	$(DICTFMT) -f -s 'Greek numerals from 1 to 200' \
                -u http://git.gnu.org.ua/cgit/dico.git \
                --without-time \
                --without-header \
		--utf8 \
                --index-keep-orig \
		--headword-separator / \
		--break-headwords ell-num-tmp < ell-num.txt
	awk -f deaccent.awk ell-num-tmp.index | \
           LC_ALL=C sort -k 1 -t $$'\t' > ell-num.index
	rm ell-num-tmp.index
	mv ell-num-tmp.dict ell-num.dict

ell-num_mime.dict ell-num_mime.index: ell-num.txt
	$(DICTFMT) -f -s 'Greek numerals from 1 to 200' \
                -u http://git.gnu.org.ua/cgit/dico.git \
                --without-time \
                --without-header \
		--utf8 \
                --index-keep-orig \
		--headword-separator / \
		--mime-header='Content-Type: text/plain; charset=utf-8%Content-Transfer-Encoding: base64'\
		--break-headwords ell-num_mime-tmp < ell-num.txt
	awk -f deaccent.awk ell-num_mime-tmp.index | \
           LC_ALL=C sort -k 1 -t $$'\t' > ell-num_mime.index
	awk '/^Content-Type/ { gsub(/%/, "\n") } { print }' ell-num_mime-tmp.dict > ell-num_mime.dict
	rm ell-num_mime-tmp.dict ell-num_mime-tmp.index

num-eng.dict num-eng.index: num-eng.txt
	$(DICTFMT) -f -s 'Numbers to English numerals' \
                -u http://git.gnu.org.ua/cgit/dico.git \
                --without-time \
                --without-header \
                --index-keep-orig num-eng < num-eng.txt

num-ell.dict num-ell.index: num-ell.txt
	$(DICTFMT) -f -s 'Numbers to Greek numerals' \
                -u http://git.gnu.org.ua/cgit/dico.git \
                --without-time \
                --without-header \
		--utf8 \
                --index-keep-orig num-ell < num-ell.txt

%-num.txt: %-num.lst %-num.info
	(cat $*-num.info; awk -f x-num.awk $<) > $@
num-%.txt: %-num.lst num-%.info
	(cat num-$*.info; awk -f num-x.awk $<) > $@

eng-num.txt: eng-num.lst eng-num.info
ell-num.txt: ell-num.lst ell-num.info

num-eng.txt: eng-num.lst num-eng.info
num-ell.txt: ell-num.lst num-ell.info

dictdir=@dictdir@
inputdictpath=@inputdictpath@

SUBDIRS=include utils lib makedict server client fonts

dict:
	makedict/makedict -cv -o$(dictdir) -d$(inputdictpath)

distcleancheck_listfiles = find . -type f -not -name 'bushu.pcf.gz' -print	

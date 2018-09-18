BEGIN {
    FS="\t"
    OFS="\t"
}
{ print }	
NF==3 {
    orig=$1
    gsub(/ά/,"α",$1)
    gsub(/έ/,"ε",$1)
    gsub(/ή/,"η",$1)
    gsub(/ί/,"ι",$1)
    if ($1 != orig) {
	print $0 "\t" orig
    }
}



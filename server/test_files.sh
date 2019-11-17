# Download the files by hand first

# Run your gfclient_download

# The next command outputs only the files which differ.
for i in $(ls courses/ud923/filecorpus) ; do md5sum courses/ud923/filecorpus/${i} ; md5sum fetched/${i} ; done |  uniq -u -w 32

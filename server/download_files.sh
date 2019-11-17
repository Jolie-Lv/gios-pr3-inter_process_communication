mkdir fetched/
cd fetched
for i in $(cat ../server/workload.txt) ; do wget https://s3.amazonaws.com/content.udacity-data.com${i} ; done
cd ..

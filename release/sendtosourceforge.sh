#!/bin/bash
# Puts the newest files into latest_stable, and deletes any files that are more than 7 days old.
trunk=$1

basename="openrave-r$revision"
cp $trunk "$basename-linux-src"
rm -rf "$basename-linux-src"/msvc_* # too big to include into openrave
tar cjf "$basename-linux-src.tar.bz2" "$basename-linux-src"
mkdir -p latest_stable
mv "$basename-linux-src.tar.bz2" latest_stable/
cp *.exe latest_stable/ # windows setup files
cp $trunk/release/README.rst .
tar cf latest_stable.tgz latest_stable README.rst
rm -rf "$basename-linux-src" latest_stable README.rst

ssh openravetesting,openrave@shell.sourceforge.net create # always create
scp latest_stable.tgz openravetesting,openrave@frs.sourceforge.net:/home/frs/project/o/op/openrave/
# remove files 7 or more days old
ssh openravetesting,openrave@shell.sourceforge.net "cd /home/frs/project/o/op/openrave; tar xf latest_stable.tgz; chmod -R g+w latest_stable; rm -f latest_stable.tgz; find latest_stable -mtime +30 -type f -exec rm -rf {} \;"
rm -f latest_stable.tgz

git tag -a "Latest Stable Tag (Tagged by Jenkins)." latest_stable
git push origin latest_stable

#fi
#ssh-keygen -t dsa -f ~/.ssh/id_dsa.openravetesting.sf -P "" -C "openravetesting@shell.sourceforge.net"

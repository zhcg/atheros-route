#! /bin/sh

INSTALL="./config/install-sh -c"

# check files
if [ ! -e pkg-slack.tgz ]
then
    echo "Cant find pkg-slack.tgz"
    exit 1
fi

# make stuff
make all
if [ $? != 0 ]
then
    echo "Make fail? Did you do configure?"
    exit 1
fi
# create dir
if [ -d pkg ]
then
    rm -rf pkg
fi
mkdir pkg
# install files
tar -xzf pkg-slack.tgz -C pkg

${INSTALL} -o root -g root -m 700 bin/routed pkg/usr/sbin/nx-routed
${INSTALL} -o root -g root -m 644 conf/routed.conf pkg/etc/routed.conf
${INSTALL} -o root -g root -m 644 routed.8 pkg/usr/man/man8/routed.8
${INSTALL} -o root -g root -m 644 routed.conf.5 pkg/usr/man/man5/routed.conf.5

${INSTALL} -o root -g root -m 644 ChangesLog pkg/usr/doc/nx-routed-0.99/ChangesLog
${INSTALL} -o root -g root -m 644 INSTALL pkg/usr/doc/nx-routed-0.99/INSTALL
${INSTALL} -o root -g root -m 644 LICENSE pkg/usr/doc/nx-routed-0.99/LICENSE
${INSTALL} -o root -g root -m 644 README pkg/usr/doc/nx-routed-0.99/README
${INSTALL} -o root -g root -m 644 TODO pkg/usr/doc/nx-routed-0.99/TODO

${INSTALL} -o root -g root -m 644 doc/rfc2453.html pkg/usr/doc/nx-routed-0.99/rfc2453.html
# pack
cd pkg
makepkg -c n nx-routed-0.99-i386.tgz
cd ..

echo "nx-routed-0.99-i386.tgz should be in pkg directory q:)"

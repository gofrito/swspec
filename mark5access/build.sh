aclocal
libtoolize --copy --force
autoconf
autoheader
automake -a -c

./configure --prefix=/usr
make
make install


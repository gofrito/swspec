aclocal -Im4
libtoolize --copy --force 
autoconf
autoheader
automake -a -c

./configure --prefix=/usr/local/
make
make install


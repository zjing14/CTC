#!/bin/sh
aclocal
autoheader
glibtoolize
automake --add-missing
autoconf

#!/bin/sh
#
# Generate dependencies from a list of source files

# Check to make sure our environment variables are set
if test x"$SOURCES" = x -o x"$output" = x; then
    echo "SOURCES, and output needs to be set"
    exit 1
fi
cache_prefix=".#$$"

generate_var()
{
    echo $1 | sed -e 's|^.*/||' -e 's|\.|_|g'
}

search_deps()
{
    base=`echo $1 | sed 's|/[^/]*$||'`
    grep '#include "' <$1 | sed -e 's|.*"\([^"]*\)".*|\1|' | \
    while read file
    do cache=${cache_prefix}_`generate_var $file`
       if test -f $cache; then
          : # We already ahve this cached
       else
           : >$cache
           for path in $base
           do dep="$path/$file"
              if test -f "$dep"; then
                 echo "	$dep \\" >>$cache
                 search_deps $dep >>$cache
                 break
              fi
           done
       fi
       cat $cache
    done
}

:>${output}.new
for src in $SOURCES
do  echo "Generating dependencies for $src"
    ext=`echo $src | sed 's|.*\.\(.*\)|\1|'`
    if test x"$ext" = x"rc"; then
        obj=`echo $src | sed "s|^.*/\([^ ]*\)\..*|\1.o|g"`
    else
        obj=`echo $src | sed "s|^.*/\([^ ]*\)\..*|\1.lo|g"`
    fi
    echo "\$(objects)/$obj: $src \\" >>${output}.new
    search_deps $src | sort | uniq >>${output}.new
    case $ext in
        c) cat >>${output}.new <<__EOF__

	\$(LIBTOOL) --mode=compile \$(CC) \$(CFLAGS) \$(EXTRA_CFLAGS) -c $src  -o \$@

__EOF__
        ;;
        cc) cat >>${output}.new <<__EOF__

	\$(LIBTOOL) --mode=compile \$(CC) \$(CFLAGS) \$(EXTRA_CFLAGS) -c $src  -o \$@

__EOF__
        ;;
        m) cat >>${output}.new <<__EOF__

	\$(LIBTOOL) --mode=compile \$(CC) \$(CFLAGS) \$(EXTRA_CFLAGS) -c $src  -o \$@

__EOF__
        ;;
        asm) cat >>${output}.new <<__EOF__

	\$(LIBTOOL) --tag=CC --mode=compile \$(auxdir)/strip_fPIC.sh \$(NASM) $src -o \$@

__EOF__
        ;;
        S) cat >>${output}.new <<__EOF__

	\$(LIBTOOL)  --mode=compile \$(CC) \$(CFLAGS) \$(EXTRA_CFLAGS) -c $src  -o \$@

__EOF__
        ;;
        rc) cat >>${output}.new <<__EOF__

	\$(WINDRES) $src \$@

__EOF__
        ;;
        *)   echo "Unknown file extension: $ext";;
    esac
    echo "" >>${output}.new
done
mv ${output}.new ${output}
rm -f ${cache_prefix}*

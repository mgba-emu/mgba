#!/bin/bash -eu

# Copyright 2017-2018 Glenn Randers-Pehrson
# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Revisions by Glenn Randers-Pehrson, 2017:
# 1. Build only the library, not the tools (changed "make -j$(nproc) all" to
#     "make -j$(nproc) libpng16.la").
# 2. Disabled WARNING and WRITE options in pnglibconf.dfa.
# 3. Build zlib alongside libpng
################################################################################

# Disable logging via pnglibconf.
cat scripts/pnglibconf.dfa | \
    sed -e "s/option STDIO/option STDIO disabled/" \
        -e "s/option WARNING /option WARNING disabled/" \
        -e "s/option WRITE enables WRITE_INT_FUNCTIONS/option WRITE disabled/" \
    > scripts/pnglibconf.dfa.temp
mv scripts/pnglibconf.dfa.temp scripts/pnglibconf.dfa

# Build the libpng library.
autoreconf -f -i
./configure --with-libpng-prefix=OSS_FUZZ_
make -j$(nproc) clean
make -j$(nproc) libpng16.la

for f in libpng_read_fuzzer \
         libpng_colormap_fuzzer \
         libpng_readapi_fuzzer \
         libpng_transformations_fuzzer;
do
    # Build the fuzzer.
    $CXX $CXXFLAGS -std=c++11 -I. \
         $SRC/libpng/contrib/oss-fuzz/${f}.cc \
         -o $OUT/${f} \
         -lFuzzingEngine .libs/libpng16.a -lz

    # Only libfuzzer can run the nalloc targets.
    if test "x$FUZZING_ENGINE" == 'xlibfuzzer'
    then

        if grep -q "nalloc_init" $SRC/libpng/contrib/oss-fuzz/${f}.cc
        then
            # Generate a wrapper that runs the fuzzer with NALLOC_FREQ=32
            # (allocation failures enabled).
            cat << EOF > $OUT/${f}@nalloc
#!/bin/bash
# LLVMFuzzerTestOneInput for fuzzer detection.
this_dir=\$(dirname "\$0")
NALLOC_FREQ=32 \$this_dir/${f} \$@
EOF
            chmod +x $OUT/${f}@nalloc
        fi

        # Add seed corpus.
        find $SRC/libpng -name "*.png" | \
            xargs zip $OUT/${f}_seed_corpus.zip

    fi
    cp $SRC/libpng/contrib/oss-fuzz/png.dict $OUT/${f}.dict
done

cp $SRC/libpng/contrib/oss-fuzz/*.dict \
   $SRC/libpng/contrib/oss-fuzz/*.options $OUT/

# end

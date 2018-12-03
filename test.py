# Unit test for the Python LZSS wrapper

# Copyright (C) 2013 Plastic Logic Limited
#
#     Guillaume Tucker <guillaume.tucker@plasticlogic.com>
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from __future__ import print_function

import os
import sys
import lzss
import tempfile

def test_buffer(data):
    data_lzss = lzss.encode(data)
    data_orig = lzss.decode(data_lzss)
    return data == data_orig

def test_file(data):
    tmp_in = tempfile.NamedTemporaryFile('wb', delete=False)
    tmp_lzss = tempfile.NamedTemporaryFile('wb', delete=False)
    tmp_orig = tempfile.NamedTemporaryFile('wb', delete=False)

    tmp_in.write(data)
    tmp_in.close()
    tmp_lzss.close()
    tmp_orig.close()

    lzss.encode_file(tmp_in.name, tmp_lzss.name)
    lzss.decode_file(tmp_lzss.name, tmp_orig.name)
    data_orig = open(tmp_orig.name, 'rb').read()

    os.unlink(tmp_in.name)
    os.unlink(tmp_lzss.name)
    os.unlink(tmp_orig.name)

    return data == data_orig

def main(argv):
    data = b"""\
Lorem ipsum dolor sit amet, consectetur
adipisicing elit, sed do eiusmod tempor
incididunt ut labore et dolore magna
aliqua. Ut enim ad minim veniam, quis
nostrud exercitation ullamco laboris nisi
ut aliquip ex ea commodo consequat.
Duis aute irure dolor in reprehenderit in
voluptate velit esse cillum dolore eu
fugiat nulla pariatur. Excepteur sint
occaecat cupidatat non proident, sunt in
culpa qui officia deserunt mollit anim id
est laborum."""

    result = True

    tests = { 'buffer': test_buffer, 'file': test_file }

    for name, f in tests.items():
        print("Test: {}... ".format(name), end='')
        if f(data) is False:
            result = False
            print("FAILED")
        else:
            print("PASSED")

    if result is True:
        print("All tests passed OK.")
    else:
        print("Some tests failed.")

    return result


if __name__ == '__main__':
    ret = main(sys.argv)
    sys.exit(0 if ret is True else 1)

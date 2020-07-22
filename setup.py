# Python package for LZSS encoding/decoding - setup.py
#
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

from setuptools import setup, Extension

setup(name="lzss",
      version='0.3',
      description="LZSS compression algorithm",
      author="Guillaume Tucker",
      author_email="guillaume.tucker@plasticlogic.com",
      maintainer="rumbah",
      maintainer_email="rumbah@users.noreply.github.com",
      url="https://github.com/rumbah/pylzss",
      license="GNU LGPL v3",
      platforms=["Windows", "Linux"],
      ext_modules=[Extension(
            "lzss",
            sources=["pylzss.c"],
            include_dirs=['./include'],
            language="C")],
      long_description="""\
A package for decoding / encoding LZSS-compressed data.""",

    classifiers=[
                "Programming Language :: Python",
                "Programming Language :: Python :: 3",
                "Programming Language :: Python :: 2",
                "Topic :: System :: Archiving :: Compression",
                "Development Status :: 3 - Alpha",
                "License :: OSI Approved :: GNU Lesser General Public License v3 (LGPLv3)",
    ])

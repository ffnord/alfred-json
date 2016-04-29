A.L.F.R.E.D - JSON Client
=========================

Compilation
-----------

alfred-json depends on:
 * libjansson
 * zlib
(on debian wheezy: `apt-get install zlib1g-dev libjansson-dev cmake`)

     $ apt-get install cmake libjansson-dev zlib1g-dev

Prepare the build using:

     $ mkdir build
     $ cd build/
     $ cmake ../

To compile, simply type:

     $ make

To install, use:

     $ make install

Usage
-----

First, an alfred daemon must be running. Data can be retrieved like this:

     $ alfred-json -r 64
    {
      "fe:f1:00:00:01:01": {
        "firmware": "0.3.2.1-1",
        "hostname": "alfredtest1"
      },
      "fe:f1:00:00:02:01": {
        "key": "value"
      }
    }

There are three output formats available: json, string (UTF-8) and binary. These can
be selected using the -f switch. "json" is default.

     $ alfred-json -r 64 -f string
    {
      "fe:f1:00:00:01:01": "{ \"hostname\": \"alfredtest1\", \"firmware\": \"0.3.2.1-1\" }\n",
      "fe:f1:00:00:02:01": "{ \"key\": \"value\" }\n"
    }

String output is wrapped in an JSON object. You'll get a warning if the string is 
invalid UTF-8.

JSON output tries to interprete the string as an JSON object. You'll get a warning
if in case the string does not represent a valid JSON object.

     $ alfred-json -r 64 -f json
    {
      "fe:f1:00:00:01:01": {
        "firmware": "0.3.2.1-1",
        "hostname": "alfredtest1"
      },
      "fe:f1:00:00:02:01": {
        "key": "value"
      }
    }

Binary output is meant to be parsed by other programs only. The format is:

      struct output {
        struct item items[n];
      };

      struct item {
        uint16_t id_len; // big-endian
        uint8_t id[id_len];

        uint16_t data_len; // big-endian
        uint8_t data[data_len];
      };

Transparent GZIP decompression
------------------------------

alfred-json will automatically and transparently decompress GZIP compressed
data when the -z option is given:

     $ echo Hello World | gzip | alfred -s 150
     $ alfred-json -z -r 150 -f string

License
-------

alfred-json is licensed under the terms of version 2 of the GNU General
Public License (GPL). Please see the LICENSE file.

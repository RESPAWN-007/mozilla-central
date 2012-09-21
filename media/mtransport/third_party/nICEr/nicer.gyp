# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# nrappkit.gyp
#
#
{
  'targets' : [
      {
          'target_name' : 'nicer',
          'type' : 'static_library',

          'include_dirs' : [
              ## EXTERNAL
              # nrappkit
	      '../nrappkit/src/event',
	      '../nrappkit/src/log',
              '../nrappkit/src/plugin',
	      '../nrappkit/src/registry',
	      '../nrappkit/src/share',
	      '../nrappkit/src/stats',
	      '../nrappkit/src/util',
	      '../nrappkit/src/util/libekr',

              # INTERNAL
              "./src/crypto",
              "./src/ice",
              "./src/net",
              "./src/stun",
              "./src/util",
          ],

          'sources' : [
                # Crypto
                "./src/crypto/nr_crypto.c",
                "./src/crypto/nr_crypto.h",
                #"./src/crypto/nr_crypto_openssl.c",
                #"./src/crypto/nr_crypto_openssl.h",

                # ICE
                "./src/ice/ice_candidate.c",
                "./src/ice/ice_candidate.h",
                "./src/ice/ice_candidate_pair.c",
                "./src/ice/ice_candidate_pair.h",
                "./src/ice/ice_codeword.h",
                "./src/ice/ice_component.c",
                "./src/ice/ice_component.h",
                "./src/ice/ice_ctx.c",
                "./src/ice/ice_ctx.h",
                "./src/ice/ice_handler.h",
                "./src/ice/ice_media_stream.c",
                "./src/ice/ice_media_stream.h",
                "./src/ice/ice_parser.c",
                "./src/ice/ice_peer_ctx.c",
                "./src/ice/ice_peer_ctx.h",
                "./src/ice/ice_reg.h",
                "./src/ice/ice_socket.c",
                "./src/ice/ice_socket.h",

                # Net
                "./src/net/nr_socket.c",
                "./src/net/nr_socket.h",
                #"./src/net/nr_socket_local.c",
                "./src/net/nr_socket_local.h",
                "./src/net/transport_addr.c",
                "./src/net/transport_addr.h",
                "./src/net/transport_addr_reg.c",
                "./src/net/transport_addr_reg.h",

                # STUN
                "./src/stun/addrs.c",
                "./src/stun/addrs.h",
                "./src/stun/nr_socket_turn.c",
                "./src/stun/nr_socket_turn.h",
                "./src/stun/stun.h",
                "./src/stun/stun_build.c",
                "./src/stun/stun_build.h",
                "./src/stun/stun_client_ctx.c",
                "./src/stun/stun_client_ctx.h",
                "./src/stun/stun_codec.c",
                "./src/stun/stun_codec.h",
                "./src/stun/stun_hint.c",
                "./src/stun/stun_hint.h",
                "./src/stun/stun_msg.c",
                "./src/stun/stun_msg.h",
                "./src/stun/stun_proc.c",
                "./src/stun/stun_proc.h",
                "./src/stun/stun_reg.h",
                "./src/stun/stun_server_ctx.c",
                "./src/stun/stun_server_ctx.h",
                "./src/stun/stun_util.c",
                "./src/stun/stun_util.h",
                "./src/stun/turn_client_ctx.c",
                "./src/stun/turn_client_ctx.h",

                # Util
                "./src/util/cb_args.c",
                "./src/util/cb_args.h",
                "./src/util/ice_util.c",
                "./src/util/ice_util.h",
                "./src/util/mbslen.c",
                "./src/util/mbslen.h",


          ],
          
          'defines' : [
              'SANITY_CHECKS',
              'USE_TURN',
              'USE_ICE',
              'USE_RFC_3489_BACKWARDS_COMPATIBLE',
              'USE_STUND_0_96',
              'USE_STUN_PEDANTIC',
              'USE_TURN',
              'NR_SOCKET_IS_VOID_PTR',
              'restrict=',
          ],
          
          'conditions' : [
              ## Mac
              [ 'OS == "mac"', {
                'cflags_mozilla': [
                    '-Werror',
                    '-Wall',
                    '-Wno-parentheses',
                    '-Wno-strict-prototypes',
                    '-Wmissing-prototypes',
                 ],
                 'defines' : [
                     'DARWIN',
                     'HAVE_LIBM=1',
                     'HAVE_STRDUP=1',
                     'HAVE_STRLCPY=1',
                     'HAVE_SYS_TIME_H=1',
                     'HAVE_VFPRINTF=1',
                     'NEW_STDIO'
                     'RETSIGTYPE=void',
                     'TIME_WITH_SYS_TIME_H=1',
                     '__UNUSED__="__attribute__((unused))"',
                 ],

		 'include_dirs': [
		     '../nrappkit/src/port/darwin/include'
		 ],
		 
		 'sources': [
		 ],
              }],
              
              ## Win
              [ 'OS == "win"', {
                'defines' : [
                    'WIN32',
                    'USE_ICE',
                    'USE_TURN',
                    'USE_RFC_3489_BACKWARDS_COMPATIBLE',
                    'USE_STUND_0_96',
                    'USE_STUN_PEDANTIC',
                    '_CRT_SECURE_NO_WARNINGS',
                    '__UNUSED__=',
                    'HAVE_STRDUP',
                    'SIZEOF_SHORT=2',
                    'SIZEOF_UNSIGNED_SHORT=2',
                    'SIZEOF_INT=4',
                    'SIZEOF_UNSIGNED_INT=4',
                    'SIZEOF_LONG=4',
                    'SIZEOF_UNSIGNED_LONG=4',
                    'SIZEOF_LONG_LONG=8',
                    'SIZEOF_UNSIGNED_LONG_LONG=8',
                    'NO_REG_RPC'
                ],

		 'include_dirs': [
		     '../nrappkit/src/port/win32/include'
		 ],
              }],

              
              ## Linux
              [ 'OS == "linux"', {
                'cflags': [
                    '-Werror',
                    '-Wall',
                    '-Wno-parentheses',
                    '-Wno-strict-prototypes',
                    '-Wmissing-prototypes',
                 ],
                 'defines' : [
                     'LINUX',
                     'HAVE_LIBM=1',
                     'HAVE_STRDUP=1',
                     'HAVE_STRLCPY=1',
                     'HAVE_SYS_TIME_H=1',
                     'HAVE_VFPRINTF=1',
                     'NEW_STDIO'
                     'RETSIGTYPE=void',
                     'TIME_WITH_SYS_TIME_H=1',
                     '__UNUSED__="__attribute__((unused))"',
                 ],

		 'include_dirs': [
		     '../nrappkit/src/port/linux/include'
		 ],
		 
		 'sources': [
		 ],
              }]
          ]
      }]
}


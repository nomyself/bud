# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'is_clang': 0,
    'gcc_version': 0,
    'openssl_no_asm%': 0,
    'llvm_version%': 0,
    'gas_version%': 0,
    'fips_dir%': 'false',
    'conditions': [
      ['fips_dir == "false"', {
        'fips_lib%': 'false',
      }, {
        'fips_lib%': '<(fips_dir)/lib/fipscanister.o',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'openssl',
      'type': '<(library)',
      'includes': ['openssl.gypi'],
      'sources': ['<@(openssl_sources)'],
      'sources/': [
        ['exclude', 'md2/.*$'],
        ['exclude', 'store/.*$']
      ],
      'conditions': [
        # FIPS
        # node-gyp - I am looking at you!
        ['fips_dir != "false" and fips_dir != "../../false"', {
          'defines': [
            'OPENSSL_FIPS',
          ],
          'sources': [
            '<(fips_lib)',
          ],
          'include_dirs': [
            '<(fips_dir)/include',
          ],

          # Trick fipsld
          'product_name': 'crypto',
        }],

        ['openssl_no_asm!=0', {
          # Disable asm
          'defines': [
            'OPENSSL_NO_ASM',
          ],
          'sources': ['<@(openssl_sources_no_asm)'],
        }, {
          # "else if" was supported in https://codereview.chromium.org/601353002
          'conditions': [
            ['target_arch=="arm"', {
              'defines': ['<@(openssl_defines_asm)'],
              'sources': ['<@(openssl_sources_arm_void_gas)'],
            }, 'target_arch=="ia32" and OS=="mac"', {
              'defines': [
                '<@(openssl_defines_asm)',
                '<@(openssl_defines_ia32_mac)',
              ],
              'sources': ['<@(openssl_sources_ia32_mac_gas)'],
            }, 'target_arch=="ia32" and OS=="win"', {
              'defines': [
                '<@(openssl_defines_asm)',
                '<@(openssl_defines_ia32_win)',
              ],
              'sources': ['<@(openssl_sources_ia32_win_masm)'],
            }, 'target_arch=="ia32"', {
              # Linux or others
              'defines': [
                '<@(openssl_defines_asm)',
                '<@(openssl_defines_ia32_elf)',
              ],
              'sources': ['<@(openssl_sources_ia32_elf_gas)'],
            }, 'target_arch=="x64" and OS=="mac"', {
              'defines': [
                '<@(openssl_defines_asm)',
                '<@(openssl_defines_x64_mac)',
              ],
              'sources': ['<@(openssl_sources_x64_mac_gas)'],
            }, 'target_arch=="x64" and OS=="win"', {
              'defines': [
                '<@(openssl_defines_asm)',
                '<@(openssl_defines_x64_win)',
              ],
              'sources': ['<@(openssl_sources_x64_win_masm)'],
            }, 'target_arch=="x64"', {
              # Linux or others
              'defines': [
                '<@(openssl_defines_asm)',
                '<@(openssl_defines_x64_elf)',
              ],
              'sources': ['<@(openssl_sources_x64_elf_gas)'],
            }, 'target_arch=="arm64"', {
              'defines': ['<@(openssl_defines_arm64)',],
              'sources': ['<@(openssl_sources_arm64_linux64_gas)'],
            }, {
              # Other architectures don't use assembly.
              'defines': ['OPENSSL_NO_ASM'],
              'sources': ['<@(openssl_sources_no_asm)'],
            }],
          ],
        }], # end of conditions of openssl_no_asm
        ['OS=="win"', {
          'defines' : ['<@(openssl_defines_all_win)'],
          'includes': ['masm_compile.gypi',],
        }, {
          'defines' : ['<@(openssl_defines_all_non_win)']
        }]
      ],
      'include_dirs': ['<@(openssl_include_dirs)'],
      'direct_dependent_settings': {
        'include_dirs': [
          'openssl/include'
        ],
      },
    },
    {
      # openssl-cli target
      'includes': ['openssl-cli.gypi',],
    }
  ],
  'target_defaults': {
    'includes': ['openssl.gypi'],
    'include_dirs': ['<@(openssl_default_include_dirs)'],
    'defines': ['<@(openssl_default_defines_all)'],
    'conditions': [
      ['OS=="win"', {
        'defines': ['<@(openssl_default_defines_win)'],
        'link_settings': {
          'libraries': ['<@(openssl_default_libraries_win)'],
        },
      }, {
        'defines': ['<@(openssl_default_defines_not_win)'],
        'cflags': ['-Wno-missing-field-initializers'],
        'conditions': [
          ['OS=="mac"', {
            'defines': ['<@(openssl_default_defines_mac)'],
          }, {
            'defines': ['<@(openssl_default_defines_linux_others)'],
          }],
        ]
      }],
      ['is_clang==1 or gcc_version>=43', {
        'cflags': ['-Wno-old-style-declaration'],
      }],
      ['OS=="solaris"', {
        'defines': ['__EXTENSIONS__'],
      }],
    ],
  },
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

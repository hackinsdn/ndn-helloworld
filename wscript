# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

import os
from waflib import Utils

VERSION = '0.1'
APPNAME = 'ndn-helloworld'

def options(opt):
    opt.load(['compiler_cxx', 'gnu_dirs'])
    opt.load(['default-compiler-flags', 'boost'],
             tooldir=['.waf-tools'])

def configure(conf):
    conf.load(['compiler_cxx', 'gnu_dirs',
               'default-compiler-flags', 'boost'])

    # Prefer pkgconf if it's installed, because it gives more correct results
    # on Fedora/CentOS/RHEL/etc. See https://bugzilla.redhat.com/show_bug.cgi?id=1953348
    # Store the result in env.PKGCONFIG, which is the variable used inside check_cfg()
    conf.find_program(['pkgconf', 'pkg-config'], var='PKGCONFIG')

    pkg_config_path = os.environ.get('PKG_CONFIG_PATH', f'{conf.env.LIBDIR}/pkgconfig')
    conf.check_cfg(package='libndn-cxx', args=['libndn-cxx >= 0.8.1', '--cflags', '--libs'],
                   uselib_store='NDN_CXX', pkg_config_path=pkg_config_path)

    conf.check_boost(lib='date_time program_options', mt=True)

    conf.check_compiler_flags()

def build(bld):
    bld.program(target='ndn-helloworld-client',
                source='src/ndn-helloworld-client.cpp',
                use='NDN_CXX BOOST')

    bld.program(target='ndn-helloworld-server',
                source='src/ndn-helloworld-server.cpp',
                use='NDN_CXX BOOST')

def dist(ctx):
    ctx.algo = 'tar.xz'

def distcheck(ctx):
    ctx.algo = 'tar.xz'

# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('ecs-clustering', ['core', 'stats', 'aodv', 'internet', 'mobility', 'wifi'])
    module.source = [
        'model/ecs-clustering.cc',
        'model/nsutil.cc',
        'model/table.cc',
        'model/logging.cc',
        'model/ecs-stats.cc',
        'helper/ecs-clustering-helper.cc',
        'model/proto/messages.proto'
        ]
    module.cxxflags = ['-I./contrib/ecs-clustering/model']

    module_test = bld.create_ns3_module_test_library('ecs-clustering')
    module_test.source = [
        'test/ecs-clustering-test-suite.cc',
        ]
    # Tests encapsulating example programs should be listed here
    if (bld.env['ENABLE_EXAMPLES']):
        module_test.source.extend([
        #    'test/ecs-clustering-examples-test-suite.cc',
             ])

    headers = bld(features='ns3header')
    headers.module = 'ecs-clustering'
    headers.source = [
        'model/ecs-clustering.h',
        'model/table.h',
        'model/nsutil.h',
        'model/util.h',
        'model/logging.h',
        'model/ecs-stats.h',
        'helper/ecs-clustering-helper.h'
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

##
# This code is taken from:
# https://stackoverflow.com/questions/11274070/integrate-protocol-buffers-into-waf
###
from waflib.Task import Task
from waflib.TaskGen import extension

"""
A simple tool to integrate protocol buffers into your build system.

    def configure(conf):
        conf.load('compiler_cxx cxx protoc_cxx')

    def build(bld):
    bld.program(source = "main.cpp file1.proto proto/file2.proto",
            target = "executable")

"""

class protoc(Task):
    run_str = '${PROTOC} ${SRC} --cpp_out=. -I..'  # set this to ../.. when running with sem and to .. when running normally
    color = 'BLUE'
    ext_out = ['pb.h', 'pb.cc']

@extension('.proto')
def process_protoc(self, node):
    cpp_node = node.change_ext('.pb.cc')
    hpp_node = node.change_ext('.pb.h')
    self.create_task('protoc', node, [cpp_node, hpp_node])
    self.source.append(cpp_node)
    self.env.append_value('INCLUDES', ['.'] )

    self.use = self.to_list(getattr(self, 'use', '')) + ['PROTOBUF']

def configure(conf):
    conf.check_cfg(package="protobuf", uselib_store="PROTOBUF",
            args=['protobuf >= 3.0.0' '--cflags', '--libs'])
    conf.find_program('protoc', var='PROTOC')

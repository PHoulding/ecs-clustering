# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('ecs-clustering', ['core'])
    module.source = [
        'model/ecs-clustering.cc',
        'helper/ecs-clustering-helper.cc',
        ]

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
        'helper/ecs-clustering-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()


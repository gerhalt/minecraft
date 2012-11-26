#!/usr/bin/python
'''

Building the C extension

'''

from distutils.core import setup, Extension

setup (name = 'Minecraft',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [
            Extension("minecraft", sources = ["minecraft.c", "chunk.c", "world.c"])
       ])

